#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG updating the AERA family as on Erez's Falcon: the family's
real latest releases (tests/github/fetch_family.py, served by
tests/family_server.py as GitHub serves them: TLS with a chain like
GitHub's, the files from a second host at signed addresses, idle
connections closed), the network at a NetUSBee's pace through FAKESTNG,
ECal, EContact and EHypView loaded as accessories (their latest
releases, common/tools/machines.py), and the rules EUpdate suggests:
the ST editions left out, the accessories into C:\\ over the ones running.
The check at the start downloads all that is new (FETCH=1).

    family_test.py WORK [MACHINE]      (MACHINE: Falcon:16; Xvfb, xdotool)

Erez's TOS 4.04 and NVDI are used when this machine has them
(machines.py: ~/.aera/tos, AERA_WITH_NVDI=1); EmuTOS otherwise.
AERA_WITH_ACCS= (empty) runs it with no accessories, as Erez ran 1.1 when
it crashed on 22 Oct 2026.
"""
import os
import shutil
import subprocess
import sys
import time

import rig
from gui_test import wait_port, check, TOP

work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "Falcon:16"
PORT = 18760
CACHE = os.path.join(os.path.expanduser("~"), ".cache", "aera", "releases")
# Erez's list (22 Oct 2026); ECal and EContact current, the others not yet downloaded
FOLLOW = (("ECal", True), ("EContact", True), ("EUpdate", False), ("ClaudeST", False), ("EHypView", False),
          ("EWeb", False), ("EMail", False), ("EFTP", False))
# the rules EUpdate suggests for them on a Falcon (update.c, up_suggest)
SKIP = {"ECal": "ECALST.ACC", "EContact": "ECONTST.ACC", "EMail": "EMAILST.PRG", "EFTP": "EFTPST.PRG",
        "EWeb": "EWEBST.PRG"}
ACCS = ("ECal", "EContact", "EHypView")


def certs():
    """a chain like GitHub's: root, an RSA-4096 intermediate, the leaf"""
    def ossl(*a):
        subprocess.run(["openssl"] + list(a), check=True, capture_output=True, cwd=work)
    open(os.path.join(work, "ca.cnf"), "w").write("basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n")
    open(os.path.join(work, "leaf.cnf"), "w").write("subjectAltName=DNS:127.0.0.1,IP:127.0.0.1\nbasicConstraints=CA:FALSE\n")
    ossl("req", "-x509", "-newkey", "rsa:4096", "-nodes", "-days", "2", "-subj", "/CN=Test Root", "-keyout", "root.key",
         "-out", "root.pem", "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    ossl("req", "-newkey", "rsa:4096", "-nodes", "-subj", "/CN=Test Intermediate", "-keyout", "int.key", "-out", "int.csr")
    ossl("x509", "-req", "-in", "int.csr", "-CA", "root.pem", "-CAkey", "root.key", "-CAcreateserial", "-days", "2",
         "-out", "int.pem", "-extfile", "ca.cnf")
    ossl("req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=127.0.0.1", "-keyout", "leaf.key", "-out", "leaf.csr")
    ossl("x509", "-req", "-in", "leaf.csr", "-CA", "int.pem", "-CAkey", "int.key", "-CAcreateserial", "-days", "2",
         "-out", "leaf.pem", "-extfile", "leaf.cnf")
    with open(os.path.join(work, "chain.pem"), "w") as f:
        f.write(open(os.path.join(work, "leaf.pem")).read() + open(os.path.join(work, "int.pem")).read())


def main():
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    subprocess.run([sys.executable, os.path.join(TOP, "tests", "github", "fetch_family.py"), CACHE], check=True)
    certs()
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    shutil.copy(os.path.join(TOP, "binary", "EUPDATE.PRG"), hd)
    shutil.copy(os.path.join(work, "root.pem"), os.path.join(hd, "CACERT.PEM"))
    tags = {r: open(os.path.join(CACHE, r, "TAG")).read().strip() for r, _ in FOLLOW}
    with open(os.path.join(hd, "EUPDATE.INF"), "w", newline="") as f:
        f.write("[OPTIONS]\r\nCHECK=1\r\nFETCH=1\r\nLOG=1\r\nAPIBASE=https://127.0.0.1:%d\r\n" % PORT)
        for repo, have in FOLLOW:
            f.write("[REPO]\r\nREPO=whomper/%s\r\nFOLDER=C:\\NET\\%s\r\nUNZIP=1\r\nHAVE=%s\r\n" %
                    (repo, repo.upper()[:8], tags[repo] if have else ""))
            if repo in SKIP:
                f.write("RULE=%s;-\r\n" % SKIP[repo])
            if repo in ACCS:
                f.write("RULE=*.ACC;C:\\\r\n")
    chain, key = os.path.join(work, "chain.pem"), os.path.join(work, "leaf.key")
    srv = [subprocess.Popen([sys.executable, os.path.join(TOP, "tests", "family_server.py"), str(port), str(PORT + 1),
                             CACHE, chain, key], stdout=subprocess.DEVNULL) for port in (PORT, PORT + 1)]
    wait_port(PORT)
    wait_port(PORT + 1)
    os.environ.setdefault("AERA_WITH_ACCS", ",".join(ACCS))
    m = rig.machine(spec, work)
    r = rig.Rig(work, m, extra=m.network(rate=int(os.environ.get("RATE", "100000"))))   # a NetUSBee's pace
    print("on", m.describe(), flush=True)
    try:
        inf = ""
        for i in range(int(os.environ.get("WAIT", "60"))):
            time.sleep(10)
            inf = open(os.path.join(hd, "EUPDATE.INF"), errors="replace").read()   # lines end "\n" here
            if inf.count("HAVE=v") == len(FOLLOW):
                break
        r.shot("end")
        have = {}
        for part in inf.split("[REPO]")[1:]:
            lines = dict(l.split("=", 1) for l in part.split("\n") if "=" in l and not l.startswith(";"))
            have[lines.get("REPO", "")] = lines.get("HAVE", "")
        for repo, _ in FOLLOW:
            check(have.get("whomper/" + repo) == tags[repo], "%s %s downloaded" % (repo, tags[repo]))
        check(not os.path.exists(os.path.join(hd, "NET", "EMAIL", "EMAILST.PRG")), "the ST editions left out")
        check(os.path.exists(os.path.join(hd, "EHYPVIEW.ACC")) and
              os.path.getsize(os.path.join(hd, "EHYPVIEW.ACC")) ==
              os.path.getsize(os.path.join(CACHE, "EHypView", tags["EHypView"], "EHYPVIEW.ACC")),
              "EHYPVIEW.ACC into C:\\, over the one running")
    finally:
        r.stop()
        m.stop()
        for s in srv:
            s.kill()
    print("%d failed" % __import__("gui_test").fails)
    sys.exit(__import__("gui_test").fails != 0)


if __name__ == "__main__":
    main()
