#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUpdate started beside a program resident from boot (a FireBee tester's,
21 Nov 2026: with HypView open after the boot, EUpdate crashed the system;
with HypView closed, or after EWeb had run, it worked). Under FreeMiNT and
XaAES through common's tests/xaaes/run.py: HOLD.PRG (hold.c, a help viewer's
stand-in) starts at boot, takes 416 KB in four blocks and opens a window;
EUpdate starts 20 s later and checks at its start, with MiNTnet's sockets
(--inet) so that its job begins and TLS.OVL is loaded and relocated
(common's ovl.c, whose cache push after the load was the suspected cause:
AERA-Atari aa30896); the check runs to its end against 127.0.0.1, which
answers nothing. Then: TLS.OVL loaded, the check ended, no CRASH.LOG, no
memory violation, no panic.

    resident_test.py WORK [plain|ct60|ct60-prot]

plain: a Falcon's 68030 with FreeMiNT's memory protection on (a stray
write is caught there); ct60: the 68060, protection off, as FreeMiNT runs
on a FireBee; ct60-prot: the 68060 with it on. What it can't be: the
FireBee's ColdFire running EUPDFB.PRG and TLSFB.OVL, FireTOS, HypView
itself (the tester's run of the release is that). Needs make base in
common/tests/xaaes, as xaaes_test.py.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.normpath(os.path.join(HERE, "..", ".."))
XA = os.path.join(TOP, "common", "tests", "xaaes")
sys.path.insert(0, os.path.join(TOP, "common", "tools"))
import machines  # noqa: E402

work = os.path.abspath(sys.argv[1])
how = sys.argv[2] if len(sys.argv) > 2 else "plain"
fails = 0
FILES = ("eupdate.prg", "tls.ovl", "tlsfb.ovl", "cacert.pem", "roots.dat")
# the check at the start (CHECK= left out), at an address that answers
# nothing; https, so the TLS is what it waits on
INF = ("[OPTIONS]\r\nNATFEATS=1\r\nLOG=1\r\nAPIBASE=https://127.0.0.1:1\r\nFOLDER=C:\\aera\r\n"
       "[REPO]\r\nREPO=whomper/ECal\r\nFOLDER=C:\\aera\\ecal\r\nUNZIP=1\r\nRULE=*.ACC;C:\\\r\n"
       "[REPO]\r\nREPO=whomper/EUpdate\r\nFOLDER=.\r\n")
HOLD_KB = "120 64 200 32\r\n"


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


def hold_prg(out):
    """HOLD.PRG from hold.c, on the objects of common's xaaes rig (make)"""
    b = os.path.join(XA, "build")
    subprocess.run(["make", "-s", "-C", XA], check=True)
    cc = "m68k-linux-gnu-gcc"
    inc = subprocess.run([cc, "-print-file-name=include"], capture_output=True, text=True).stdout.strip()
    a = os.path.join(TOP, "common", "atari")
    obj, elf = os.path.join(work, "hold.o"), os.path.join(work, "hold.elf")
    subprocess.run([cc, "-m68000", "-Os", "-fomit-frame-pointer", "-ffreestanding", "-fno-builtin", "-fno-pic",
                    "-fno-pie", "-fno-tree-loop-distribute-patterns", "-fno-asynchronous-unwind-tables", "-Wall",
                    "-Werror", "-nostdinc", "-isystem", inc, "-I" + os.path.join(a, "include"), "-I" + a,
                    "-c", "-o", obj, os.path.join(HERE, "hold.c")], check=True)
    subprocess.run([cc, "-m68000", "-nostdlib", "-static", "-no-pie", "-Wl,--emit-relocs",
                    "-Wl,-T," + os.path.join(a, "link.ld"), "-Wl,--build-id=none", "-Wl,-z,noexecstack",
                    "-Wl,--no-warn-rwx-segments", "-o", elf, os.path.join(b, "crt0.o"), obj,
                    os.path.join(b, "gem.o"), os.path.join(b, "tos.o"), os.path.join(b, "libc.o"),
                    os.path.join(b, "nf.o"), "-lgcc"], check=True)
    subprocess.run([sys.executable, os.path.join(TOP, "common", "tools", "elf2tos.py"), elf, out],
                   check=True, capture_output=True)


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    rel = os.path.join(work, "release")
    os.makedirs(rel)
    for f in FILES:
        shutil.copy(os.path.join(TOP, "binary", f), rel)
    with open(os.path.join(rel, "eupdate.inf"), "w", newline="") as f:
        f.write(INF)
    hold_prg(os.path.join(rel, "HOLD.PRG"))
    with open(os.path.join(rel, "HOLD.TXT"), "w", newline="") as f:
        f.write(HOLD_KB)
    args = [sys.executable, os.path.join(XA, "run.py"), os.path.join(work, "w"), machines.emutos(), "--nvdi",
            "--inet", os.path.join(rel, "eupdate.prg"),
            "--copy"] + [os.path.join(rel, f) for f in FILES[1:] + ("eupdate.inf", "HOLD.PRG", "HOLD.TXT")]
    args += ["--cnf", "run c:\\HOLD.PRG", "--later", "20"]
    if how.startswith("ct60"):
        args += ["--machine", "ct60"]
    if how != "ct60":
        args += ["--mem-prot"]
    # the check: two repositories, each tried twice, 20 s a try unanswered
    args += ["--wait", "150", "--steps", "shot; wait 20; shot"]
    print("on", "FreeMiNT 1.17.0 + XaAES with Erez's NVDI, EmuTOS 1.4 under it,",
          {"plain": "a Falcon's 68030, memory protection on",
           "ct60": "a 68060 (a CT60's stand-in), memory protection off, as on a FireBee",
           "ct60-prot": "a 68060 (a CT60's stand-in), memory protection on"}[how],
          "; HOLD.PRG resident from boot", flush=True)
    r = subprocess.run(args, capture_output=True, text=True, timeout=1200)
    print(r.stdout[-600:], r.stderr[-800:], flush=True)
    check(r.returncode == 0, "the rig's steps ran")
    hl = os.path.join(work, "w", "hatari.log")
    htext = open(hl, errors="replace").read() if os.path.exists(hl) else ""
    check("HOLD holds its memory" in htext, "HOLD.PRG resident, its memory taken, before EUpdate started")
    check("LATER started it" in htext, "EUpdate started beside it")
    out = os.path.join(work, "w", "out")
    log = find(out, "eupdate.log")
    text = open(log, errors="replace").read() if log else ""
    check("TLS.OVL" in text and " loaded, " in text, "TLS.OVL loaded and relocated, the caches pushed")
    check("whomper/EUpdate failed" in text, "the check ran to its end (nothing answers at 127.0.0.1)")
    check(find(out, "crash.log") is None, "no CRASH.LOG")
    # (a stray write under protection: MiNT kills the process and says so)
    check("Panic" not in htext and "Exception number" not in htext and "MEMORY VIOLATION" not in htext.upper(),
          "no memory violation, unhandled exception or panic in Hatari's log")
    if fails and text:
        print(text[-1500:])
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
