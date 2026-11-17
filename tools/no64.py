#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""no64.py PROGRAM.elf: fails where the program's code holds a 64-bit
mulu.l/muls.l or divu.l/divs.l (Dh:Dl), which a 68060 and a ColdFire
lack (exception 61 on the FireBee stand-in: 2.08 stopped there at its
first log line, 17 Nov 2026). GCC's -m68030 divides by a constant with
one; the Makefile tunes the objects that had them for the 68020-60
(TUNE64). Run after each link: a new one names its function here.

It reads the code, not the read-only data after it (the map's
"*(.rodata .rodata.*)" line, PROGRAM.elf.map), a word at a time: an
opcode 0x4C00-0x4C7F (not An direct) with an extension word whose size
bit (0x0400) is set."""
import bisect
import re
import struct
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    f = sys.argv[1]
    b = open(f, "rb").read()
    hdr = subprocess.run(["m68k-linux-gnu-objdump", "-h", f], capture_output=True, text=True, check=True).stdout
    m = re.search(r"\.text\s+([0-9a-f]+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)", hdr)
    off = int(m.group(3), 16)
    m = re.search(r"\*\(\.rodata \.rodata\.\*\)\s*\n\s*\S*\s+0x([0-9a-f]+)", open(f + ".map").read())
    if not m:
        sys.exit("no64: no read-only data line in %s.map" % f)
    end = int(m.group(1), 16)
    syms = []
    for ln in subprocess.run(["m68k-linux-gnu-nm", "-n", f], capture_output=True, text=True).stdout.splitlines():
        p = ln.split()
        if len(p) == 3 and p[1] in "tT":
            syms.append((int(p[0], 16), p[2]))
    at = [a for a, _ in syms]
    t = b[off:off + end]
    found = []
    for i in range(0, len(t) - 4, 2):
        op, ext = struct.unpack(">HH", t[i:i + 4])
        if 0x4C00 <= op <= 0x4C7F and (op & 0x38) != 0x08 and not ext & 0x8000 and not ext & 0x03F8 \
                and ext & 0x0400:
            k = bisect.bisect_right(at, i) - 1
            found.append("%s+%#x" % (syms[k][1], i - syms[k][0]) if k >= 0 else hex(i))
    if found:
        print("no64: %d 64-bit multiplies or divides a 68060 and a ColdFire lack: %s (add the object to TUNE64)"
              % (len(found), ", ".join(found[:12])))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
