#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A FireBee's EUpdate from 2.14's EUPDFB.PRG through 2.15, the bridge,
to 2.16 (Erez, 22 Nov 2026: "Bridge for 2.15"; the guidelines 7.4):

  1. 2.14, running as C:\\EUPDFB.PRG, follows whomper/EUpdate in its own
     folder and finds 2.15: two zips, which it unpacks both, the FireBee's
     first (GitHub lists them by name). The standard one, written last,
     carries EUPDFB.PRG and TLSFB.OVL once more: so EUPDFB.PRG becomes 2.15.
  2. 2.15, started again as EUPDFB.PRG, finds 2.16 and takes the FireBee's
     zip alone: EUPDATE.PRG and TLS.OVL the FireBee's, EUPDFB.PRG and
     TLSFB.OVL gone, and an alert says to start EUPDATE.PRG.

On machines.py's FireBee stand-in (_CF_; a 68060 that runs no ColdFire
code): each "ColdFire" program is its 68030 build without the FireBee
alert (2.14's from the tag v2.14, built here; 2.15's make cftest's), and
TLSFB.OVL is TLS.OVL; over plain HTTP (tests/family_server.py PLAIN=1),
as the stand-in runs no TLS handshake. EUpdate alone (the guidelines 1.2).

    bridge_test.py WORK          (Xvfb, xdotool; make and make cftest first)
"""
import os
import shutil
import socket
import subprocess
import sys
import time
import zipfile

import rig
from machines import ci_path

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
work = os.path.abspath(sys.argv[1])
PORT = 18770
FILES_HOST = "127.0.0.2"
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what, flush=True)
    fails += not cond


def wait_port(port, host="127.0.0.1"):
    for _ in range(50):
        try:
            socket.create_connection((host, port), 0.2).close()
            return
        except OSError:
            time.sleep(0.1)


def build_214(out):
    """2.14's EUPDATE.PRG without its FireBee alert, from the tag: what
    2.14's EUPDFB.PRG does, in 68030 code for the stand-in"""
    src = os.path.join(work, "v214")
    subprocess.run(["git", "-C", TOP, "worktree", "add", "-f", "--detach", src, "v2.14"], check=True,
                   capture_output=True)
    try:
        main = os.path.join(src, "ui", "main.c")
        text = open(main).read().replace("const char cpu_cf_alert[]",
                                         "static const char cpu_cf_alert_off[] __attribute__((unused))")
        open(main, "w").write(text)
        subprocess.run(["make", "-s", "-C", src, "OBJDIR=build/cft", "build/cft/eupdate.elf"], check=True,
                       capture_output=True)
        subprocess.run([sys.executable, os.path.join(src, "common", "tools", "elf2tos.py"),
                        os.path.join(src, "build", "cft", "eupdate.elf"), out], check=True, capture_output=True,
                       env=dict(os.environ, PRGFLAGS="7"))
    finally:
        subprocess.run(["git", "-C", TOP, "worktree", "remove", "--force", src], capture_output=True)


def release(cache, tag, std, fb):
    """CACHE/EUpdate/<tag>: eupdate-x.y.zip and eupdate-x.y-firebee.zip,
    each {name: bytes}"""
    d = os.path.join(cache, "EUpdate", tag)
    os.makedirs(d)
    for name, files in (("eupdate-%s.zip" % tag[1:], std), ("eupdate-%s-firebee.zip" % tag[1:], fb)):
        with zipfile.ZipFile(os.path.join(d, name), "w", zipfile.ZIP_DEFLATED) as zf:
            for n, b in files.items():
                zf.writestr(n, b)
    with open(os.path.join(cache, "EUpdate", "TAG"), "w") as f:
        f.write(tag + "\n")


def serve(cache):
    srv = [subprocess.Popen([sys.executable, os.path.join(TOP, "tests", "family_server.py"), str(p), str(PORT + 1),
                             cache, "-", "-"], stdout=subprocess.DEVNULL,
                            env=dict(os.environ, FILES_HOST=FILES_HOST, PLAIN="1"))
           for p in (PORT, PORT + 1)]
    wait_port(PORT)
    wait_port(PORT + 1, FILES_HOST)
    return srv


def have(hd):
    inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()
    return inf.split("HAVE=")[1].split()[0] if "HAVE=" in inf else ""


def run(hd, tag, stage):
    """EUPDFB.PRG started from the desktop's auto, until EUpdate has tag"""
    m = rig.machine("FireBee:16", work)
    r = rig.Rig(work, m, display=os.environ.get("XDISPLAY", ":99"), auto="C:\\EUPDFB.PRG",
                extra=m.network(rate=int(os.environ.get("RATE", "100000"))))
    print("on", m.describe(), flush=True)
    try:
        for _ in range(60):
            time.sleep(5)
            if have(hd) == tag:
                break
        time.sleep(8)
        return r.shot(stage), r, m
    except Exception:
        r.stop()
        m.stop()
        raise


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    b = lambda p: open(os.path.join(TOP, p), "rb").read()
    std_prg, cf_prg, tls = b("binary/eupdate.prg"), b("build/cft/eupdate.prg"), b("binary/tls.ovl")
    tls16 = tls + b"2.16's, the FireBee's"     # never loaded here: told apart by its tail
    build_214(os.path.join(hd, "EUPDFB.PRG"))
    shutil.copy(os.path.join(TOP, "binary", "tls.ovl"), os.path.join(hd, "tlsfb.ovl"))   # 2.14's beside it
    shutil.copy(os.path.join(TOP, "binary", "cacert.pem"), hd)
    with open(os.path.join(hd, "eupdate.inf"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nNATFEATS=1\r\nCHECK=1\r\nFETCH=1\r\nLOG=1\r\nAPIBASE=http://127.0.0.1:%d\r\n"
                "[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=.\r\nUNZIP=1\r\nHAVE=v2.14\r\n" % PORT)

    # 1. 2.14 meets 2.15: both zips, the standard one last
    cache = os.path.join(work, "c215")
    release(cache, "v2.15", {"eupdate.prg": std_prg, "tls.ovl": tls, "eupdfb.prg": cf_prg, "tlsfb.ovl": tls},
            {"eupdate.prg": cf_prg, "tlsfb.ovl": tls})
    srv = serve(cache)
    try:
        _, r, m = run(hd, "v2.15", "v215")
        r.ctrl("q")
        time.sleep(6)
        r.stop()
        m.stop()
    finally:
        for s in srv:
            s.kill()
            s.wait()
    rd = lambda n: open(ci_path(hd, n), "rb").read() if os.path.exists(ci_path(hd, n)) else None
    check(have(hd) == "v2.15", "2.14 (as EUPDFB.PRG) took 2.15")
    check(rd("eupdfb.prg") == cf_prg, "EUPDFB.PRG is 2.15's FireBee build now (the bridge)")
    check(rd("tlsfb.ovl") == tls, "TLSFB.OVL beside it")
    check(rd("eupdate.prg") == std_prg, "EUPDATE.PRG the standard one, its zip unpacked last")

    # 2. 2.15 meets 2.16: the FireBee's zip alone; EUPDFB.PRG and TLSFB.OVL go
    cache = os.path.join(work, "c216")
    release(cache, "v2.16", {"eupdate.prg": std_prg, "tls.ovl": tls}, {"eupdate.prg": cf_prg, "tls.ovl": tls16})
    srv = serve(cache)
    try:
        before, r, m = run(hd, "v2.16", "v216")
        try:
            r.key(rig.K["RETURN"])
            time.sleep(3)
            from PIL import Image, ImageChops
            x, y = Image.open(before).convert("L"), Image.open(r.shot("v216-ok")).convert("L")
            box = (160, 160, 480, 320)
            d = ImageChops.difference(x.crop(box), y.crop(box)).point(lambda v: 255 if v else 0)
            moved = d.histogram()[255] / float((box[2] - box[0]) * (box[3] - box[1]))
            check(moved > 0.05, "an alert (start EUPDATE.PRG from now on) answered (%.2f changed)" % moved)
            r.ctrl("q")
            time.sleep(6)
        finally:
            r.stop()
            m.stop()
    finally:
        for s in srv:
            s.kill()
            s.wait()
    check(have(hd) == "v2.16", "2.15 (as EUPDFB.PRG) took 2.16")
    check(rd("eupdate.prg") == cf_prg and rd("tls.ovl") == tls16,
          "EUPDATE.PRG and TLS.OVL the FireBee's: its zip alone")
    check(rd("eupdfb.prg") is None and rd("tlsfb.ovl") is None, "EUPDFB.PRG and TLSFB.OVL gone")
    log = open(ci_path(hd, "eupdate.log"), errors="replace").read().lower()
    check("eupdfb.prg replaced by eupdate.prg" in log and "tlsfb.ovl replaced by tls.ovl" in log,
          "and the log says so")
    check("downloading eupdate-2.16-firebee.zip" in log and "downloading eupdate-2.16.zip" not in log,
          "2.16's FireBee zip downloaded, its standard one not")
    check("downloading eupdate-2.15-firebee.zip" in log and "downloading eupdate-2.15.zip" in log,
          "and 2.14 had taken both of 2.15's")
    check(not os.path.exists(ci_path(hd, "crash.log")), "no CRASH.LOG")
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
