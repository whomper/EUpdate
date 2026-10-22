#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The fuzz targets' seeds (tests/fuzz/seeds/TARGET/), the same every time:
answers as GitHub gives them, JSON, and the unit tests' zips."""
import gzip
import os

H = os.path.dirname(os.path.abspath(__file__))
S = os.path.join(H, "seeds")


def put(target, name, data):
    d = os.path.join(S, target)
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, name), "wb").write(data)


def answer(body, extra=b"", gz=False, chunked=False):
    if gz:
        body = gzip.compress(body, mtime=0)
        extra += b"Content-Encoding: gzip\r\n"
    if chunked:
        return (b"HTTP/1.1 200 OK\r\n" + extra + b"Transfer-Encoding: chunked\r\n\r\n" +
                b"%x\r\n" % len(body) + body + b"\r\n0\r\n\r\n")
    return b"HTTP/1.1 200 OK\r\n" + extra + b"Content-Length: %d\r\n\r\n" % len(body) + body


release = (b'{"tag_name":"v1.45","name":"EFTP 1.45","published_at":"2026-10-20T09:15:00Z",'
           b'"assets":[{"id":101,"name":"EFTP-1.45.zip","size":5},{"id":102,"name":"EFTP.PRG","size":3}]}')
search = (b'{"total_count":1,"items":[{"full_name":"whomper/EFTP","description":"FTP \\u00e9",'
          b'"stargazers_count":7,"pushed_at":"2026-10-21T10:00:00Z","fork":false,"archived":false}]}')
users = b'{"total_count":1,"items":[{"login":"whomper","type":"User"}]}'
repos = b'[{"full_name":"whomper/ECal","description":null,"stargazers_count":3}]'
put("http", "plain", answer(release) + answer(search) + answer(users) + answer(repos) +
    b"HTTP/1.1 302 Found\r\nLocation: /x\r\nContent-Length: 0\r\n\r\n" + answer(b"hello"))
put("http", "gzip", answer(release, gz=True) + answer(search, gz=True, chunked=True))
put("http", "none", b"HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\n\r\n{}" * 2 +
    b"HTTP/1.1 403 Forbidden\r\nX-RateLimit-Remaining: 0\r\nContent-Length: 27\r\n\r\n"
    b'{"message":"rate limited"}\n')
for name, data in (("release", release), ("search", search), ("repos", repos)):
    put("json", name, data)
put("json", "deep", b"[" * 40 + b"]" * 40)
put("json", "escapes", b'["\\ud83d\\ude00\\u05e9", "\\n\\t", -12, 1.5e3, true, false, null]')
for z in ("good.zip", "bad.zip", "evil.zip"):
    put("zip", z, open(os.path.join(H, "..", "data", z), "rb").read())
