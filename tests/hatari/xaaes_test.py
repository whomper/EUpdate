#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUpdate under FreeMiNT and XaAES (the regression gate's row 5, and its
68060 row: standards/TEST-MATRIX.md), through common's
tests/xaaes/run.py: it starts, its window is drawn, Settings opens and
closes, the About box opens and closes, it quits, and it leaves no
CRASH.LOG; EUPDATE.LOG has its start and its quit. Screenshots and the
disk as it was left in WORK.

    xaaes_test.py WORK [plain|ct60|ext2]

plain: a Falcon's 68030; ct60: the 68060 (a CT60's stand-in); ext2: the
release's files on a case-sensitive ext2 drive, D:, as a tester's
(EUpdate writes lowercase names, the guidelines 6.6). Needs make base in
common/tests/xaaes (FreeMiNT 1.17.0) and EmuTOS, which machines.py
fetches: FreeMiNT boots from it here (its rig's way).
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

work = os.path.abspath(sys.argv[1])
how = sys.argv[2] if len(sys.argv) > 2 else "plain"
fails = 0
FILES = ("eupdate.prg", "tls.ovl", "tlsfb.ovl", "cacert.pem", "roots.dat")
INF = ("[OPTIONS]\r\nNATFEATS=1\r\nCHECK=0\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:1\r\n"
       "FOLDER=C:\\NET\r\n[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=C:\\NET\\EUPDATE\r\nUNZIP=1\r\n")


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def find(top, name):
    """a file under top, its name in any case"""
    for d, _, fs in os.walk(top):
        for f in fs:
            if f.lower() == name:
                return os.path.join(d, f)
    return None


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    rel = os.path.join(work, "release")
    os.makedirs(rel)
    for f in FILES:
        shutil.copy(os.path.join(TOP, "binary", f), rel)
    with open(os.path.join(rel, "eupdate.inf"), "w", newline="") as f:
        f.write(INF)
    run = os.path.join(TOP, "common", "tests", "xaaes", "run.py")
    args = [sys.executable, run, os.path.join(work, "w"), machines.emutos(), "--nvdi"]
    if how == "ext2":
        args += ["--ext2", rel, "--start", "d:\\eupdate.prg"]
    else:
        args += [os.path.join(rel, "eupdate.prg")]
        args += ["--copy"] + [os.path.join(rel, f) for f in FILES[1:] + ("eupdate.inf",)]
        if how == "ct60":
            args += ["--machine", "ct60"]
    # start; the window; Settings (^,) and Undo; the About box from the
    # first menu (its title at the menu bar's left) and Return; ^Q
    args += ["--wait", "70", "--steps",
             "shot; ctrl ,; wait 4; shot; key 0x61; wait 3; click 40 8; wait 2; click 60 30; wait 4; "
             "shot; key 0x1c; wait 3; shot; ctrl q; wait 8; shot"]
    print("on", "FreeMiNT 1.17.0 + XaAES with Erez's NVDI, EmuTOS 1.4 under it,",
          {"plain": "a Falcon's 68030", "ct60": "a 68060 (a CT60's stand-in)",
           "ext2": "the release on an ext2 D:"}[how], flush=True)
    r = subprocess.run(args, capture_output=True, text=True, timeout=900)
    print(r.stdout[-1500:], r.stderr[-800:], flush=True)
    check(r.returncode == 0, "the rig's steps ran")
    out = os.path.join(work, "w", "ext2" if how == "ext2" else "out")
    log = find(out, "eupdate.log")
    text = open(log, errors="replace").read() if log else ""
    check("EUpdate " in text and " on " in text, "EUPDATE.LOG: it started (the machine line)")
    check(find(out, "crash.log") is None, "no CRASH.LOG")
    hl = os.path.join(work, "w", "hatari.log")
    htext = open(hl, errors="replace").read() if os.path.exists(hl) else ""
    check("Exception" not in htext and "Panic" not in htext and "BUS ERROR" not in htext.upper(),
          "no exception in Hatari's log")
    if fails and text:
        print(text[-1500:])
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
