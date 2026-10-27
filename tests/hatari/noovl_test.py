#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG with no TLS.OVL beside it (the guidelines 7.4): the check
at the start says TLS.OVL is missing in an alert and in EUPDATE.LOG, and
EUpdate goes on when it is answered, its window there. Screenshots in
WORK/shots.

    noovl_test.py WORK [MACHINE]     (Xvfb, xdotool; MACHINE: Falcon:16)
"""
import os
import shutil
import sys
import time

import rig
from rig import K

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "Falcon:16"
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def main():
    shutil.rmtree(work, ignore_errors=True)
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    for f in ("EUPDATE.PRG", "CACERT.PEM", "ROOTS.DAT"):
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    with open(os.path.join(hd, "EUPDATE.INF"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nCHECK=1\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:1\r\nFOLDER=C:\\NET\r\n"
                "[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=C:\\NET\\EUPDATE\r\nUNZIP=1\r\n")
    m = rig.machine(spec, work)
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, extra=m.network())
    log = os.path.join(hd, "EUPDATE.LOG")
    try:
        time.sleep(float(os.environ.get("BOOT", "45")))
        alert = r.shot("alert")
        text = open(log, errors="replace").read() if os.path.exists(log) else ""
        check("TLS.OVL is missing" in text, "the check at the start: EUPDATE.LOG says TLS.OVL is missing")
        r.key(K["RETURN"])
        time.sleep(2)
        after = r.shot("after")
        check(open(alert, "rb").read() != open(after, "rb").read(), "the alert answered, EUpdate goes on")
        inf = open(os.path.join(hd, "EUPDATE.INF"), errors="replace").read()
        check("LATEST=" not in inf, "nothing was asked of GitHub")
    finally:
        if fails and os.path.exists(log):
            print(open(log, errors="replace").read()[-2000:])
        r.stop()
        m.stop()
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
