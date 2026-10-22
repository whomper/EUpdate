#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
"""The AERA family's latest releases, every file, as GitHub has them, for
tests/family_server.py: CACHE/<Repo>/<tag>/<file>, and CACHE/<Repo>/TAG.

    fetch_family.py CACHE [Repo ...]

The list of a release's files comes from GitHub's API when it answers;
where it doesn't (its hourly limit, or a session that may not ask it),
the family's own names are tried one by one (each app's make dist), by
github.com's download addresses. Standard Python only.
"""
import json
import os
import sys
import urllib.error
import urllib.request

OWNER = "whomper"
FAMILY = ["ECal", "EContact", "EMail", "EFTP", "EHypView", "ClaudeST", "EWeb", "EUpdate"]
# what each app's make dist publishes, VER for its version
NAMES = {
    "ECal": "ECAL.ACC ECALST.ACC ECAL.PRG ECAL.HYP ECAL.REF ECAL.RSC ECAL.ICN ECALMK.ICN ECAL-VER.zip",
    "EContact": "ECONTACT.ACC ECONTST.ACC ECONTACT.PRG ECONTACT.HYP ECONTACT.REF ECONTACT.RSC ECONTACT.DEF "
                "ECONTACT.ICN ECONTMSK.ICN ECONTACT-VER.zip",
    "EMail": "EMAIL.PRG EMAILST.PRG EMAIL.HYP EMAIL.REF EMAIL.RSC EMAIL.ICN EMAILMK.ICN EMAIL-VER.zip",
    "EFTP": "EFTP.PRG EFTPST.PRG EFTP.HYP EFTP.REF EFTP.RSC EFTP.ICN EFTPMK.ICN EFTP-VER.zip",
    "EHypView": "EHYPVIEW.PRG EHYPVIEW.ACC EHYPVIEW.HYP EHYPVIEW.REF EHYPVIEW.RSC EHYPVIEW.ICN EHYPVWMK.ICN "
                "EHYPVIEW-VER.zip",
    "ClaudeST": "CLAUDEST.PRG CLAUDEST.HYP CLAUDEST.REF CLAUDEST.RSC CLAUDEST.ICN CLAUDSMK.ICN CLAUDEST-VER.zip",
    "EWeb": "EWEB.PRG EWEBST.PRG EWEB.FNT EWEB.HYP NSBENCH.TTP NSBENCH.TXT EWGET.TTP EWGET.TXT SAMPLE.HTM "
            "NEWS.HTM NEWS.CSS PICBENCH.TTP PICBENCH.TXT PHOTO.JPG EWEB-VER.zip",
    "EUpdate": "EUPDATE.PRG EUPDATE.HYP EUPDATE.RSC EUPDATE.ICN EUPDMK.ICN EUPDATE-VER.zip",
}
COMMON = "CACERT.PEM ROOTS.DAT"


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a):
        return None


def latest_tag(repo):
    """the latest release's tag, from where github.com sends a file of it
    (as common/tools/getrelease.py)"""
    try:
        urllib.request.build_opener(_NoRedirect).open(urllib.request.Request(
            "https://github.com/%s/%s/releases/latest/download/x" % (OWNER, repo), method="HEAD"))
    except urllib.error.HTTPError as e:
        loc = e.headers.get("Location", "")
        if "/download/" in loc:
            return loc.rsplit("/download/", 1)[1].split("/")[0]
    sys.exit("no latest release for %s" % repo)


def api_names(repo):
    try:
        req = urllib.request.Request("https://api.github.com/repos/%s/%s/releases/latest" % (OWNER, repo),
                                     headers={"User-Agent": "EUpdate-tests"})
        return [a["name"] for a in json.load(urllib.request.urlopen(req, timeout=30))["assets"]]
    except (urllib.error.URLError, KeyError, ValueError):
        return None


def get(repo, tag, name, path):
    try:
        data = urllib.request.urlopen("https://github.com/%s/%s/releases/download/%s/%s" %
                                      (OWNER, repo, tag, name), timeout=120).read()
    except urllib.error.HTTPError:
        return False
    with open(path, "wb") as f:
        f.write(data)
    return True


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cache = sys.argv[1]
    for repo in sys.argv[2:] or FAMILY:
        tag = latest_tag(repo)
        d = os.path.join(cache, repo, tag)
        if not os.path.isdir(d) or not os.listdir(d):
            os.makedirs(d, exist_ok=True)
            names = api_names(repo) or (NAMES[repo] + " " + COMMON).replace("VER", tag[1:]).split()
            got = [n for n in names if get(repo, tag, n, os.path.join(d, n))]
            if not got:
                sys.exit("no files for %s %s" % (repo, tag))
        with open(os.path.join(cache, repo, "TAG"), "w") as f:
            f.write(tag + "\n")
        print("%-9s %-6s %s" % (repo, tag, " ".join(sorted(os.listdir(d)))))


if __name__ == "__main__":
    main()
