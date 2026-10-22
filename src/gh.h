/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gh.h - what EUpdate asks GitHub's REST API (api.github.com): finding
 * repositories by name, finding owners (people and organisations) and
 * listing theirs, a repository's latest release, and its files.
 *
 * Without a token GitHub answers 60 questions an hour from one address,
 * and 10 searches a minute; with one (Settings), 5,000 and 30.
 */
#ifndef GH_H
#define GH_H

#include "util.h"

#define GH_MAXASSET 24

typedef struct {
	char full[80];		/* "owner/name" */
	char desc[100];		/* its description, Atari text */
	long stars;
	char pushed[11];	/* last changed: "2026-10-20" */
	short fork, archived;
} GHREPO;

typedef struct {
	char login[40];
	short org;		/* an organisation, not a person */
} GHUSER;

typedef struct {
	long id;
	char name[64];		/* the file's name on GitHub */
	long size;
} GHASSET;

typedef struct {
	char tag[40];		/* "v1.45" */
	char name[64];		/* the release's title */
	char date[11];		/* published: "2026-10-20" */
	short nasset;
	GHASSET asset[GH_MAXASSET];
} GHREL;

/* where the API is: "https://api.github.com" (the tests point it at a
   server of their own) */
extern char gh_base[64];
/* a personal access token, or "": sent to the API's host only */
extern char gh_token[100];
/* GitHub's limit, from the last answer: questions left, -1 unknown */
extern long gh_rate_left;

/* repositories whose names have text in them, the most starred first:
   up to max into out; the count found, -1 on error (err says why);
   *total is how many GitHub has (it lists the first 30) */
int gh_find_repos(const char *text, GHREPO *out, int max, long *total, char *err, int errlen);
/* owners whose login or name has text in them */
int gh_find_owners(const char *text, GHUSER *out, int max, long *total, char *err, int errlen);
/* an owner's public repositories, the last changed first */
int gh_owner_repos(const char *login, GHREPO *out, int max, char *err, int errlen);
/* the latest release of "owner/name": 1, 0 when it has none (no release,
   or no such repository: err says which), -1 on error */
int gh_latest(const char *full, GHREL *rel, char *err, int errlen);
/* one of its files, appended to out (at most max bytes) */
int gh_download(const char *full, const GHASSET *a, SBUF *out, long max, char *err, int errlen);

/* "owner/name": 1 if it is one (a GitHub login, a repository's name) */
int gh_valid_full(const char *full);

#endif
