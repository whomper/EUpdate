/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fuzz_http.c - GitHub's answers as the fuzz input says them (fuzz_net.c):
 * the HTTP client (status lines, headers, chunks, gzip, redirects) and
 * gh.c's reading of a release, a search and a list of repositories.
 */
#include <stdint.h>
#include <stddef.h>
#include "fuzz_net.h"
#include "util.h"
#include "http.h"
#include "gh.h"

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	char err[200];
	GHREL rel;
	GHREPO repos[8];
	GHUSER users[8];
	long total;
	SBUF b;
	str_copy(gh_base, "http://api.test", sizeof(gh_base));
	fuzz_net_set(d, (long)n);
	gh_latest("whomper/EFTP", &rel, err, sizeof(err));
	gh_find_repos("atari", repos, 8, &total, err, sizeof(err));
	gh_find_owners("atari", users, 8, &total, err, sizeof(err));
	gh_owner_repos("whomper", repos, 8, err, sizeof(err));
	if (rel.nasset) {
		sb_init(&b);
		gh_download("whomper/EFTP", &rel.asset[0], &b, 1L << 20, err, sizeof(err));
		sb_free(&b);
	}
	http_close_all();
	return 0;
}
