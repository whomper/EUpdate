#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG in Hatari, on the family's Falcon (common/tools/machines.py),
against tests/server.py (a GitHub of the tests' own, over HTTP: TLS is
make itest's) through FAKESTNG: the check at the start, Find, an owner's
repositories, Follow with its rules, the download into the folders, Get
info, Settings and the About box. Screenshots in WORK/shots.

    gui_test.py WORK [MACHINE]     (Xvfb, xdotool; MACHINE: Falcon:16)
"""
import os
import shutil
import socket
import subprocess
import sys
import time

import rig
from rig import K
from machines import ci_path      # EUpdate writes lowercase names (6.6); Hatari keeps the case

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "Falcon:16"
PORT = 18741
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def drive_c():
    hd = os.path.join(work, "hd")
    os.makedirs(hd, exist_ok=True)
    for f in ("eupdate.prg", "tls.ovl", "cacert.pem", "roots.dat"):
        shutil.copy(os.path.join(TOP, "binary", f), hd)
    with open(os.path.join(hd, "eupdate.inf"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nCHECK=1\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:%d\r\nFOLDER=C:\\NET\r\n"
                "[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=C:\\NET\\EUPDATE\r\nUNZIP=1\r\nHAVE=v1.0\r\n"
                "HAVEDATE=2026-10-01\r\nRULE=README.TXT;-\r\n"
                "[REPO]\r\nREPO=whomper/EDOS\r\nFOLDER=C:\\XL\\EDOS\r\n" % PORT)
    return hd


def server_token():
    sys.path.insert(0, os.path.join(TOP, "tests"))
    import server
    return server.LONG_TOKEN


def changed(r, base, name, act, tries=5, again=True):
    """act (again: each try, else once) until the screen differs from
    the shot base; that shot's path"""
    for i in range(tries):
        if again or not i:
            act()
        time.sleep(1.5)
        shot = r.shot(name)
        if open(shot, "rb").read() != open(base, "rb").read():
            return shot
    return shot


def wait_port(port, host="127.0.0.1"):
    for _ in range(50):
        try:
            socket.create_connection((host, port), 0.2).close()
            return
        except OSError:
            time.sleep(0.1)


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    hd = drive_c()
    auth_log = os.path.join(work, "auth.log")
    srv = subprocess.Popen([sys.executable, os.path.join(TOP, "tests", "server.py"), str(PORT)],
                           stdout=subprocess.DEVNULL, env=dict(os.environ, AUTH_LOG=auth_log))
    wait_port(PORT)
    m = rig.machine(spec, work)
    print("on", m.describe(), flush=True)
    r = rig.Rig(work, m, extra=m.network())
    try:
        time.sleep(float(os.environ.get("BOOT", "45")))
        r.shot("start")
        inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()
        check("LATEST=v1.1" in inf, "the check at the start: v1.1 is the latest")
        r.ctrl("f")
        time.sleep(1.5)
        r.type("eftp")
        r.shot("find")
        r.key(K["RETURN"])
        time.sleep(12)
        r.shot("found")
        r.move(600, 450)
        # the owners, then one's repositories, then back
        r.ctrl("f")
        time.sleep(1.5)
        r.key(K["DOWN"])                       # the Owners choice: through the ticks' words
        r.click(330, 233)                      # [Owners] (radio buttons, Erez 10 Nov 2026)
        r.key(K["RETURN"])
        time.sleep(10)
        r.shot("owners")
        r.click(60, 331, double=True)          # whomper
        time.sleep(12)
        r.shot("owner_repos")
        r.click(60, 331, double=True)          # whomper/R0: Follow, its latest release asked first
        time.sleep(12)
        r.shot("follow")
        for _ in range(4):                     # back into the folder's name, a letter in there
            r.key(K["LEFT"])
        r.type("Q")
        edited = r.shot("follow_edit")
        # the guidelines 3.5: Esc empties the field the cursor is in and
        # the dialog stays; Undo cancels it
        r.key(K["ESC"])
        time.sleep(1)
        emptied = r.shot("follow_esc")
        r.key(K["UNDO"])
        time.sleep(1.5)
        undone = r.shot("follow_undo")
        same = lambda a, b: open(a, "rb").read() == open(b, "rb").read()
        check(not same(edited, emptied) and not same(emptied, undone),
              "in Follow, Esc empties the folder's field and Undo cancels")
        r.key(K["BACKSPACE"])
        time.sleep(1.5)
        r.shot("back")
        # Update the one with a new release: unpacked, README.TXT left out (its rule)
        r.click(60, 129)
        r.ctrl("d")                            # Update (^U closes, the guidelines 3.2)
        time.sleep(25)
        r.shot("updated")
        prg = ci_path(hd, "NET/EUPDATE/eupdate.prg")	# as the zip names it (6.6)
        check(os.path.exists(prg) and os.path.getsize(prg) == 10240,
              "Update: the zip unpacked into its folder")
        check(not os.path.exists(ci_path(hd, "NET/EUPDATE/icons/readme.txt")),
              "its rule left README.TXT out")
        inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()
        check("HAVE=v1.1" in inf, "it has v1.1 now, in EUPDATE.INF")
        # ^C: the row chosen, as one line, in the clipboard (common's clip.c)
        r.ctrl("c")
        time.sleep(2)
        scrap = os.path.join(hd, "CLIPBRD", "SCRAP.TXT")
        check(os.path.exists(scrap) and open(scrap, "rb").read() == b"whomper/EUpdate\r\n",
              "^C: whomper/EUpdate in C:\\CLIPBRD\\SCRAP.TXT")
        r.ctrl("i")
        time.sleep(2)
        r.shot("info")
        r.key(K["UNDO"])
        time.sleep(1)
        before = r.shot("before_settings")
        settings = changed(r, before, "settings", lambda: r.key(0x33, ctrl=True), again=False)   # ^,
        # every 999 hours: refused with an alert saying what to type, the
        # field kept as typed (D30, Erez's words of 10 Nov 2026); then 24
        r.click(410, 217)                      # [Every] __ hours
        time.sleep(1)
        r.key(K["ESC"])                        # the hours field emptied
        r.type("999")
        time.sleep(0.5)
        typed = r.shot("settings_999")
        r.key(K["RETURN"])                     # OK
        time.sleep(1.5)
        shown = r.shot("settings_hours_alert")
        r.key(K["RETURN"])                     # the alert's OK
        time.sleep(1.5)
        back = r.shot("settings_hours_back")
        check(open(shown, "rb").read() != open(typed, "rb").read() and
              open(back, "rb").read() == open(typed, "rb").read(),
              "Settings: 999 hours refused by an alert, the dialog back with the field as typed")
        r.key(K["ESC"])
        r.type("24")
        time.sleep(0.5)
        # a fine-grained token, 93 characters, into the field 30 wide
        # (the guidelines 5.4): the field scrolls, and all of it is kept.
        # Typed only once the GitHub page is up: on a loaded host the
        # dialog can be drawn before it takes clicks (the hub, 26 Oct)
        changed(r, settings, "settings_github_page", lambda: r.click(140, 241))
        r.type(server_token())
        time.sleep(0.5)
        r.shot("settings_github")
        r.click(140, 281)                      # Look
        time.sleep(1)
        r.shot("settings_look")
        r.click(140, 321)
        time.sleep(1)
        r.shot("settings_advanced")
        r.key(K["RETURN"])                     # OK
        time.sleep(1.5)
        r.ctrl("r")                            # a check, with the token
        time.sleep(12)
        inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()
        check("HOURS=24" in inf, "Settings: every 24 hours kept once corrected")
        sent = open(auth_log).read().split("\n") if os.path.exists(auth_log) else []
        check("Bearer " + server_token() in sent,
              "a 93-character token typed into Settings reaches GitHub whole")
        r.click(40, 9)                         # the Desk menu: About
        time.sleep(1)
        r.click(60, 27)
        time.sleep(2)
        r.shot("about")
        r.key(K["RETURN"])
        time.sleep(1)
    finally:
        if fails and os.path.exists(ci_path(hd, "eupdate.log")):
            print(open(ci_path(hd, "eupdate.log"), errors="replace").read()[-3000:])
        r.stop()
        m.stop()
        srv.kill()
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
