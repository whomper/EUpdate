#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The pictures in docs/: EUPDATE.PRG on a Falcon in 640x480 with 256
colours, following some of the AERA family (one with a new release, one
not downloaded yet), with whomper's repositories found below; and the
Follow dialog with its suggested rules. tests/server.py stands in for
GitHub.

    screenshots.py WORK     (Xvfb, xdotool)
"""
import os
import shutil
import subprocess
import sys
import time

import rig
from rig import K
from gui_test import wait_port, TOP

work = os.path.abspath(sys.argv[1])
DOCS = os.path.join(TOP, "docs")
PORT = 18742
FOLLOWED = (("EContact", "v1.58", "C:\\NET\\ECONTACT"),
            ("EMail", "v1.50", "C:\\NET\\EMAIL"), ("EFTP", "v1.44", "C:\\NET\\EFTP"),
            ("EHypView", "v1.16", "C:\\NET\\EHYPVIEW"), ("EWeb", "", "C:\\NET\\EWEB"),
            ("EUpdate", "v1.1", "C:\\NET\\EUPDATE"))


def main():
    shutil.rmtree(work, ignore_errors=True)
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    for f in ("EUPDATE.PRG", "CACERT.PEM", "ROOTS.DAT"):
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    with open(os.path.join(hd, "EUPDATE.INF"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nCHECK=1\r\nAPIBASE=http://127.0.0.1:%d\r\nFOLDER=C:\\NET\r\n" % PORT)
        for name, have, folder in FOLLOWED:
            f.write("[REPO]\r\nREPO=whomper/%s\r\nFOLDER=%s\r\nHAVE=%s\r\nHAVEDATE=2026-10-20\r\n" %
                    (name, folder, have))
    srv = subprocess.Popen([sys.executable, os.path.join(TOP, "tests", "server.py"), str(PORT)],
                           stdout=subprocess.DEVNULL)
    wait_port(PORT)
    m = rig.machine("Falcon:256", work)
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, extra=m.network())
    try:
        time.sleep(float(os.environ.get("BOOT", "50")))
        r.ctrl("f")
        time.sleep(1.5)
        r.type("whomper")
        r.click(200, 255)                      # Owners
        r.key(K["RETURN"])
        time.sleep(10)
        r.click(60, 331, double=True)          # whomper: the family
        time.sleep(12)
        r.click(60, 161)                       # EFTP: the new release, selected
        r.move(330, 30)                        # the title bar: no tooltip
        time.sleep(1)
        shutil.copy(r.shot("main"), os.path.join(DOCS, "screenshot.png"))
        r.click(60, 331, double=True)          # whomper/ECal, not followed: Follow, its rules suggested
        time.sleep(12)
        r.move(330, 30)
        shutil.copy(r.shot("follow"), os.path.join(DOCS, "follow.png"))
        r.key(K["UNDO"])
    finally:
        r.stop()
        m.stop()
        srv.kill()


if __name__ == "__main__":
    main()
