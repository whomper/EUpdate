#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""EUPDATE.PRG updating the AERA family as on Erez's Falcon, on its own:
stand-in releases of the family (tests/github/standin_family.py: each
app's file names, sizes like the real ones', made-up bytes), served by
tests/family_server.py as GitHub serves them (TLS with a chain like
GitHub's, the files from a second host at signed addresses, idle
connections closed), the network at a NetUSBee's pace through FAKESTNG,
and the rules EUpdate suggests: the ST editions left out, the accessories
into C:\\. The check at the start downloads all that is new (FETCH=1).
No other AERA program is fetched or loaded (Erez, 8 Nov 2026; the
guidelines 1.2): the real ones together, as accessories too, are the
hub's family checks (AERA-Atari, common/tests/family/acc_all.py).

    family_test.py WORK [MACHINE]      (MACHINE: Falcon:16; Xvfb, xdotool)

Erez's TOS 4.04 and NVDI are used when this machine has them
(machines.py: ~/.aera/tos, AERA_WITH_NVDI=1); EmuTOS otherwise.
"""
import os
import socket
import shutil
import subprocess
import sys
import time

import rig
from machines import ci_path      # EUpdate writes lowercase names (6.6); Hatari keeps the case
from gui_test import wait_port, check, TOP

work = os.path.abspath(sys.argv[1])
spec = sys.argv[2] if len(sys.argv) > 2 else "Falcon:16"
PORT = 18760
FILES_HOST = "127.0.0.2"      # GitHub's files are on a host of their own
# Erez's list (22 Oct 2026); ECal and EContact current, the others not yet downloaded
FOLLOW = (("ECal", True), ("EContact", True), ("EUpdate", False), ("ClaudeST", False), ("EHypView", False),
          ("EWeb", False), ("EMail", False), ("EFTP", False),
          ("EPDF", False))
# the rules EUpdate suggests for them on a Falcon (update.c, up_suggest)
SKIP = {"ECal": "ECALST.ACC", "EContact": "ECONTST.ACC", "EMail": "EMAILST.PRG", "EFTP": "EFTPST.PRG",
        "EWeb": "EWEBST.PRG"}
ACCS = ("ECal", "EContact", "EHypView")   # their .ACC into C:\ by a rule


def certs():
    """a chain like GitHub's: root, an RSA-4096 intermediate, the leaf"""
    def ossl(*a):
        subprocess.run(["openssl"] + list(a), check=True, capture_output=True, cwd=work)
    open(os.path.join(work, "ca.cnf"), "w").write("basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n")
    open(os.path.join(work, "leaf.cnf"), "w").write("subjectAltName=DNS:127.0.0.1,IP:127.0.0.1,DNS:127.0.0.2,IP:127.0.0.2\nbasicConstraints=CA:FALSE\n")
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
    # and an ECDSA P-256 leaf, as GitHub has both: which one a hello gets
    # shows in the log (Erez's stock Falcon, 1 Nov 2026: GitHub's ECDSA
    # signature took its DSP 5.5 s, and every handshake ran out of time)
    ossl("ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", "ecleaf.key")
    ossl("req", "-new", "-key", "ecleaf.key", "-subj", "/CN=127.0.0.1", "-out", "ecleaf.csr")
    ossl("x509", "-req", "-in", "ecleaf.csr", "-CA", "int.pem", "-CAkey", "int.key", "-CAcreateserial", "-days", "2",
         "-out", "ecleaf.pem", "-extfile", "leaf.cnf")
    with open(os.path.join(work, "ecchain.pem"), "w") as f:
        f.write(open(os.path.join(work, "ecleaf.pem")).read() + open(os.path.join(work, "int.pem")).read())


def port_taken(port):
    s = socket.socket()
    try:
        return s.connect_ex(("127.0.0.1", port)) == 0
    finally:
        s.close()


