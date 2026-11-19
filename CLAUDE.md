# Notes for working on EUpdate

EUpdate is one of the AERA Atari apps (ECal, EContact, EMail, EFTP, EHypView, ClaudeST, EWeb, EPDF). They are meant to
look and behave as one family.

## For every AERA app

These come from AERA-Atari's repo template and are the same in every app's CLAUDE.md.

- **Follow the AERA Atari app guidelines** ([ATARI-APP-GUIDELINES.md](https://github.com/whomper/AERA-Atari/blob/main/standards/ATARI-APP-GUIDELINES.md)). Pay most
  attention to:
  - About box
  - family shortcuts
  - PRG/ACC rules
  - context menus
  - VA_START, help, alerts
  - INF location

  When a change touches one of these, do it the guideline's way, even if the old code here did it
  differently.
- **Speed is set by the stock machines** (Erez, 26 Oct 2026; the guidelines' "Speed"): an 8 MHz ST
  and a stock Falcon (`ST`, `ST-1`, `Falcon-4`), not the DFB1. Design and code for them: draw only
  what changed, keep the 68000's slow operations out of inner loops, do work once. A change to
  drawing, a loop over the data or the start gives its `AESGAP` stretches on `ST` and `Falcon-4`,
  before and after, in its commit message. A GEM release passes the speed rig within the
  guidelines' times.
- **Shared code lives in [AERA-Atari](https://github.com/whomper/AERA-Atari).** Files under `common/` come from it. Change them
  there, not here, and pull them in with `tools/sync-common.sh`. A fix made only in this repo will drift
  again.
- **Build and test before every push:** `make` with no warnings and `make test`, plus this repo's
  own checks (below): `make itest` when network code changed, the Hatari rigs in `tests/hatari/` when
  the GEM side changed.
- **Versions and releases:** `VERSION` changes only in a release commit. A release: `tools/bump.py`,
  commit, push, then run the Release workflow on `main` with the notes (or push a tag `vX.Y` made with
  `git tag -a`); it tests, builds, tags vX.Y and publishes each file and the zip as a GitHub Release.
  A build must leave every tracked file unchanged and shows the version in `VERSION` as it is (there is
  no `-dev` label). Don't
  commit built programs, help files or certificates (`binary/`, `dist/`). Never reuse a number.
- **Only formal releases leave the build machine.** Erez gets vX.Y releases made by the Release workflow from
  `main`, nothing else: no test builds, no CI artifacts, no builds from a branch.
  Work lands on `main`; to try a fix on real hardware, release it. If a release doesn't work, roll back
  with a release: revert on `main`, release the next number, and run the Withdraw workflow
  (`.github/workflows/withdraw.yml`) on the bad one: it puts "Withdrawn: use vX.Y" at the top of its notes.
- **Releases go out without asking Erez** (his decision, 23 Oct 2026; guidelines 1.2), through
  **the regression gate** (Erez, 17 Nov 2026: "we need to be extra careful with regression issues
  as there are users of our software. This means testing across all configurations" and "make sure
  we don't loose performance"; guidelines 1.2, AERA-Atari a22574e). Every code candidate, in order:
  1. this repo's checks;
  2. the rigs on every row of `standards/TEST-MATRIX.md`'s gate table EUpdate runs on, on Erez's TOS
     images and NVDI (EmuTOS is no evidence for a release), each row named by `describe()`, a row
     that can't run said with why. EUpdate's: Erez's configuration; `Falcon-4:16` without NVDI;
     `Falcon:65536`; TT; MiNT + XaAES (plain, ct60, ext2); a 68060; the FireBee stand-in; on an ST
     only its alert (`st_test.py`: cpucheck.c is its only 68000 code); the network path it has
     (Direct, TLS.OVL and tlsfb.ovl, through STinG and MiNTnet); one boot with NatFeats off. No
     accessory, no Gateway;
  3. the paired speed run against the last release (`make prof`, at the shipped layout by
     common's `profpad.py`) on Erez's configuration and `Falcon-4:16`: no row slower beyond a tick
     on either; fix it first, and only Erez accepts one;
  4. the candidate commit and the results to the hub, who runs the family check (the real programs
     together) and answers go or the faults;
  5. only then the release.

  A fault a user reports becomes a case in the rigs, in the release that fixes it. Then give Erez the link in one line, with anything he has to do, and send the hub the link. Roll
  back a bad release the same way. Decisions that are his still go to him first.
- **Who decides what** (Erez, 31 Oct 2026; the guidelines' "Who decides what"). Erez's part, put
  to him before the work as a decision with a short context:
  - new programs and ideas, their core capabilities, and how they work with the other programs;
  - the user experience: the look, the layout, the way through a task, the words, the icons, the
    defaults;
  - phases: work stays inside the phase he approved; at its end, one phase report (what it set out
    to do, what was done with the release links, what is left, the next phase as a proposal), and
    the next phase starts when he says so;
  - priorities and conflicts between programs or sessions (through the hub, with its options and
    recommendation);
  - packages on the Pi and ports.

  How it is made is the team's, without asking: code, structure, shared code, tests, measurements,
  fixes, docs and releases inside an approved phase.
- **No test uses another AERA program or accessory** (Erez, 8 Nov 2026; the guidelines 1.2): EUpdate's
  tests run it alone, with nothing of the family loaded beside it, started by it or fetched for it
  (no `getrelease.py`). What it does with the others' releases is tested against stand-ins this repo
  makes; the real programs together are the hub's family checks.
- **Every new source file starts with the SPDX line and copyright** (see any `.c` file); a generator
  writes them into the files it makes.
- **The name** reads "AERA | Erez Yaary" wherever it is given: copyright lines
  (`Copyright (C) 2026 AERA | Erez Yaary`), the About box, the help file's `@author`, the docs, and as
  the author of every commit: `git commit --author="AERA | Erez Yaary <yaary@me.com>"`, with Claude as
  the signing committer, so GitHub shows the commits as Verified (AERA-Atari,
  `standards/GITHUB-IDENTITY.md`).
- **Never commit:**
  - real addresses from the owner's network: use `192.168.1.x`, `PI_IP`, `ATARI_IP`
  - passwords, session keys, `.KEY` / `.INF` files from a test run
  - photos that still carry EXIF/GPS data
- **README:** keep the section order from the template (Highlights, Getting started, How it reaches the
  servers, Building, Source, Tests, Licence, family footer). Use British English.

## Talking with Erez

His rule for the whole family (18 Oct 2026): no progress or in-between messages; write to him
only when he has to decide something, with a short context and the decision. Findings: the
information, your assessment and what he should do, nothing more.

## Maintenance mode (Erez, 1 Nov 2026; what it means since 17 Nov 2026)

EUpdate is in maintenance mode while EWeb, EPDF and EPlay go to 1.0 (AERA-Atari,
ATARI-APP-GUIDELINES.md, section 8, as of 8ddb463). Until Erez ends it:
- No new features and no speed rounds of its own.
- **Every family rule holds for it as for the focus apps** (Erez, 17 Nov 2026: "all applications
  must adhere to family rules and be notified. We will review whether they need to issue a
  release." and "Applications on maintenance does not mean they contain faults"). The hub
  notifies EUpdate of each new or changed rule and each shared fix; EUpdate adopts it, with the
  shared code it needs (a sync of `common/` with `tools/sync-common.sh`), and its row in the
  guidelines' table is ticked. Its ✗, ~ and ? there are work for EUpdate.
- Whether such a change needs a release is reviewed with Erez through the hub. A fault fix always
  gets one, as do a security or data-loss fault, a crash or breakage, something outside that stops
  it working (a certificate, GitHub's API), and something EWeb, EPDF or EPlay need from it. The
  release notes say why.
- Work on `main` that isn't released stays unreleased until that review.
- Nothing runs in the background of its own accord; the session waits for Erez or the hub.
- Only Erez's explicit word ends this mode; the hub passes it on quoting him.
- Lifted for one speed fix (Erez, 19 Nov 2026, on the About box at 210 ms against its 150 on
  `Falcon-4:256` with NVDI: "Fix it"): the About box within its time on his configuration, with no
  other row slower beyond a tick; done in 2.11 (130 ms; the close 50 -> 120 accepted by Erez,
  "Accept"), and maintenance is back on.
- Speed: a change ships only if no row of the paired run (`make prof`, padded to the shipped
  places by common's `profpad.py`) is slower by more than one tick (AERA-Atari 098183d); a
  change that moves code gets a pad round first (`tools/objpad.py`, `atari/pads.txt` against the
  last release's map), and a row still slower goes to Erez through the hub.

## EUpdate

- **What it is:** `EUPDATE.PRG`, a program only (no accessory: its work is TLS to GitHub, which an
  accessory may not do in the background, guidelines 2.2), for the Falcon and TT only: 68030
  code, TLS on the Atari in the family's `TLS.OVL` (Erez, 27 Oct 2026; guidelines 7.4:
  `atari/tls_ovl.c`, `src/ovl.c`, `ui/dsp_ovl.c`; RSA and the curves on the DSP), loaded at the
  first job (`job_begin`), its error in an alert; the host builds keep `src/tls.c`. There is no
  ST edition and no gateway: `opt.falcon` is always 1, kept only because `dsp_ovl.c` reads it.
- **Light (Erez's wish):** every file is built with `-ffunction-sections -fdata-sections` and
  linked with `--gc-sections`, so of `common/` only what EUpdate calls is in the program. Don't
  add code nothing calls, nor settings nobody asked for; take what `common/` has before writing
  your own. Results of searches live in the heap (TT RAM), not the BSS.
- **The core** is `src/` and builds on this computer too (`tests/host/plat_posix.c`):
  `http.c` (GET, redirects, chunked, gzip, two kept connections; the token only to the API's
  host and never past a redirect), `json.c` (a pull reader: nothing built in memory), `gh.c`
  (GitHub's REST API), `store.c` (`EUPDATE.INF`: `[OPTIONS]`, then a `[REPO]` part each, the
  token encrypted with `EUPDATE.KEY`), `unzip.c` (stored and deflated, CRC checked, on common's
  `inflate.c`), `update.c` (a check, the files chosen, the **rules** that send files elsewhere or
  leave them out and their suggestions, 8+3 names, safe writes through `pf_save`).
- **The GEM side** is `ui/`: `win.c` (EFTP's window: toolbar, two panes, divider, size box),
  `lists.c` (Following and Find), `dialogs.c` (EFTP's builder and form loop: Find, Follow/Get
  info, Settings, Keys), `main.c` (menus, jobs, the schedule, the event loop), and EFTP's
  `draw.c`, `font.c`, `theme.c`. Keep them looking and behaving as EFTP's.
- **Before pushing:** `make` (no warnings: EUpdate's own files are built with `-Werror`), `make
  test` and `make itest`; after a change to a parser, `make fuzz` for a while, and keep what it
  finds in `tests/fuzz/found/TARGET`. After a change to the GEM side, `tests/hatari/gui_test.py
  WORK` (Falcon:16; TT and Falcon:256 too for drawing) and look at its screenshots; when the
  window changes, `tests/hatari/screenshots.py WORK` for `docs/` and `helpfront.py thumb` for
  `docs/help/WINDOW.IMG`. After a change to the start (common's `cpucheck.c`, `app_main()`, crt0, the
  Makefile's flags), `tests/hatari/st_test.py WORK`: on an ST, the alert and the desktop again; after
  a change to where EUpdate finds its files, `tests/hatari/appdir_test.py WORK` (on Erez's TOS 4.04
  where the session has it: EmuTOS hides the bug). After a change to how TLS.OVL
  is loaded, `tests/hatari/noovl_test.py WORK`.
- **Speed:** EUpdate runs only on a TT or Falcon, so its slow machine is `Falcon-4:16`, with and
  without NVDI. `make prof && tests/hatari/speed.py WORK` times what the user does (the window
  drawn, rows, scrolling, the dialogs); `PRG=build/prof/eupdate.prg tests/hatari/family_test.py
  WORK Falcon-4:16` times the check, the downloads and the unzip (the `AESGAP` lines in
  `WORK/hatari.log`).
- **The rigs mimic Erez's machine as closely as they can** (Erez, 22 Oct 2026): before a release,
  `tests/hatari/family_test.py WORK` updates stand-ins of the AERA family
  (`tests/github/standin_family.py`: each app's file names, sizes like the real ones', made-up
  bytes), served by `tests/family_server.py` as GitHub serves them: TLS with a chain like GitHub's,
  the files from a second host at signed addresses of 900 characters and more, idle connections
  closed; at a NetUSBee's pace, EUpdate on its own, the suggested rules on; under Erez's TOS 4.04
  and NVDI where this machine has them. What it can't mimic (STinG with a NetUSBee, GitHub itself,
  the real programs) is said in the report; the real family is the hub's acc_all.
- **The tests' GitHub** is `tests/server.py` (its repository is `tests/github/repo.json`, a real
  one as the API gave it): `APIBASE=` points EUpdate at it, as at a GitHub Enterprise server.
  GitHub's real API isn't asked by any test.
- **The FireBee edition** (Erez, 18 Nov 2026, on a tester's FireBee, where 2.08's and 2.09's
  EUPDATE.PRG froze: 68030 code doesn't run reliably on the ColdFire, and the 68060 stand-in
  can't show it): `EUPDFB.PRG`, `make binary/eupdfb.prg`, `tools/mkfirebee.py` running
  EUPDATE.PRG's own rules again as `-mcpu=5475` code into `build/cf/` (EWeb's way:
  `cpucheck_cf.S` its first code, `ll64.c` for libgcc). EUPDATE.PRG on a FireBee names it in
  an alert (`cpu_cf_alert`), it on any other machine names EUPDATE.PRG (`cpu_alert` under
  `__mcoldfire__`); `tests/hatari/firebee_test.py WORK wrong|falcon` checks both. EUpdate
  updates itself from its zip, so a FireBee gets EUPDFB.PRG too. Its own code on a ColdFire is
  common's `tests/coldfire` (qemu-m68k -cpu cfv4e) and the tester's: 2.10's EUPDFB.PRG runs on
  the tester's real FireBee (Erez, 18 Nov 2026: "The tester reported it now works"), which
  closes 2.08's and 2.09's freeze. Should the tester send an EUPDATE.LOG, its machine block (no
  addresses or tokens from it) becomes the FireBee's reference in the tests.
- **TLSFB.OVL for the FireBee** (the guidelines' rule of 15 Nov 2026, AERA-Atari a99ea65; for
  EUpdate Erez, 17 Nov: "every application that has a TLS must have this dual approach"):
  `tlsfb.ovl`, TLS.OVL's ColdFire build, ships beside `tls.ovl`, both copied from the same
  common commit and of one version; common's `tls_ovl.c` loads it where the `_CF_` cookie is
  set, and `update.c` carries it with a program as it carries TLS.OVL. The program is built
  `-mtune=68020-60`: 68030 code without the 64-bit `mulu.l`/`divu.l` a 68060 and a ColdFire
  lack (the 68060's, since EUPDFB.PRG is the FireBee's). On the FireBee, EUPDFB.PRG loads
  tlsfb.ovl (common's `tests/firebee` checks that choice); the stand-in, a 68060, runs no
  ColdFire code (an illegal instruction at once: AERA-Atari's `common/tests/coldfire` runs
  common's under qemu).
- **A release** carries `EUPDATE.PRG`, `EUPDFB.PRG`, `TLS.OVL` (common's, copied), `TLSFB.OVL`, `EUPDATE.HYP`, `CACERT.PEM`,
  `ROOTS.DAT`, `DEFAULTS.INF` and the icon (`icons/`, `make icons`), from `make dist`. `DEFAULTS.INF` (Erez, 24 Oct 2026) is
  what a first start follows, with no `EUPDATE.INF` yet: the AERA programs into `C:\AERA\NAME`,
  accessories to `C:\`, EUpdate in its own folder (`.`) at the release's version
  (`HAVE=v@VERSION@`, filled in by make). A new AERA program is added there.

## UI alignment (Erez's decisions of 9 and 10 Nov 2026; released in 2.03)

Done in 2.03 (maintenance lifted for it, then back on 11 Nov): D8, D14 and Copy, D15, D18, D23, D24,
D30, D35, D37, D41 (bold drawn as plain text twice: the VDI's bold cost a stock Falcon 55-95 ms),
D42, D43, D44, D50, D51, W1, W2, W6, W7. Still open:
- W24 (clicks through `click.h`): kept `0x102`, the AES counting the double click; under Hatari the
  one-click loop loses or mistimes the rig's double click (the hub has the traces).
- C11 (the own `form_loop` onto `dlg_do`) and C12 (the token's copy flag): the hub's, in `common/`
  first; taken with a release made anyway.

## Testing configuration (Erez, 10 Nov 2026; AERA-Atari 148df17, TEST-MATRIX.md)

From the next release made anyway: the speed rig's NVDI run on `Falcon-4:16` becomes Erez's own
configuration, `Falcon-4:256` with `AERA_WITH_NVDI=1` (`machines.ERZ_MACHINE`, `machines.erz_env()`);
the plain `Falcon-4:16` run stays only for a drawing change. A replacement, not an extra run. Since
the regression gate (17 Nov 2026) `Falcon-4:16` without NVDI is paired for speed too, and every rig
runs on Erez's TOS images and NVDI: a row without them is named as not run, not run on EmuTOS.
