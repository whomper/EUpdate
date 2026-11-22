#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The FireBee's two programs, each where it belongs (a tester's FireBee
froze with 2.08's and 2.09's EUPDATE.PRG, 17 Nov 2026: its 68030 code
doesn't run on a ColdFire; Erez, 18 Nov 2026: EUPDFB.PRG, the ColdFire
build, common/tools/mkfirebee.py):

  wrong   EUPDATE.PRG on machines.py's FireBee stand-in (FireTOS's
          cookies): common's cpucheck.c names the FireBee's package in an alert and
          EUpdate goes, before anything of its 68030 code runs;
  falcon  the FireBee package's EUPDATE.PRG (binary/eupdfb.prg here) on a
          Falcon (TOS 4.04): common's cpucheck_cf.S names the standard package
          in an alert and it goes.

Each: an alert, answered with Return, the desktop again, and no
EUPDATE.INF written (nothing of the program ran). The stand-in is a
68060 and runs no ColdFire code (common/tests/coldfire runs common's
under qemu-m68k -cpu cfv4e): EUPDFB.PRG itself on the FireBee is the
tester's. Screenshots in WORK/shots.

    firebee_test.py WORK [wrong|falcon]     (Xvfb, xdotool)
"""
import os
import shutil
import sys
import time

import rig
from rig import K
from machines import ci_path

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
work = os.path.abspath(sys.argv[1])
case = sys.argv[2] if len(sys.argv) > 2 else "wrong"
fails = 0
CASES = {"wrong": ("FireBee:16", "eupdate.prg", "EUpdate's FireBee package"),
         "falcon": ("Falcon:16", "eupdfb.prg", "EUpdate's standard package")}


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def main():
    spec, prg, named = CASES[case]
    shutil.rmtree(work, ignore_errors=True)
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    for f in (prg, "tls.ovl", "tlsfb.ovl", "cacert.pem", "roots.dat", "defaults.inf"):
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    m = rig.machine(spec, work)
    r = rig.Rig(work, m, auto="C:\\" + prg.upper())
    print("on", m.describe(), flush=True)
    try:
        time.sleep(float(os.environ.get("BOOT", "45")))
        alert = r.shot("alert")
        r.key(K["RETURN"])
        time.sleep(4)
        after = r.shot("after")
        check(open(alert, "rb").read() != open(after, "rb").read(),
              "%s on %s: an alert (naming %s), answered" % (prg.upper(), spec, named))
        time.sleep(2)
        again = r.shot("again")
        check(open(after, "rb").read() == open(again, "rb").read(), "the desktop again, still")
        check(not os.path.exists(ci_path(hd, "eupdate.inf")), "no EUPDATE.INF: nothing of the program ran")
        check(not os.path.exists(ci_path(hd, "crash.log")), "no CRASH.LOG")
    finally:
        r.stop()
        m.stop()
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