def main():
    # another family_test's servers on these ports would answer with its
    # own CA's chain: every check then fails on the certificate
    taken = [p for p in (PORT, PORT + 1) if port_taken(p)]
    if taken:
        sys.exit("family_test: %s %s in use (another family_test running?)" %
                 ("ports" if len(taken) > 1 else "port", " and ".join(map(str, taken))))
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    cache = os.path.join(work, "releases")
    subprocess.run([sys.executable, os.path.join(TOP, "tests", "github", "standin_family.py"), cache], check=True)
    certs()
    hd = os.path.join(work, "hd")
    os.makedirs(hd)
    shutil.copy(os.environ.get("PRG", os.path.join(TOP, "binary", "eupdate.prg")), os.path.join(hd, "eupdate.prg"))
    shutil.copy(os.path.join(TOP, "binary", "tls.ovl"), os.path.join(hd, "tls.ovl"))
    shutil.copy(os.path.join(work, "root.pem"), os.path.join(hd, "cacert.pem"))
    tags = {r: open(os.path.join(cache, r, "TAG")).read().strip() for r, _ in FOLLOW}
    # an ST edition copied by hand beside its program (the guidelines 7.4):
    # replaced once the release has TLS.OVL and no ST edition
    os.makedirs(os.path.join(hd, "NET", "EFTP"))
    with open(os.path.join(hd, "NET", "EFTP", "EFTPST.PRG"), "wb") as f:
        f.write(b"by hand")
    eftp = [n.upper() for n in os.listdir(os.path.join(cache, "EFTP", tags["EFTP"]))]  # any case (6.6)
    one_for_all = "TLS.OVL" in eftp and "EFTPST.PRG" not in eftp
    with open(os.path.join(hd, "eupdate.inf"), "w", newline="") as f:
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
                             cache, chain, key, os.path.join(work, "ecchain.pem"), os.path.join(work, "ecleaf.key")],
                            stdout=subprocess.DEVNULL, env=dict(os.environ, FILES_HOST=FILES_HOST))
           for port in (PORT, PORT + 1)]
    wait_port(PORT)
    wait_port(PORT + 1, FILES_HOST)
    os.environ["AERA_WITH_ACCS"] = ""         # EUpdate alone: no AERA accessory beside it (1.2)
    m = rig.machine(spec, work)
    r = rig.Rig(work, m, display=os.environ.get("XDISPLAY", ":99"),
                extra=m.network(rate=int(os.environ.get("RATE", "100000"))))   # a NetUSBee's pace
    print("on", m.describe(), flush=True)
    try:
        inf = ""
        for i in range(int(os.environ.get("WAIT", "150"))):
            time.sleep(10)
            inf = open(ci_path(hd, "eupdate.inf"), errors="replace").read()   # lines end "\n" here
            if inf.count("HAVE=v") == len(FOLLOW):
                break
        r.shot("end")
        have = {}
        for part in inf.split("[REPO]")[1:]:
            lines = dict(l.split("=", 1) for l in part.split("\n") if "=" in l and not l.startswith(";"))
            have[lines.get("REPO", "")] = lines.get("HAVE", "")
        for repo, _ in FOLLOW:
            check(have.get("whomper/" + repo) == tags[repo], "%s %s downloaded" % (repo, tags[repo]))
        check(not os.path.exists(ci_path(hd, "NET/EMAIL/emailst.prg")), "the ST editions left out")
        check(os.path.exists(ci_path(hd, "sigs.dat")), "the checked signatures kept in SIGS.DAT")
        check(os.path.exists(ci_path(hd, "ehypview.acc")) and
              os.path.getsize(ci_path(hd, "ehypview.acc")) ==
              os.path.getsize(ci_path(os.path.join(cache, "EHypView", tags["EHypView"]), "ehypview.acc")),
              "EHYPVIEW.ACC into C:\\, by its rule")
        st = os.path.exists(ci_path(hd, "NET/EFTP/eftpst.prg"))
        if one_for_all:
            check(os.path.exists(ci_path(hd, "NET/EFTP/tls.ovl")) and
                  os.path.getsize(ci_path(hd, "NET/EFTP/tls.ovl")) ==
                  os.path.getsize(ci_path(os.path.join(cache, "EFTP", tags["EFTP"]), "tls.ovl")),
                  "EFTP %s: TLS.OVL beside EFTP.PRG" % tags["EFTP"])
            check(not st, "EFTP %s: the hand-copied EFTPST.PRG replaced" % tags["EFTP"])
            log = open(ci_path(hd, "eupdate.log"), errors="replace").read()
            check("eftpst.prg replaced by eftp.prg" in log.lower(), "and the log says so")
        else:
            check(st, "EFTP %s still has its ST edition: the hand-copied one kept" % tags["EFTP"])
        # the hello asks for RSA certificates, DSP or not: GitHub's ECDSA
        # signature took a stock Falcon's DSP 5.5 s (common's conn.c)
        log = open(ci_path(hd, "eupdate.log"), errors="replace").read()
        check("RSA-2048 signature" in log and "ECDSA" not in log,
              "GitHub's RSA certificate asked for, not its ECDSA one")
    finally:
        r.stop()
        m.stop()
        for s in srv:
            s.kill()
    print("%d failed" % __import__("gui_test").fails)
    sys.exit(__import__("gui_test").fails != 0)


if __name__ == "__main__":
    main()
