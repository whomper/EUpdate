#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG with no TLS.OVL beside it (the guidelines 7.4): the check
at the start says TLS.OVL is missing in an alert and in EUPDATE.LOG, and
EUpdate goes on when it is answered, its window there. Screenshots in
WORK/shots.

On the FireBee stand-in (machines.py's "FireBee": FireTOS's cookies on a
68060 Falcon) the file it asks for is TLSFB.OVL, TLS.OVL's ColdFire build
(the guidelines' "TLSFB.OVL for the FireBee"): HAVE=tls.ovl puts TLS.OVL
beside it and the alert names TLSFB.OVL all the same; HAVE=tlsfb.ovl
puts TLSFB.OVL alone beside it, and the log says it was loaded (the
stand-in's 68060 can't run ColdFire code to the end: what it shows is
the file chosen by name; common/tests/coldfire runs that code).

    [HAVE=FILE,...] noovl_test.py WORK [MACHINE]   (Xvfb, xdotool;
                                                     MACHINE: Falcon:16)
"""
import os
import re
import shutil
import sys
import time

import rig
from rig import K
from machines import ci_path      # EUpdate writes lowercase names (6.6); Hatari keeps the case

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
    have = [f for f in os.environ.get("HAVE", "").lower().split(",") if f]
    for f in ["eupdate.prg", "cacert.pem", "roots.dat"] + have:
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    fb = spec.split(":")[0] == "FireBee"
    want = "tlsfb.ovl" if fb else "tls.ovl"
    with open(os.path.join(hd, "eupdate.inf"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nNATFEATS=1\r\nCHECK=1\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:1\r\nFOLDER=C:\\NET\r\n"
                "[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=C:\\NET\\EUPDATE\r\nUNZIP=1\r\n")
    m = rig.machine(spec, work)
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, extra=m.network())
    def log_path():
        return ci_path(hd, "eupdate.log")
    try:
        time.sleep(float(os.environ.get("BOOT", "45")))
        alert = r.shot("alert")
        log = log_path()
        text = open(log, errors="replace").read() if os.path.exists(log) else ""
        if want in have:
            # (the only overlay beside it, so the one loaded)
            check(re.search(r"TLS\.OVL \S+ loaded", text) is not None,
                  "the check at the start: EUPDATE.LOG says %s (the only overlay there) was loaded" % want.upper())
            check("is missing" not in text, "no overlay said missing")
        else:
            check("%s is missing" % want.upper() in text,
                  "the check at the start: EUPDATE.LOG says %s is missing" % want.upper())
            if fb:
                check("TLS.OVL is missing" not in text, "not TLS.OVL, which a FireBee can't run")
            r.key(K["RETURN"])
            time.sleep(2)
            after = r.shot("after")
            check(open(alert, "rb").read() != open(after, "rb").read(), "the alert answered, EUpdate goes on")
            inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()
            check("LATEST=" not in inf, "nothing was asked of GitHub")
    finally:
        if fails and os.path.exists(log_path()):
            print(open(log_path(), errors="replace").read()[-2000:])
        r.stop()
        m.stop()
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
