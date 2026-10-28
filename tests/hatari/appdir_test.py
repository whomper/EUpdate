#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUpdate finds its own files in its own folder (common's app_dir(), the
guidelines 6.1), also when another program starts it: on TOS 1.04 and
4.04 a program started with shel_write starts in the desktop's folder,
whatever the caller made current (EMail's finding, 28 Oct 2026).

common/tests/appdir's C:\\LAUNCH.PRG makes A:\\APP current and starts
A:\\APP\\PROBE.PRG, here EUPDATE.PRG under that name, from a real FAT
floppy (mtools: Hatari's emulated C: keeps one current folder for every
program and hides it). EUpdate's first start writes EUPDATE.KEY and
reads DEFAULTS.INF beside the program: so A:\\APP\\EUPDATE.KEY, and none
in C:\\. Then EUPDATE.PRG started as from the desktop (C:\\APP\\EUPDATE.PRG,
Hatari's --auto): C:\\APP\\EUPDATE.KEY. The bug shows only on Atari's
TOS (machines.py's AERA_TOS); EmuTOS sets the folder either way.

    appdir_test.py WORK [MACHINE]     (MACHINE: Falcon:16; mtools)
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "Falcon:16"
WAIT = float(os.environ.get("WAIT", "60"))
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def mt(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def hatari(w, hd, auto, floppy=None):
    m = machines.Machine(spec, hd)
    args = ["hatari", "--fast-boot", "yes", "--log-level", "warn"] + m.args() + ["--auto", auto]
    if floppy:
        args += ["--disk-a", floppy]
    p = subprocess.Popen(args, env=m.env(), stdout=open(os.path.join(w, "hatari.log"), "w"),
                         stderr=subprocess.STDOUT)
    time.sleep(WAIT)
    p.terminate()           # Hatari writes the floppy's changes back as it ends
    try:
        p.wait(20)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
    return m.describe()


def main():
    shutil.rmtree(work, ignore_errors=True)
    build = os.path.join(work, "build")
    subprocess.run(["make", "-s", "-C", os.path.join(TOP, "common", "tests", "appdir"), "B=" + build,
                    os.path.join(build, "LAUNCH.PRG")], check=True)
    prg = os.environ.get("PRG", os.path.join(TOP, "binary", "EUPDATE.PRG"))
    defaults = os.path.join(TOP, "binary", "DEFAULTS.INF")

    # started by another program, from A:\APP
    w = os.path.join(work, "launched")
    hd = os.path.join(w, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(build, "LAUNCH.PRG"), hd)
    img = os.path.join(w, "a.st")
    mt("mformat", "-i", img, "-f", "720", "-C", "::")
    mt("mmd", "-i", img, "::/APP")
    mt("mcopy", "-i", img, prg, "::/APP/PROBE.PRG")
    mt("mcopy", "-i", img, defaults, "::/APP/DEFAULTS.INF")
    what = hatari(w, hd, "C:\\LAUNCH.PRG", img)
    print("on", what, flush=True)
    names = mt("mdir", "-b", "-i", img, "::/APP").upper()
    check("EUPDATE.KEY" in names, "started by another program from A:\\APP: its EUPDATE.KEY in A:\\APP")
    check(not os.path.exists(os.path.join(hd, "EUPDATE.KEY")), "and none in C:\\ (the desktop's folder)")

    # started as from the desktop, from C:\APP
    w = os.path.join(work, "desktop")
    hd = os.path.join(w, "hd")
    os.makedirs(os.path.join(hd, "APP"))
    shutil.copy(prg, os.path.join(hd, "APP", "EUPDATE.PRG"))
    shutil.copy(defaults, os.path.join(hd, "APP", "DEFAULTS.INF"))
    hatari(w, hd, "C:\\APP\\EUPDATE.PRG")
    check(os.path.exists(os.path.join(hd, "APP", "EUPDATE.KEY")) and
          not os.path.exists(os.path.join(hd, "EUPDATE.KEY")),
          "started from the desktop in C:\\APP: its EUPDATE.KEY in C:\\APP")
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
