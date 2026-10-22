/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * eupd_cli.c - EUpdate's core from the command line, on this computer:
 * what tests/integration.py drives against tests/server.py, and a way to
 * try GitHub itself.
 *
 *   eupd-cli [-b BASE] [-c CACERT] [-t TOKEN] [-l LOG] COMMAND ...
 *     repos TEXT            find repositories by name
 *     owners TEXT           find owners
 *     owner LOGIN           an owner's repositories
 *     latest OWNER/NAME     its latest release and its files
 *     get OWNER/NAME DIR [FILES]   download it into DIR (FILES: "*.PRG,...")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "plat.h"
#include "util.h"
#include "conn.h"
#include "http.h"
#include "gh.h"
#include "store.h"
#include "update.h"

static void status(const char *m)
{
	fprintf(stderr, "%s\n", m);
}

int main(int argc, char **argv)
{
	char err[200];
	int c, n, i;
	long total;
	pf_init();
	str_copy(conn_cacert, "common/certs/CACERT.PEM", sizeof(conn_cacert));
	while ((c = getopt(argc, argv, "b:c:t:l:")) != -1) {
		if (c == 'b')
			str_copy(gh_base, optarg, sizeof(gh_base));
		else if (c == 'c')
			str_copy(conn_cacert, optarg, sizeof(conn_cacert));
		else if (c == 't')
			str_copy(gh_token, optarg, sizeof(gh_token));
		else if (c == 'l')
			str_copy(conn_logfile, optarg, sizeof(conn_logfile));
		else
			return 2;
	}
	argc -= optind;
	argv += optind;
	if (argc < 2)
		return 2;
	opt.shorten = 1;
	up_status = status;
	if (!strcmp(argv[0], "repos") || !strcmp(argv[0], "owner")) {
		GHREPO r[100];
		n = !strcmp(argv[0], "repos") ? gh_find_repos(argv[1], r, 100, &total, err, sizeof(err)) :
		    gh_owner_repos(argv[1], r, 100, err, sizeof(err));
		for (i = 0; i < n; i++)
			printf("%s\t%ld\t%s\t%s\n", r[i].full, r[i].stars, r[i].pushed, r[i].desc);
	} else if (!strcmp(argv[0], "owners")) {
		GHUSER u[30];
		n = gh_find_owners(argv[1], u, 30, &total, err, sizeof(err));
		for (i = 0; i < n; i++)
			printf("%s\t%s\n", u[i].login, u[i].org ? "organisation" : "person");
	} else if (!strcmp(argv[0], "latest") || !strcmp(argv[0], "get")) {
		GHREL rel;
		REPO *r = repo_add(argv[1]);
		n = up_check(r, &rel);
		if (n >= 0 && !rel.tag[0]) {
			printf("none: %s\n", r->note);
			n = 0;
		} else if (n >= 0) {
			printf("%s\t%s\t%s\n", rel.tag, rel.date, rel.name);
			for (i = 0; i < rel.nasset; i++)
				printf("  %ld\t%ld\t%s\n", rel.asset[i].id, rel.asset[i].size, rel.asset[i].name);
			if (!strcmp(argv[0], "get") && argc >= 3) {
				str_copy(r->folder, argv[2], sizeof(r->folder));
				r->unzip = 1;
				if (argc >= 4)
					str_copy(r->files, argv[3], sizeof(r->files));
				n = up_install(r, &rel) ? 1 : -1;
				printf("have %s\n", r->have);
			}
		}
		if (n < 0)
			str_copy(err, r->note, sizeof(err));
	} else {
		return 2;
	}
	printf("rate left %ld\n", gh_rate_left);
	http_close_all();
	conn_log_flush();
	if (n < 0) {
		fprintf(stderr, "error: %s\n", err);
		return 1;
	}
	return 0;
}
