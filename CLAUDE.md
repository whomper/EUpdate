# Notes for working on EUpdate

EUpdate is one of the AERA Atari apps (ECal, EContact, EMail, EFTP, EHypView, ClaudeST, EWeb). They are meant to
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
- **Releases go out without asking Erez** (his decision, 23 Oct 2026; guidelines 1.2). Release when
  work on `main` is ready:
  - this repo's checks pass;
  - the rigs on his TOS pass, where this file asks for them;
  - the hub's family checks pass.

  Then give Erez the link in one line, with anything he has to do, and send the hub the link. Roll
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
- **Testing with another AERA program uses its latest release**, what Erez runs, never a build of its
  `main`: `common/tools/getrelease.py APP DIR FILE...` fetches it. Only for a change both programs are
  still making, test against the other's `main` at a named commit, say so, and test again against its
  release when it is out.
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
  drawn, rows, scrolling, the dialogs); `PRG=build/prof/EUPDATE.PRG tests/hatari/family_test.py
  WORK Falcon-4:16` times the check, the downloads and the unzip (the `AESGAP` lines in
  `WORK/hatari.log`).
- **The rigs mimic Erez's machine as closely as they can** (Erez, 22 Oct 2026): before a release,
  `tests/hatari/family_test.py WORK` updates the AERA family from their real latest releases
  (`tests/github/fetch_family.py` into `~/.cache/aera/releases`, served by `tests/family_server.py`
  as GitHub serves them: TLS with a chain like GitHub's, the files from a second host at signed
  addresses of 900 characters and more, idle connections closed), at a NetUSBee's pace, with ECal,
  EContact and EHypView loaded as accessories and the suggested rules on; under Erez's TOS 4.04
  and NVDI where this machine has them. What it can't mimic (STinG with a NetUSBee, GitHub itself,
  the accessories syncing) is said in the report.
- **The tests' GitHub** is `tests/server.py` (its repository is `tests/github/repo.json`, a real
  one as the API gave it): `APIBASE=` points EUpdate at it, as at a GitHub Enterprise server.
  GitHub's real API isn't asked by any test.
- **A release** carries `EUPDATE.PRG`, `TLS.OVL` (common's, copied), `EUPDATE.HYP`, `CACERT.PEM`,
  `ROOTS.DAT`, `DEFAULTS.INF` and the icon (`icons/`, `make icons`), from `make dist`. `DEFAULTS.INF` (Erez, 24 Oct 2026) is
  what a first start follows, with no `EUPDATE.INF` yet: the AERA programs into `C:\AERA\NAME`,
  accessories to `C:\`, EUpdate in its own folder (`.`) at the release's version
  (`HAVE=v@VERSION@`, filled in by make). A new AERA program is added there.
