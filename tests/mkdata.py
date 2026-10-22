#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The unit tests' archives, the same bytes every time (tests/data/):

  good.zip   a release's zip: a stored file, a deflated one, a folder
  evil.zip   a file whose name climbs out of the folder ("../EVIL.PRG")
  bad.zip    good.zip with one byte of a deflated file changed (CRC)
  hello.gz   "hello, Atari\n" * 50, gzip'ed (an HTTP answer)
"""
import gzip
import io
import os
import zipfile

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
T = (2026, 10, 22, 12, 0, 0)


def zip_of(entries):
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w") as z:
        for name, data, method in entries:
            i = zipfile.ZipInfo(name, T)
            i.compress_type = method
            i.external_attr = (0o40755 << 16) | 0x10 if name.endswith("/") else 0o644 << 16
            z.writestr(i, data)
    return b.getvalue()


prg = bytes(range(256)) * 40
text = b"EUpdate's test file.\r\n" * 200
good = zip_of([("EUPDATE.PRG", prg, zipfile.ZIP_STORED),
               ("icons/", b"", zipfile.ZIP_STORED),
               ("icons/README.TXT", text, zipfile.ZIP_DEFLATED)])
open(os.path.join(D, "good.zip"), "wb").write(good)
open(os.path.join(D, "evil.zip"), "wb").write(zip_of([("../EVIL.PRG", b"x", zipfile.ZIP_STORED)]))
bad = bytearray(good)
at = bad.index(b"icons/README.TXT") - 30     # the deflated file's local header
bad[at + 30 + len("icons/README.TXT") + 5] ^= 0x55
open(os.path.join(D, "bad.zip"), "wb").write(bytes(bad))
open(os.path.join(D, "hello.gz"), "wb").write(gzip.compress(b"hello, Atari\n" * 50, mtime=0))
