# EUpdate

<img src="docs/icon16.png" width="96" align="right" alt="EUpdate icon: an Atari SM124-style monitor showing an arrow coming down into a tray, and an orange spot for what is new">

The latest releases of GitHub repositories, straight to the Atari Falcon and TT: a GEM program,
written in C, that finds repositories and owners on GitHub, follows the ones you choose into
folders of your choice, shows which are current, and updates them with a click or on a schedule.

**Highlights**

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
  safely, so a reset never leaves half a program.
- **TLS on the Atari itself:** TLS 1.2 with BearSSL, the handshake's sums on the Falcon's DSP; no
  gateway. A GitHub token is optional, kept encrypted, and goes nowhere but GitHub's API.
- **Modern but Atari:** one window with a toolbar and tooltips, right-click menus, the family's
  Settings dialog, VA_START and drag and drop (drop a repository's address on the window),
  iconify, help in EHypView or ST-Guide.
- **Light:** about 300 KB, a third of it BearSSL; built so that only the shared AERA code it calls
  goes in.

![EUpdate on a Falcon: the AERA programs followed above, one with a new release, one not
downloaded yet; whomper's repositories found below, ticks by those followed](docs/screenshot.png)

## Getting started

Download what you need from the **[latest release](https://github.com/whomper/EUpdate/releases/latest)**,
file by file or all together in the zip:

| File | What it is |
|---|---|
| `EUPDATE.PRG` | the program, for a Falcon or TT (68030; a 68020 or better) |
| `CACERT.PEM`, `ROOTS.DAT` | the root certificates GitHub's are checked against |
| `EUPDATE.HYP` | the user guide, for EHypView or ST-Guide; the Help key opens it |
| `EUPDATE.RSC`, `EUPDATE.ICN`, `EUPDMK.ICN` | the desktop icon (in the zip's `icons/` folder) |
| `EUPDATE-x.y.zip` | all of these together |

1. Copy `EUPDATE.PRG`, `CACERT.PEM`, `ROOTS.DAT` and `EUPDATE.HYP` into a folder of their own.
   EUpdate keeps its settings and the repositories it follows (`EUPDATE.INF`, `EUPDATE.KEY`) in
   the folder it is started from.
2. Load STinG (or run MiNT with MiNTnet).
3. Start EUpdate, choose **File > Find on GitHub** (^F), and double-click a repository to follow
   it. Press **Help** for the guide.

EUpdate can keep itself up to date: follow `whomper/EUpdate` into its own folder.

**Needs:** a Falcon or TT; TOS 3 or 4, EmuTOS, MagiC or MiNT; 640×400 or larger; about 500 KB free,
and room for the biggest file you download; STinG or MiNTnet.

**Under TOS, put Atari's `FOLDR100.PRG` first in `AUTO`**, renamed for more room (`FOLDR400.PRG`).
TOS keeps an entry in a small internal pool for every folder a program has looked into, and halts
with "OUT OF INTERNAL MEMORY" when it runs out; FOLDRxxx makes the pool bigger.

**With STinG, set the Delay in `STING.CPX` to 10** on a Falcon or TT, as STinG's manual says.
STinG looks at the network card that often, so it sets the speed of every STinG program: on
Erez's Falcon with a NetUSBee, Delay 10 took EFTP's downloads from 40 to 75 KB/s.

**GitHub answers 60 questions an hour** from an address without a token (a check asks one a
repository); with a token, 5,000. Make one with no rights at all on github.com (Settings >
Developer settings > Personal access tokens) and type or paste it in EUpdate's **Settings >
GitHub**: the field takes a fine-grained token's 93 characters, as every field takes what its
setting holds and scrolls.

**If it crashes,** `CRASH.LOG` in its folder says where; when TOS shows the bombs, start EUpdate
again and it writes TOS's record of them there. **Options > Protocol log** writes each step of a
job to `EUPDATE.LOG` as it happens (the token never).

## How it reaches GitHub

```
Falcon, TT:  Atari (EUpdate, TLS 1.2, the DSP) ---TLS---> api.github.com
                                               ---TLS---> where GitHub sends the files
```

EUpdate asks GitHub's REST API itself: the latest release
(`/repos/OWNER/NAME/releases/latest`), searches (`/search/repositories`, `/search/users`), an
owner's repositories (`/users/LOGIN/repos`), and a release's files (`/releases/assets/ID`, which
GitHub redirects to its file servers). Answers come gzip'ed; the connections stay open between
questions, as a handshake costs a Falcon a second or more. No gateway is needed. For a GitHub
Enterprise server, `APIBASE=` in `EUPDATE.INF`.

## Building

```sh
sudo apt install gcc-m68k-linux-gnu python3   # Debian/Ubuntu; no MiNTLib needed
make            # binary/EUPDATE.PRG, CACERT.PEM, ROOTS.DAT
make test       # the core on this computer, with AddressSanitizer and UBSan
make itest      # eupd-cli against a GitHub of the tests' own (tests/server.py), HTTP and TLS
make fuzz       # the parsers under libFuzzer (clang)
make dist       # dist/EUPDATE-x.y.zip, as on the Releases page
make help HCP=/path/to/hcp   # EUPDATE.HYP from docs/GUIDE.md (common/tools/build-hcp.sh builds hcp)
```

`binary/` and `dist/` are what make builds; they are never committed, and a build changes no tracked
file. A release: `tools/bump.py`, commit and push, then **Actions > Release > Run workflow** on `main`
with what is new. The workflow tests, builds, tags the commit `vX.Y` and publishes the files and the
zip on the Releases page. Builds in between are never handed out: to try a change, release it.

## Source

```
src/        the core, also built for this computer in the tests: HTTP (http.c), a JSON reader
            (json.c), GitHub's API (gh.c), EUPDATE.INF (store.c), zips (unzip.c), checking and
            updating a repository, its rules and its names (update.c)
ui/         GEM: the window and its two lists, dialogs, menus, the jobs and the schedule
atari/      files and time on GEMDOS (plat_tos.c)
common/     the code the AERA programs share (git subtree of AERA-Atari): the runtime, TLS and
            connections, the DSP, inflate, charsets, the GEM building blocks
tests/      unit.c, integration.py with server.py, the fuzz targets, the Hatari rigs (tests/hatari)
tools/      the icon, the help file, the version bump, common/'s update
docs/       the user guide (GUIDE.md, the source of EUPDATE.HYP) and pictures
```

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
- `tests/hatari/family_test.py WORK`: as on Erez's Falcon: the AERA family updated from their real
  latest releases (`tests/github/fetch_family.py`, served by `tests/family_server.py` as GitHub
  serves them: a certificate chain like GitHub's, the files from a second host at signed addresses,
  idle connections closed), at a NetUSBee's pace, with ECal, EContact and EHypView loaded as
  accessories and the suggested rules on (the ST editions left out, the accessories into `C:\`);
  `AERA_WITH_ACCS=` (empty) runs it with none. Under Erez's TOS 4.04 and NVDI where the machine
  has them (`common/tools/machines.py`), EmuTOS otherwise.

## Licence

EUpdate is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 2 of the License, or (at your option)
any later version. It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [COPYING](COPYING).

Copyright (C) 2026 AERA | Erez Yaary.

The AERA name and logos are not covered by the GPL (see
[AERA-Atari](https://github.com/whomper/AERA-Atari#licence)). GitHub is a trademark of GitHub, Inc.;
EUpdate is not affiliated with it.

It includes, under their own licences (compatible with the GPL):

- [BearSSL](common/third_party/bearssl) by Thomas Pornin: MIT licence
  ([LICENSE.txt](common/third_party/bearssl/LICENSE.txt))
- `CACERT.PEM`, the root certificates, from Mozilla's CA list (Mozilla Public License 2.0)

---

Part of **AERA** for Atari: [ECal](https://github.com/whomper/ECal) · [EContact](https://github.com/whomper/EContact) · [EMail](https://github.com/whomper/EMail) · [EFTP](https://github.com/whomper/EFTP) · [EHypView](https://github.com/whomper/EHypView) · [ClaudeST](https://github.com/whomper/ClaudeST) · [EWeb](https://github.com/whomper/EWeb) · [EUpdate](https://github.com/whomper/EUpdate) · 8-bit: [EDOS](https://github.com/whomper/EDOS) · [EFTP_XL](https://github.com/whomper/EFTP_XL) · Pi: [AERA-Gateway](https://github.com/whomper/AERA-Gateway)
