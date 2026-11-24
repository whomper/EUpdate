#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""objpad.py: the shipped build's functions, tables and variables at a
release's places, modulo the 68030's 256-byte caches, with pads between
the objects.

  objpad.py step BASE.map NEW.map PADS [NAME]  puts the first object out of
        place right in PADS (made if missing); 1 when none is: link
        again after each step (an alignment fill makes one pass wrong)
  objpad.py check BASE.map NEW.map             fails where a section
        both maps hold sits elsewhere mod 256
  objpad.py asm PADS OBJ > pad_OBJ.S           the pad that goes before
        OBJ.o in the link

Why (17 Nov 2026, 2.08): the MiNTnet fix grew net_tos.o by 450 bytes and
everything after it moved; a Falcon's start came out 10 ms slower in both
pairs with no work added at the start (the hub's rule, AERA-Atari
098183d: code-placement rows get a pad round before they go to the owner).
atari/pads.txt was made with it against v2.07's map. The speed rig's
timed build is put at the shipped build's places by common's
tools/profpad.py instead (make prof).

EPlay's tools/mkpad.py pads after each section and sorts the link by
name; EUpdate's link keeps the objects' order (common/atari/link.ld, the
link order of 2.05), so a pad is an object of its own, put before the
object whose sections it moves: its .text before that object's code, its
.rodata before its constants, its .data and .bss likewise. A pad inside
an object can't be: where an object's own sections changed, the sections
after them in it stay moved, and the next object is put back. The strings
(.rodata.str1.1) are merged into the first object that has any, so a
string added anywhere moves the constants after that object, and the pad
goes there.

PADS: a line each, "OBJ KIND BYTES" (KIND text, rodata, data or bss)."""
import os
import re
import sys

KINDS = (".text", ".data", ".bss")


def load(path):
    """the map's sections: (name, address, size, object), in the link's
    order (from EPlay's tools/mkpad.py), the pads left out"""
    out, pend, osec = [], None, None
    for line in open(path):
        m = re.match(r"^(\.\S+)", line)
        if m:
            osec = m.group(1) if m.group(1) in KINDS else None
            continue
        if osec is None:
            continue
        m = re.match(r"^ (\.\S+)\s*$", line)
        if m:
            pend = m.group(1)
            continue
        m = re.match(r"^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)", line)
        if m:
            out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4).split("/")[-1]))
            pend = None
            continue
        m = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)", line)
        if m and pend:
            out.append((pend, int(m.group(1), 16), int(m.group(2), 16), m.group(3).split("/")[-1]))
        pend = None
    # the merged strings: the first object's section holds them all, and
    # the map lists the others, merged away, at one address after it
    seen, keep = set(), []
    for o in out:
        m = re.search(r"\.str[0-9]+\.[0-9]+$", o[0])
        if m:
            if m.group(0) in seen:
                continue
            seen.add(m.group(0))
        if o[2] > 0 and not o[3].startswith("pad_"):
            keep.append(o)
    return keep


def kind(s):
    for k in ("rodata", "data", "bss"):
        if s == "." + k or s.startswith("." + k + "."):
            return k
    return "text"


def read_pads(path):
    p = {}
    for line in open(path):
        f = line.split()
        if len(f) == 3 and not line.startswith("#"):
            p[(f[0], f[1])] = int(f[2])
    return p


def obj(o):
    return o[:-2] if o.endswith(".o") else o


def first_off(base, new):
    """the first object whose sections of a kind start elsewhere mod 256:
    ((object, kind), how far)"""
    old = {(s, o): ad for s, ad, sz, o in base}
    prev = None
    for s, ad, sz, o in new:
        key = (obj(o), kind(s))
        if ad and (s, o) in old and key != prev:
            d = (ad - old[(s, o)]) % 256
            if d:
                return key, d
        prev = key
    return None, 0


def check(base, new):
    old = {(s, o): ad for s, ad, sz, o in base}
    return [(s, o, (ad - old[(s, o)]) % 256) for s, ad, sz, o in new
            if ad and (s, o) in old and (ad - old[(s, o)]) % 256]


def main():
    a = sys.argv[1:]
    if len(a) in (4, 5) and a[0] == "step":
        cur = read_pads(a[3]) if os.path.exists(a[3]) else {}
        key, d = first_off(load(a[1]), load(a[2]))
        if not key:
            return 1
        cur[key] = (cur.get(key, 0) - d) % 256
        with open(a[3], "w") as f:
            f.write("# written by tools/objpad.py against %s: do not edit\n" % (a[4] if len(a) > 4 else a[1]))
            f.write("# %d pads, %d bytes\n" % (sum(1 for v in cur.values() if v), sum(cur.values())))
            for (o, k), x in sorted(cur.items()):
                if x:
                    f.write("%s %s %d\n" % (o, k, x))
        print("objpad: %s.o's %s %+d" % (key[0], key[1], -d))
        return 0
    if len(a) == 3 and a[0] == "check":
        off = check(load(a[1]), load(a[2]))
        if off:
            objs = []
            for s, o, d in off:
                if o not in objs:
                    objs.append(o)
            print("objpad: %d sections of %s not at %s's places mod 256, in %s"
                  % (len(off), a[2], a[1], " ".join(objs)))
            for s, o, d in off[:8]:
                print("  %s (%s) %+d" % (s, o, d))
            return 1
        print("objpad: every section of %s at %s's place mod 256" % (a[2], a[1]))
        return 0
    if len(a) == 3 and a[0] == "asm":
        p = read_pads(a[1])
        print("| SPDX-License-Identifier: GPL-2.0-or-later")
        print("| Copyright (C) 2026 AERA | Erez Yaary")
        print("| written by tools/objpad.py from %s: the pad before %s.o" % (a[1], a[2]))
        for k, flags in (("text", '"ax"'), ("rodata", '"a"'), ("data", '"aw"'), ("bss", '"aw",@nobits')):
            x = p.get((a[2], k), 0)
            if x:
                print('\t.section ".%s.eupad_%s",%s' % (k, a[2], flags))
                print("\t.p2align 0")
                print("\t.globl eu_pad_%s_%s" % (a[2], k))
                print("eu_pad_%s_%s:" % (a[2], k))
                print("\t.space %d" % x)
        return 0
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
