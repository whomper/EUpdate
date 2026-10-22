#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""A GitHub of the tests' own (make itest): the REST API's answers that
EUpdate asks for, shaped as GitHub's (github/repo.json is a real
repository's, as the API gave it), over HTTP or TLS.

  server.py PORT [CERT KEY]

  /search/repositories   items made from github/repo.json; gzip'ed when asked
  /search/users          two owners
  /users/LOGIN/repos     LOGIN's, chunked: whomper's are the AERA family
  /repos/O/N/releases/latest
      whomper/EUpdate    v1.1: EUpdate-1.1.zip (tests/data/good.zip) and EUPDATE.HYP
                         (the family's others: the same files, their own tags)
      whomper/EDOS       no release (404, and the repository is there)
      anything else      404 twice: no such repository
      whomper/Limit      403, the hour's limit
  /repos/O/N/releases/assets/ID  302 to /dl/ID?SIGNED on the same server
      (a query of 900 characters and more, as GitHub's), which gives the
      file to Accept: application/octet-stream and the whole query only
Every request must carry a User-Agent; a token, when one comes, must be
"Bearer good-token" (401 otherwise), and it must never reach /dl/.
"""
import copy
import gzip
import json
import os
import ssl
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

H = os.path.dirname(os.path.abspath(__file__))
REPO = json.load(open(os.path.join(H, "github", "repo.json")))
ZIP = open(os.path.join(H, "data", "good.zip"), "rb").read()
HYP = b"HDOC" + bytes(range(256)) * 20
# where GitHub sends a release's file: its file server, with a signature
# in the query (release-assets.githubusercontent.com, 902 characters for
# EUpdate 1.0's EUPDATE.PRG); anything else is answered 400
SIGNED = "sp=r&sv=2018-11-09&sr=b&spr=https&se=2026-10-22T09%3A00%3A00Z&rscd=attachment&rsct=application" \
         "%2Foctet-stream&skoid=" + "96c2d410-5711-43a1-aedd-ab1947aa7ab0" * 3 + "&sktid=398a6654&skt=2026" \
         "-10-22T08%3A00%3A00Z&ske=2026-10-22T09%3A00%3A00Z&sks=b&skv=2018-11-09&sig=" + "Zm9vYmFy" * 60 + \
         "%3D&jwt=" + "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9" * 4


def repo(full, stars, desc):
    r = copy.deepcopy(REPO)
    r["full_name"] = full
    r["name"] = full.split("/")[1]
    r["stargazers_count"] = stars
    r["description"] = desc
    return r


# whomper's repositories, as the AERA family's READMEs describe them, and
# the latest release of each
FAMILY = [("ECal", "Calendar desk accessory for the Atari: iCloud and CalDAV", "v2.21"),
          ("EContact", "Contacts desk accessory for the Atari: CardDAV", "v1.58"),
          ("EMail", "E-mail for the Atari: IMAP, POP3 and SMTP", "v1.50"),
          ("EFTP", "Two-pane FTP for the Atari: FTPS and SFTP", "v1.45"),
          ("EHypView", "ST-Guide hypertext viewer, program or desk accessory", "v1.16"),
          ("ClaudeST", "A native GEM client for claude.ai", "v1.55"),
          ("EWeb", "A web browser for the Atari Falcon", "v0.24"),
          ("EUpdate", "GitHub releases for the Atari Falcon and TT", "v1.1"),
          ("EDOS", "Disk Utility Package for Atari 8-bit computers", None),
          ("EFTP_XL", "File transfer for Atari 8-bit computers over FujiNet", None),
          ("AERA-Gateway", "The Raspberry Pi gateways for mail, calendars, FTP and Claude", None),
          ("AERA-Atari", "How the AERA Atari programs look and behave, and the code they share", None)]
TAGS = {"whomper/" + n: t for n, _, t in FAMILY if t}


def files_of(full, tag):
    """a release's files: EUpdate's for the tests; the others named as
    AERA's releases name theirs (ECal: ECAL.ACC, ECALST.ACC...)"""
    if full == "whomper/EUpdate":
        return [("EUpdate-1.1.zip", ZIP), ("EUPDATE.HYP", HYP)]
    stem = full.split("/")[1].upper()[:8]
    st = (stem[:6] if len(stem) > 6 else stem) + "ST"
    names = ["%s-%s.zip" % (full.split("/")[1], tag[1:]), stem + ".PRG", st + ".PRG", stem + ".HYP"]
    if full in ("whomper/ECal", "whomper/EContact", "whomper/EHypView"):
        names += [stem + ".ACC", st + ".ACC"]
    return [(n, ZIP if n.endswith(".zip") else HYP) for n in names]


def release(full, tag):
    assets = []
    for i, (name, data) in enumerate(files_of(full, tag)):
        assets.append({"url": "u", "id": 900 + i, "node_id": "n", "name": name, "label": None,
                       "uploader": {"login": "github-actions[bot]", "id": 1, "type": "Bot"},
                       "content_type": "application/zip", "state": "uploaded", "size": len(data),
                       "download_count": 3, "created_at": "2026-10-21T10:00:00Z",
                       "browser_download_url": "https://github.com/x/%s" % name})
    return {"url": "u", "id": 1, "author": {"login": "github-actions[bot]"}, "tag_name": tag,
            "target_commitish": "main", "name": "%s %s" % (full.split("/")[1], tag[1:]), "draft": False, "prerelease": False,
            "created_at": "2026-10-21T09:00:00Z", "published_at": "2026-10-21T09:05:00Z",
            "assets": assets, "body": "What is new: שלום \U0001F680 \"quoted\"\n",
            "reactions": {"+1": 2, "heart": 1}}


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def send(self, code, body, ctype="application/json", chunked=False, extra=()):
        if isinstance(body, (dict, list)):
            body = json.dumps(body).encode()
        gz = "gzip" in self.headers.get("Accept-Encoding", "") and ctype == "application/json"
        if gz:
            body = gzip.compress(body)
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("X-RateLimit-Remaining", "0" if code == 403 else "57")
        for k, v in extra:
            self.send_header(k, v)
        if gz:
            self.send_header("Content-Encoding", "gzip")
        if chunked:
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for i in range(0, len(body), 777):
                part = body[i:i + 777]
                self.wfile.write(b"%x\r\n%s\r\n" % (len(part), part))
            self.wfile.write(b"0\r\n\r\n")
        else:
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    def do_GET(self):
        path, _, query = self.path.partition("?")
        auth = self.headers.get("Authorization")
        if not self.headers.get("User-Agent", "").startswith("EUpdate"):
            return self.send(403, {"message": "no User-Agent"})
        if path.startswith("/dl/"):
            if query != SIGNED:
                return self.send(400, b"<Error>AuthenticationFailed</Error>", "application/xml")
            if auth:
                return self.send(400, {"message": "the token went to the download's host"})
            if self.headers.get("Accept") != "application/octet-stream":
                return self.send(415, {"message": "Accept"})
            data = ZIP if path[4:] == "900" else HYP if path[4:].isdigit() else None
            return self.send(200, data, "application/octet-stream") if data else self.send(404, {})
        if auth and auth != "Bearer good-token":
            return self.send(401, {"message": "Bad credentials"})
        p = path.strip("/").split("/")
        if path == "/search/repositories":
            if "q=" not in query or "in%3Aname" not in query and "in:name" not in query:
                return self.send(422, {"message": "Validation Failed"})
            items = [repo("whomper/EFTP", 12, "FTP for the Atari – FTPS, SFTP"),
                     repo("whomper/EFTP_XL", 3, None), repo("someone/eftp-tools", 0, "x" * 300)]
            return self.send(200, {"total_count": 3, "incomplete_results": False, "items": items})
        if path == "/search/users":
            return self.send(200, {"total_count": 2, "items": [
                {"login": "whomper", "id": 1, "type": "User", "site_admin": False},
                {"login": "atari-org", "id": 2, "type": "Organization"}]})
        if len(p) == 3 and p[0] == "users" and p[2] == "repos":
            if p[1] == "nobody":
                return self.send(404, {"message": "Not Found"})
            if p[1] == "whomper":
                return self.send(200, [repo("whomper/" + n, 3, d) for n, d, _ in FAMILY], chunked=True)
            return self.send(200, [repo("%s/R%d" % (p[1], i), i, "repo %d" % i) for i in range(40)],
                             chunked=True)
        if len(p) >= 4 and p[0] == "repos" and p[3] == "releases":
            full = p[1] + "/" + p[2]
            if len(p) == 6 and p[4] == "assets":
                # as GitHub: on to a signed address of 900 characters and more
                return self.send(302, b"", "text/html", extra=(("Location", "/dl/%s?%s" % (p[5], SIGNED)),))
            if full == "whomper/Limit":
                return self.send(403, {"message": "API rate limit exceeded"})
            if full in TAGS:
                return self.send(200, release(full, TAGS[full]))
            return self.send(404, {"message": "Not Found"})
        if len(p) == 3 and p[0] == "repos":
            if p[1] + "/" + p[2] == "whomper/EDOS":
                return self.send(200, repo("whomper/EDOS", 5, "Disk Utility Package"))
            return self.send(404, {"message": "Not Found"})
        self.send(404, {"message": "Not Found"})


def main():
    srv = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler)
    if len(sys.argv) > 3:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(sys.argv[2], sys.argv[3])
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    print("ready", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
