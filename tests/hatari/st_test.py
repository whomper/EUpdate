#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG started on an ST: it says it needs a Falcon or a TT, and
the desktop comes back when the alert is answered (the hub, 27 Oct 2026:
1.18 died on TOS 1.04 with an illegal instruction before its check; the
check is ui/cpucheck.c now, built for the 68000).

    st_test.py WORK [MACHINE]     (MACHINE: ST:mono; Xvfb)

The screen is read by its grey: the desktop's is a dither, half its
pixels black; an alert is a white box in the middle of it; EmuTOS's
crash screen and a white screen are neither.
"""
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rig  # noqa: E402
from PIL import Image  # noqa: E402

TOP = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "ST:mono"
failed = 0


def check(ok, what):
    global failed
    print("%s %s" % ("ok  " if ok else "FAIL", what), flush=True)
    failed += not ok


def black(im, box):
    """the share of black pixels in box (x0, y0, x1, y1)"""
    hist = im.crop(box).histogram()
    return sum(hist[:128]) / float(sum(hist))


def look(path):
    """'alert', 'desktop' or 'other' for a screenshot"""
    im = Image.open(path).convert("L")
    w, h = im.size
    middle = black(im, (w // 3, h * 2 // 5, w * 2 // 3, h * 3 // 5))
    below = black(im, (w // 8, h * 4 // 5, w * 7 // 8, h * 19 // 20))
    if 0.35 < below < 0.65:                     # the desktop's dither around it
        if middle < 0.3:
            return "alert"
        if abs(middle - below) < 0.1:
            return "desktop"
    return "other"


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    m = rig.machine(spec, work)
    os.makedirs(os.path.join(work, "hd"), exist_ok=True)
    shutil.copy(os.environ.get("PRG", os.path.join(TOP, "binary", "EUPDATE.PRG")),
                os.path.join(work, "hd", "EUPDATE.PRG"))
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, display=os.environ.get("XDISPLAY", ":99"))
    try:
        seen = "other"
        for i in range(30):                     # up to a minute from boot
            time.sleep(2)
            seen = look(r.shot("t%02d" % i))
            if seen == "alert":
                break
        check(seen == "alert", "on %s EUpdate says it needs a Falcon or a TT" % spec)
        r.key(rig.K["RETURN"])                  # Quit
        back = "other"
        for i in range(10):
            time.sleep(1.5)
            back = look(r.shot("after%02d" % i))
            if back == "desktop":
                break
        check(back == "desktop", "Quit: the desktop again")
    finally:
        r.stop()
        m.stop()
    print("%d failed" % failed)
    sys.exit(1 if failed else 0)


main()
