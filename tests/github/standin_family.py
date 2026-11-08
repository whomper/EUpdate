#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""Stand-in releases of the AERA family, for tests/family_server.py: no
other AERA program is fetched or run for EUpdate's tests (Erez, 8 Nov 2026;
the guidelines 1.2). Each is made here, as each app's make dist names its
files (lowercase, rule 6.6), with sizes like the real ones', so a download
takes as long as Erez's: CACHE/<Repo>/<tag>/<file>, and CACHE/<Repo>/TAG.
The files hold made-up bytes, nothing of the programs.

    standin_family.py CACHE

The real programs together are the hub's family checks (AERA-Atari,
common/tests/family/acc_all.py).
"""
import os
import random
import sys
import zipfile

# repo: (tag, [(name in the zip, size in KB, packs?)]); the release carries
# the zip and each file beside it, as GitHub has them
FAMILY = {
    "ECal": ("v9.1", [("ecal.acc", 300, 1), ("ecalst.acc", 180, 1), ("ecal.prg", 300, 1), ("ecal.hyp", 40, 1),
                      ("ecal.ref", 1, 1), ("ecal.rsc", 5, 1), ("cacert.pem", 182, 1), ("roots.dat", 52, 0),
                      ("tls.ovl", 138, 1)]),
    "EContact": ("v9.1", [("econtact.acc", 260, 1), ("econtst.acc", 160, 1), ("econtact.prg", 260, 1),
                          ("econtact.hyp", 30, 1), ("econtact.def", 2, 1), ("cacert.pem", 182, 1),
                          ("roots.dat", 52, 0), ("tls.ovl", 138, 1)]),
    "EMail": ("v9.1", [("email.prg", 520, 1), ("emailst.prg", 400, 1), ("email.hyp", 40, 1), ("email.rsc", 5, 1),
                       ("cacert.pem", 182, 1), ("roots.dat", 52, 0), ("tls.ovl", 138, 1)]),
    # one program and TLS.OVL, no ST edition (the guidelines 7.4)
    "EFTP": ("v9.1", [("eftp.prg", 420, 1), ("eftp.hyp", 30, 1), ("eftp.rsc", 5, 1), ("cacert.pem", 182, 1),
                      ("roots.dat", 52, 0), ("tls.ovl", 138, 1)]),
    "EHypView": ("v9.1", [("ehypview.prg", 200, 1), ("ehypview.acc", 200, 1), ("ehypview.hyp", 25, 1),
                          ("ehypview.rsc", 5, 1)]),
    "ClaudeST": ("v9.1", [("claudest.prg", 170, 1), ("claudest.hyp", 20, 1), ("icons/claudest.rsc", 5, 1),
                          ("icons/claudest.icn", 1, 1)]),
    # the largest: a 2.7 MB font that doesn't pack, 4.4 MB in all (eweb-0.74)
    "EWeb": ("v9.1", [("eweb.prg", 1165, 1), ("ewebst.prg", 712, 1), ("eweb.fnt", 2718, 0), ("eweb.hyp", 29, 1),
                      ("ewebjs.ovl", 905, 1), ("icons/eweb.rsc", 5, 1), ("icons/eweb.icn", 1, 1),
                      ("cacert.pem", 182, 1), ("roots.dat", 52, 0), ("tls.ovl", 138, 1)]),
    "EPDF": ("v9.1", [("epdf.prg", 900, 1), ("epdf.hyp", 30, 1), ("notices.txt", 9, 1),
                      ("econvert/econvert.prg", 815, 1), ("econvert/fonts/sans.ttf", 400, 0)]),
    "EUpdate": ("v9.1", [("eupdate.prg", 230, 1), ("eupdate.hyp", 25, 1), ("eupdate.rsc", 5, 1),
                         ("cacert.pem", 182, 1), ("roots.dat", 52, 0), ("tls.ovl", 138, 1)]),
}


def data(rnd, kb, packs):
    """made-up bytes: text-like ones pack about as a program does (to half),
    random ones not at all (a font, the roots)"""
    n = kb * 1024
    if not packs:
        return rnd.randbytes(n)
    words = [rnd.randbytes(rnd.randint(2, 9)) for _ in range(4096)]
    out = bytearray()
    while len(out) < n:
        out += words[rnd.randrange(len(words))]
    return bytes(out[:n])


def make(cache):
    for repo, (tag, files) in FAMILY.items():
        d = os.path.join(cache, repo, tag)
        os.makedirs(d, exist_ok=True)
        rnd = random.Random(repo)         # the same bytes each time
        z = os.path.join(d, "%s-%s.zip" % (repo.lower(), tag[1:]))
        with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
            for name, kb, packs in files:
                b = data(rnd, kb, packs)
                zf.writestr(name, b, zipfile.ZIP_DEFLATED if packs else zipfile.ZIP_STORED)
                if "/" not in name:       # beside the zip too, as each release has them
                    with open(os.path.join(d, name), "wb") as f:
                        f.write(b)
        with open(os.path.join(cache, repo, "TAG"), "w") as f:
            f.write(tag + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    make(sys.argv[1])
