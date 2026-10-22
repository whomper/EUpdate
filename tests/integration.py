#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""make itest: eupd-cli (EUpdate's core on this computer) against
tests/server.py, once over HTTP and once over TLS with a certificate
authority of the test's own: finding repositories and owners, an
owner's list, the latest release, no release, no repository, the hour's
limit, a wrong token, and a download unpacked into a folder."""
import os
import shutil
import subprocess
import sys
import time

H = os.path.dirname(os.path.abspath(__file__))
CLI = os.path.abspath(sys.argv[1])
W = os.path.join(H, "..", "build", "itest")
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    fails += not cond


def cli(base, ca, *args, token=None):
    cmd = [CLI, "-b", base, "-c", ca, "-l", os.path.join(W, "EUPDATE.LOG")]
    if token:
        cmd += ["-t", token]
    r = subprocess.run(cmd + list(args), capture_output=True, text=True, timeout=60)
    return r.returncode, r.stdout, r.stderr


def certs():
    def ossl(*a):
        subprocess.run(["openssl"] + list(a), check=True, capture_output=True)
    ossl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=EUpdate test CA",
         "-keyout", "ca.key", "-out", "ca.pem", "-addext", "basicConstraints=critical,CA:TRUE",
         "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    ossl("req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost", "-keyout", "srv.key", "-out", "srv.csr")
    open("ext.cnf", "w").write("subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\n")
    ossl("x509", "-req", "-in", "srv.csr", "-CA", "ca.pem", "-CAkey", "ca.key", "-CAcreateserial",
         "-days", "2", "-out", "srv.pem", "-extfile", "ext.cnf")


def run(base, ca):
    print("--", base)
    rc, out, err = cli(base, ca, "repos", "eftp")
    check(rc == 0 and out.startswith("whomper/EFTP\t12\t"), "repositories by name")
    check("whomper/EFTP_XL\t3" in out and "someone/eftp-tools" in out, "all three, a long description cut")
    rc, out, err = cli(base, ca, "owners", "whomper")
    check(rc == 0 and "whomper\tperson" in out and "atari-org\torganisation" in out, "owners")
    rc, out, err = cli(base, ca, "owner", "someone")
    check(rc == 0 and out.count("someone/R") == 40, "an owner's 40 repositories, chunked and gzip'ed")
    rc, out, err = cli(base, ca, "owner", "nobody")
    check(rc == 1 and "no one called nobody" in err, "no such owner")
    rc, out, err = cli(base, ca, "latest", "whomper/EUpdate")
    check(rc == 0 and out.startswith("v1.1\t2026-10-21\tEUpdate 1.1") and "EUpdate-1.1.zip" in out, "the latest release")
    rc, out, err = cli(base, ca, "latest", "whomper/EDOS")
    check(rc == 0 and "no release yet" in out, "no release")
    rc, out, err = cli(base, ca, "latest", "whomper/Nothing")
    check(rc == 0 and "no such repository" in out, "no repository")
    rc, out, err = cli(base, ca, "latest", "whomper/Limit")
    check(rc == 1 and "limit" in err, "the hour's limit")
    rc, out, err = cli(base, ca, "latest", "whomper/EUpdate", token="bad-token")
    check(rc == 1 and "token" in err, "a wrong token")
    dest = os.path.join(W, "dest")
    shutil.rmtree(dest, ignore_errors=True)
    rc, out, err = cli(base, ca, "get", "whomper/EUpdate", dest, token="good-token")
    check(rc == 0 and "have v1.1" in out, "downloaded with a token (not sent on to the file's host)")
    check(sorted(os.listdir(dest)) == ["EUPDATE.PRG", "ICONS"] and
          os.path.getsize(os.path.join(dest, "ICONS", "README.TXT")) == 4400, "the zip unpacked, 8+3")
    rc, out, err = cli(base, ca, "get", "whomper/EUpdate", dest, "*.HYP")
    check(rc == 0 and os.path.getsize(os.path.join(dest, "EUPDATE.HYP")) == 5124, "the files a pattern names")


def main():
    shutil.rmtree(W, ignore_errors=True)
    os.makedirs(W)
    os.chdir(W)
    certs()
    plain = subprocess.Popen([sys.executable, os.path.join(H, "server.py"), "18731"], stdout=subprocess.PIPE)
    tls = subprocess.Popen([sys.executable, os.path.join(H, "server.py"), "18732", "srv.pem", "srv.key"],
                           stdout=subprocess.PIPE)
    try:
        plain.stdout.readline()
        tls.stdout.readline()
        run("http://127.0.0.1:18731", os.path.join(W, "ca.pem"))
        run("https://localhost:18732", os.path.join(W, "ca.pem"))
        log = open(os.path.join(W, "EUPDATE.LOG"), errors="replace").read()
        check("good-token" not in log, "the token is not in the log")
    finally:
        plain.kill()
        tls.kill()
    print("%d failed" % fails)
    sys.exit(fails != 0)


if __name__ == "__main__":
    main()
