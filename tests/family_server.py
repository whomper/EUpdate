#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""GitHub as the AERA family's releases have it, for the Hatari rigs: the
real latest releases of each app (tests/github/fetch_family.py's CACHE),
answered as GitHub's API answers them, and their files from a second
server, as GitHub sends them on to release-assets.githubusercontent.com.

    family_server.py API_PORT FILES_PORT CACHE CERT KEY

Run twice: once on API_PORT (the API), once on FILES_PORT (the files);
each answers both, the rig points EUpdate at the first. As GitHub's:
  - a file's download is a 302 to the other server, at a signed address
    of more than 900 characters, refused (400) if it comes back cut short;
  - an idle connection is closed after a few seconds (GitHub closes the
    API's during every long download, Erez's log of 22 Oct 2026);
  - answers come gzip'ed when asked, with X-RateLimit-Remaining;
  - TLS with a chain like GitHub's (the rig makes it: a leaf signed by an
    intermediate of RSA-4096).
"""
import gzip
import json
import os
import ssl
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SIGNED = ("sp=r&sv=2018-11-09&sr=b&spr=https&se=2026-10-22T10%3A55%3A59Z&rscd=attachment&rsct=application"
          "%2Foctet-stream&skoid=96c2d410-5711-43a1-aedd-ab1947aa7ab0&sig=" + "Zm9vYmFy" * 100)


def load(cache):
    """{repo: (tag, [asset...])}, {id: path}"""
    rel, files, n = {}, {}, 1000
    for repo in sorted(os.listdir(cache)):
        tagfile = os.path.join(cache, repo, "TAG")
        if not os.path.exists(tagfile):
            continue
        tag = open(tagfile).read().strip()
        d = os.path.join(cache, repo, tag)
        assets = []
        for f in sorted(os.listdir(d)):
            n += 1
            files[str(n)] = os.path.join(d, f)
            assets.append({"url": "u", "id": n, "node_id": "n", "name": f, "label": None,
                           "uploader": {"login": "github-actions[bot]", "type": "Bot"},
                           "content_type": "application/octet-stream", "state": "uploaded",
                           "size": os.path.getsize(os.path.join(d, f)), "download_count": 1,
                           "browser_download_url": "https://github.com/whomper/%s/releases/download/%s/%s" %
                                                   (repo, tag, f)})
        rel[repo] = (tag, assets)
    return rel, files


def main():
    api_port, files_port, cache, cert, key = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
    rel, files = load(cache)

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        timeout = 4                     # idle connections closed, as GitHub's

        def log_message(self, *a):
            pass

        def send(self, code, body, ctype="application/json", extra=()):
            if isinstance(body, (dict, list)):
                body = json.dumps(body).encode()
            gz = "gzip" in self.headers.get("Accept-Encoding", "") and ctype == "application/json"
            if gz:
                body = gzip.compress(body)
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("X-RateLimit-Remaining", "4321")
            for k, v in extra:
                self.send_header(k, v)
            if gz:
                self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path, _, query = self.path.partition("?")
            p = path.strip("/").split("/")
            if p[0] == "dl" and len(p) == 2:
                if query != SIGNED or p[1] not in files:
                    return self.send(400, b"<Error>AuthenticationFailed</Error>", "application/xml")
                return self.send(200, open(files[p[1]], "rb").read(), "application/octet-stream")
            if len(p) >= 4 and p[0] == "repos" and p[3] == "releases":
                if len(p) == 6 and p[4] == "assets":
                    return self.send(302, b"", "text/html", extra=(
                        ("Location", "https://127.0.0.1:%d/dl/%s?%s" % (files_port, p[5], SIGNED)),))
                if p[2] in rel:
                    tag, assets = rel[p[2]]
                    return self.send(200, {"url": "u", "tag_name": tag, "name": "%s %s" % (p[2], tag[1:]),
                                           "author": {"login": "github-actions[bot]"}, "draft": False,
                                           "prerelease": False, "published_at": "2026-10-21T09:05:00Z",
                                           "assets": assets, "body": "What is new"})
                return self.send(404, {"message": "Not Found"})
            if len(p) == 3 and p[0] == "repos":
                return self.send(200 if p[2] in rel else 404, {"full_name": "%s/%s" % (p[1], p[2])})
            self.send(404, {"message": "Not Found"})

    srv = ThreadingHTTPServer(("127.0.0.1", api_port), Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    print("ready", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
