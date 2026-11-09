# EUpdate in more detail

What the [README](../README.md) leaves out: how EUpdate talks to GitHub, building and releasing,
and what each test checks. How to use it is in the user guide, [GUIDE.md](GUIDE.md) (`EUPDATE.HYP`).

## What it does, in full

- **The AERA programs, ready to follow:** on its first start EUpdate follows ECal, EContact,
  EMail, EFTP, EHypView, ClaudeST, EWeb, EPDF and itself (`DEFAULTS.INF`), each into `C:\AERA\`, the
  desk accessories to `C:\`; a Check shows what is new, and Update brings them in.
- **Find on GitHub:** repositories by name, the most starred first; or owners, people and
  organisations, and then their repositories.
- **Follow** a repository into a folder: what you have, the latest release GitHub has, and
  whether you are current, at a glance (new releases in AERA's orange, current ones in teal).
- **Update** one, several or all of them; **Check** without downloading; **Download again**.
- **On a schedule:** a check when EUpdate starts, and every few hours while it runs; downloading
  what is new, or only saying so. Iconified, the window says how many are new.
- **Every file where it belongs:** a release's zip unpacked into the folder; rules for the files
  that go elsewhere (desk accessories to the boot drive's root) or nowhere (the ST edition, on a
  Falcon), suggested from what the release has; long names made 8+3 for TOS; every file written
  safely, so a reset never leaves half a program. An AERA program's `TLS.OVL` always goes with it,
  and a hand-copied ST edition (`ECALST.ACC`) gives way to the one program that replaces it.
- **TLS on the Atari itself:** TLS 1.2 with BearSSL, the handshake's sums on the Falcon's DSP; no
  gateway. It is the family's `TLS.OVL`, beside the program, loaded when EUpdate first talks to GitHub. A GitHub token is optional, kept encrypted, and goes nowhere but GitHub's API.
- **Modern but Atari:** one window with a toolbar and tooltips, right-click menus, the family's
  Settings dialog, VA_START and drag and drop (drop a repository's address on the window),
  iconify, help in EHypView or ST-Guide.
- **Light:** about 220 KB, and 140 KB more for `TLS.OVL` once it talks to GitHub; built so that
  only the shared AERA code it calls goes in.

## How it reaches GitHub

EUpdate asks GitHub's REST API itself: the latest release
(`/repos/OWNER/NAME/releases/latest`), searches (`/search/repositories`, `/search/users`), an
owner's repositories (`/users/LOGIN/repos`), and a release's files (`/releases/assets/ID`, which
GitHub redirects to its file servers). Answers come gzip'ed; the connections stay open between
questions, as a handshake costs a Falcon a second or more. No gateway is needed. For a GitHub
Enterprise server, `APIBASE=` in `EUPDATE.INF`.

## Building and releasing

```sh
sudo apt install gcc-m68k-linux-gnu python3   # Debian/Ubuntu; no MiNTLib needed
make            # binary/eupdate.prg, tls.ovl (common's), cacert.pem, roots.dat
make test       # the core on this computer, with AddressSanitizer and UBSan
make itest      # eupd-cli against a GitHub of the tests' own (tests/server.py), HTTP and TLS
make fuzz       # the parsers under libFuzzer (clang)
make dist       # dist/eupdate-x.y.zip, as on the Releases page
make help HCP=/path/to/hcp   # EUPDATE.HYP from docs/GUIDE.md (common/tools/build-hcp.sh builds hcp)
```

`binary/` and `dist/` are what make builds; they are never committed, and a build changes no tracked
file. A release: `tools/bump.py`, commit and push, then **Actions > Release > Run workflow** on `main`
with what is new. The workflow tests, builds, tags the commit `vX.Y` and publishes the files and the
zip on the Releases page. Builds in between are never handed out: to try a change, release it.

## Tests

- `make test`: the JSON reader (escapes, UTF-8 and Hebrew to the Atari's characters, broken and
  too-deep text), HTTP against answers `common/tests/fuzz/fuzz_net.c` plays (chunked, gzip,
  redirects, broken lengths, endless lines), GitHub's answers (a release, no release, no
  repository, the hour's limit, searches), zips (stored, deflated, a damaged CRC, a name that
  climbs out of the folder), the rules and their suggestions, 8+3 names, and `EUPDATE.INF`
  written and read back with the token encrypted (an empty line after every setting, and 1.1's
  file without them read too); then the fuzz targets' seeds and finds.
- `make itest`: `eupd-cli` against `tests/server.py`, over HTTP and TLS with a certificate
  authority of the test's own: every question EUpdate asks, a wrong token, a download unpacked
  into a folder, and the token never sent on to the file's server nor written in the log.
- `tests/hatari/gui_test.py WORK [MACHINE]`: `EUPDATE.PRG` on the family's emulated Falcon
  (`common/tools/machines.py`), through FAKESTNG to `tests/server.py`: the check at the start,
  Find, an owner's repositories, Follow (a letter typed into the middle of its folder), Update
  (unpacked, a rule obeyed, `EUPDATE.INF` written), Get info, a 93-character token typed into
  Settings and reaching the server whole, and the About box. `tests/hatari/screenshots.py` makes the pictures in
  `docs/`.
- `tests/hatari/st_test.py WORK`: `EUPDATE.PRG` started on an ST (`ST:mono`) says it needs a Falcon
  or a TT, and Quit brings the desktop back (common's `cpucheck.c`, the program's first code, is built for
  the 68000).
- `tests/hatari/appdir_test.py WORK`: EUpdate started by another program (common's
  `tests/appdir` launcher, from a real FAT floppy) and as from the desktop finds its own files in
  its own folder, not the current one (common's `app_dir()`; TOS 1.04 and 4.04 start a program
  in the desktop's folder).
- `tests/hatari/noovl_test.py WORK`: with no `TLS.OVL` beside `EUPDATE.PRG`, the check at the
  start says so in an alert and in `EUPDATE.LOG`, asks GitHub nothing, and EUpdate goes on.
- `tests/hatari/family_test.py WORK`: as on Erez's Falcon, EUpdate on its own: stand-in releases
  of the AERA family (`tests/github/standin_family.py`: each app's file names, sizes like the real
  ones', made-up bytes), served by `tests/family_server.py` as GitHub serves them (a certificate
  chain like GitHub's, the files from a second host at signed addresses, idle connections closed),
  at a NetUSBee's pace, with the suggested rules on (the ST editions left out, the accessories into
  `C:\`). No other AERA program is fetched or loaded (Erez, 8 Nov 2026): the real ones together are
  the hub's family checks. Under Erez's TOS 4.04 and NVDI where the machine has them
  (`common/tools/machines.py`), EmuTOS otherwise.
