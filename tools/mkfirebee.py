#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDFB.PRG, the FireBee's EUpdate (ColdFire, -mcpu=5475; Erez, 18 Nov
2026, on a tester's FireBee, where 2.08's and 2.09's 68030 code froze:
on the ColdFire some 68020 instructions are valid and mean something
else, divul.l Dr:Dq read as its remu.l, so FireTOS's emulation never
sees them), made from the rules EUPDATE.PRG is made with, as EWeb's
tools/mkfirebee.py makes EWEBFB.PRG: `make -n -B build/eupdate.elf
PADS=no` gives each command, and each cross-compiler command is run
again for the ColdFire:

- -m68030, -m68000 and -mtune=... become -mcpu=5475;
- build/'s objects and ELF become build/cf/'s (its generated sources stay);
- common's cpucheck.c (68000 code, which a ColdFire may not run) becomes
  cpucheck_cf.S: on a machine without the _CF_ cookie it shows
  ui/main.c's cpu_alert (under __mcoldfire__) and goes;
- -lgcc (68020 code) becomes common's atari/ll64.c, the 64-bit multiply
  and divide in C.

No pads: the 68030's caches aren't the ColdFire's. TLS on the FireBee is
TLSFB.OVL (common's, copied: the Makefile's binary/tlsfb.ovl).

    tools/mkfirebee.py OUT.PRG"""
import os
import shlex
import subprocess
import sys

CC = "m68k-linux-gnu-gcc"
MCPU = "-mcpu=5475"
OUT = "build/cf/"


def run(cmd):
    p = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if p.returncode != 0:
        sys.stderr.write(cmd + "\n" + p.stdout + p.stderr)
        sys.exit("mkfirebee: a command failed")
    return p.stdout


def cpu(words):
    out = []
    for w in words:
        if w in ("-m68030", "-m68000", "-m68020", "-m68020-60") or w.startswith("-mtune="):
            if MCPU not in out:
                out.append(MCPU)
            continue
        out.append(w.replace("build/", OUT, 1) if w.startswith("build/") and w.endswith((".o", ".elf")) else w)
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    target = sys.argv[1]
    lines = run("make -n -B build/eupdate.elf PADS=no").replace("\\\n", " ").splitlines()
    compiles, link = [], None
    for line in lines:
        if not line.startswith(CC + " "):
            continue
        words = shlex.split(line)
        if "-c" in words:
            compiles.append(words)
        elif "-o" in words and words[words.index("-o") + 1] == "build/eupdate.elf":
            link = words
    if link is None:
        sys.exit("mkfirebee: make gave no link of build/eupdate.elf")
    os.makedirs(OUT + "rt", exist_ok=True)
    n = 0
    for words in compiles:
        obj = words[words.index("-o") + 1]
        if "common/atari/cpucheck.c" in words:
            words = [CC, MCPU, "-c", "common/atari/cpucheck_cf.S", "-o", obj]
        words = cpu(words)
        os.makedirs(os.path.dirname(words[words.index("-o") + 1]), exist_ok=True)
        run(" ".join(shlex.quote(w) for w in words))
        n += 1
    run(" ".join([CC, MCPU, "-O2", "-ffreestanding", "-fno-builtin", "-fno-pic", "-fno-pie",
                  "-fno-asynchronous-unwind-tables", "-c", "common/atari/ll64.c", "-o", OUT + "rt/ll64.o"]))
    words = cpu([w for w in link if w != "-lgcc" and not w.startswith("-Wl,-Map")])
    elf = words[words.index("-o") + 1]
    words += [OUT + "rt/ll64.o", "-Wl,-Map," + elf + ".map"]
    run(" ".join(shlex.quote(w) for w in words))
    env = dict(os.environ, PRGFLAGS="7")
    subprocess.run(["python3", "common/tools/elf2tos.py", elf, target], env=env, check=True)
    print("mkfirebee: %s, %d objects, %d bytes" % (target, n, os.path.getsize(target)))


if __name__ == "__main__":
    main()
