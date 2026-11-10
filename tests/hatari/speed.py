#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The speed rig (the guidelines' "Speed: the stock machines set it"):
build/prof/eupdate.prg (make prof: -DAES_PROFILE=20) on a stock Falcon,
the longest stretch between event calls of each thing the user does,
from the AESGAP lines on the emulator's console. Fails when one is over
its time.

    make prof && speed.py WORK [MACHINE]     (MACHINE: Erez's configuration)

By default Erez's own configuration (Erez, 10 Nov 2026): machines.ERZ_MACHINE,
a stock Falcon with TOS 4.04 and NVDI in 256 colours (erz_env()); where this
computer has no TOS images or NVDI, the same machine on EmuTOS, a stand-in
(common/tools/machines.py says which). Falcon-4:16 for a drawing change. The check and the
downloads are timed by family_test.py with PRG=build/prof/eupdate.prg.
"""
import os
import re
import shutil
import sys
import time

import rig
from rig import K
from machines import ci_path, ERZ_MACHINE, erz_env   # ci_path: EUpdate writes lowercase names (6.6)

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else ERZ_MACHINE
if spec == ERZ_MACHINE:
    os.environ.update(erz_env())
NREPOS = 40
fails = 0
results = []


def drive_c():
    hd = os.path.join(work, "hd")
    os.makedirs(hd, exist_ok=True)
    shutil.copy(os.path.join(TOP, "build", "prof", "eupdate.prg"), hd)
    for f in ("tls.ovl", "cacert.pem", "roots.dat"):
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    # more repositories than the window shows, as they are after a check
    with open(os.path.join(hd, "eupdate.inf"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nCHECK=0\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:1\r\nFOLDER=C:\\NET\r\n")
        for i in range(NREPOS):
            f.write("[REPO]\r\nREPO=whomper/Repo%02d\r\nFOLDER=C:\\NET\\REPO%02d\r\nUNZIP=1\r\n"
                    "HAVE=v1.%d\r\nHAVEDATE=2026-10-01\r\nLATEST=v1.%d\r\nLATESTDATE=2026-10-20\r\n" %
                    (i, i, i, i + i % 3))
    return hd


# where things are on the 640x480 screen: EUpdate's window full size,
# the Following pane's slider the AES's at the right
ARROW_DOWN = (628, 445)
TRACK = (628, 400)
DESK = (40, 9)
ABOUT = (60, 27)
REST = (300, 420)            # an empty part of the Find pane: no tip


class Console:
    """the AESGAP lines Hatari has written since the last look"""

    def __init__(self, path):
        self.path, self.at = path, 0

    def lines(self):
        with open(self.path, errors="replace") as f:
            f.seek(self.at)
            text = f.read()
            self.at = f.tell()
        return text.split("\n")

    def gaps(self, lines=None):
        return [(int(m.group(1)), m.group(2).strip()) for m in
                (re.match(r"AESGAP (\d+) ms (.*)", l) for l in (self.lines() if lines is None else lines)) if m]


def report(what, worst, limit):
    global fails
    over = worst[0] > limit
    fails += over
    results.append((what, worst[0], limit))
    print("%s %-46s %5d ms (%d)%s" % ("FAIL" if over else "ok  ", what, worst[0], limit,
                                      "  " + worst[1] if worst[1] else ""), flush=True)


def measure(con, what, limit, wait=2.0):
    """the longest stretch since the last look"""
    time.sleep(wait)
    report(what, max(con.gaps() or [(0, "")]), limit)


def each(con, what, limit, do, times, pace=0.6):
    """an action done several times, its longest stretch"""
    con.gaps()
    for _ in range(times):
        do()
        time.sleep(pace)
    measure(con, what, limit, 1.0)


def main():
    global fails
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    drive_c()
    m = rig.machine(spec, work)
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, display=os.environ.get("XDISPLAY", ":98"))
    con = Console(os.path.join(work, "hatari.log"))
    try:
        time.sleep(float(os.environ.get("BOOT", "50")))
        r.move(*REST)
        r.shot("start")
        lines = con.lines()
        # common's AESSTART: appl_init to the first event call; the window
        # is drawn in the first redraw after it
        start = [int(m.group(1)) for m in (re.match(r"AESSTART (\d+) ms", l) for l in lines) if m]
        drawn = max([g for g in con.gaps(lines) if g[1] == "redraw"] or [(0, "")])
        report("the program started: its window drawn", (start[0] + drawn[0] if start else 99999, ""), 2000)
        report("the window drawn whole (opened)", drawn, 300)
        if os.environ.get("EXPLORE"):
            return
        r.key(K["DOWN"])                       # the first row
        time.sleep(1)
        each(con, "a row selected (Down), the list scrolled by it", 50, lambda: r.key(K["DOWN"]), 14)
        each(con, "a line scrolled (the AES's arrow)", 50, lambda: r.click(*ARROW_DOWN), 6)
        r.move(*REST)
        each(con, "a page scrolled (a click in the track)", 150, lambda: r.click(*TRACK), 3, 1.0)
        r.move(*REST)
        r.shot("scrolled")
        con.gaps()
        r.click(*DESK)
        time.sleep(1)
        r.click(*ABOUT)
        measure(con, "the About box opened", 150)
        r.shot("about")
        r.key(K["RETURN"])
        measure(con, "the window uncovered (the About box closed)", 300)
        r.key(0x33, ctrl=True)                 # ^,
        measure(con, "Settings opened (^,)", 150)
        r.shot("settings")
        r.key(K["UNDO"])
        measure(con, "Settings closed (Undo), the window uncovered", 300)
        r.ctrl("f")
        measure(con, "Find opened (^F)", 150)
        each(con, "a letter typed in Find's field", 50, lambda: r.type("e", 0), 6, 0.4)
        r.shot("find")
        r.key(K["UNDO"])
        measure(con, "Find closed (Undo), the window uncovered", 300)
        r.move(60, 129)                        # a row: its tip
        measure(con, "a row's tip shown", 150, 2.5)
        r.move(*REST)
        r.shot("end")
        r.ctrl("q")                            # quit: the longest waits in EUPDATE.LOG
        time.sleep(4)
        log = open(ci_path(os.path.join(work, "hd"), "eupdate.log"), errors="replace").read()
        line = [l for l in log.split("\n") if "the longest waits:" in l]
        print("%s EUPDATE.LOG at quit: %s" % ("ok  " if line else "FAIL", line[-1].split(" -- ")[-1] if line
                                              else "no line of the longest waits"), flush=True)
        fails += not line
    finally:
        r.stop()
        m.stop()
    print("%d over" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
