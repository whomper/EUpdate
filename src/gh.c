/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gh.c - GitHub's REST API for EUpdate (see gh.h), on http.c and json.c.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "http.h"
#include "json.h"
#include "charset.h"
#include "gh.h"
#include "plat.h"

char gh_base[64] = "https://api.github.com";
char gh_token[100];
long gh_rate_left = -1;

/* what an answer may hold: a search's thirty repositories with their
   owners and links are about 150 KB, unpacked */
#define MAXANSWER (1024L * 1024)

int gh_valid_full(const char *full)
{
	const char *s = full, *slash = strchr(full, '/');
	long n;
	if (!slash || slash == full || slash - full > 39 || strchr(slash + 1, '/'))
		return 0;
	for (; s < slash; s++)
		if (!isalnum((unsigned char)*s) && *s != '-')
			return 0;
	n = (long)strlen(slash + 1);
	if (n < 1 || n > 100 || !strcmp(slash + 1, ".") || !strcmp(slash + 1, ".."))
		return 0;
	for (s = slash + 1; *s; s++)
		if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_' && *s != '.')
			return 0;
	return 1;
}

/* the API's host name, for the token: it goes nowhere else */
static const char *api_host(void)
{
	static char host[64];
	const char *s = strstr(gh_base, "://"), *e;
	s = s ? s + 3 : gh_base;
	e = s + strcspn(s, ":/");
	snprintf(host, sizeof(host), "%.*s", (int)(e - s), s);
	return host;
}

/* the "message" of an error's JSON, for the alert */
static void message_of(const SBUF *b, char *out, int size)
{
	JSON js;
	char key[16];
	out[0] = 0;
	json_init(&js, b->s, b->len);
	if (json_object(&js))
		while (json_key(&js, key, sizeof(key))) {
			if (!strcmp(key, "message"))
				json_string(&js, out, size);
			else
				json_skip(&js);
		}
}

/* GET gh_base + path into body: GitHub's status, or -1 with err */
static int ask(const char *path, int flags, SBUF *body, long max, char *err, int errlen)
{
	char url[400], auth[120];
	HTTPRESP rs;
	snprintf(url, sizeof(url), "%s%s", gh_base, path);
	snprintf(auth, sizeof(auth), "Bearer %s", gh_token);
	if (!http_get(url, flags, gh_token[0] ? auth : 0, api_host(), body, max, &rs, err, errlen))
		return -1;
	if (rs.rate_left >= 0)
		gh_rate_left = rs.rate_left;
	if (rs.status == 200 || rs.status == 404)
		return rs.status;
	if (rs.status == 401) {
		str_copy(err, "GitHub didn't take the token: check it in Settings, or leave it empty", errlen);
	} else if ((rs.status == 403 || rs.status == 429) && rs.rate_left == 0) {
		str_copy(err, gh_token[0] ? "GitHub's limit for this hour is reached: try again later" :
			 "GitHub's limit is reached (60 an hour, 10 searches a minute): try again later, "
			 "or give EUpdate a token in Settings", errlen);
	} else {
		char m[100];
		message_of(body, m, sizeof(m));
		snprintf(err, errlen, "GitHub answered %d%s%s", rs.status, m[0] ? ": " : "", m);
	}
	return -1;
}

/* text for a query: UTF-8, %-escaped, spaces as '+' */
static void query_text(const char *atari, char *out, int size)
{
	long n = 0, i, len;
	char *u = atari_to_utf8(atari, (long)strlen(atari), &len);
	out[0] = 0;
	if (!u)
		return;
	for (i = 0; i < len && n < size - 4; i++) {
		unsigned char c = (unsigned char)u[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.')
			out[n++] = (char)c;
		else if (c == ' ')
			out[n++] = '+';
		else
			n += snprintf(out + n, size - n, "%%%02X", c);
	}
	out[n] = 0;
	free(u);
}

static void repo_from(JSON *js, GHREPO *r)
{
	char key[24], date[24];
	memset(r, 0, sizeof(*r));
	if (!json_object(js))
		return;
	while (json_key(js, key, sizeof(key))) {
		if (!strcmp(key, "full_name"))
			json_string(js, r->full, sizeof(r->full));
		else if (!strcmp(key, "description"))
			json_string(js, r->desc, sizeof(r->desc));
		else if (!strcmp(key, "stargazers_count"))
			r->stars = json_long(js, 0);
		else if (!strcmp(key, "pushed_at")) {
			json_string(js, date, sizeof(date));
			str_copy(r->pushed, date, sizeof(r->pushed));
		} else if (!strcmp(key, "fork"))
			r->fork = (short)json_bool(js);
		else if (!strcmp(key, "archived"))
			r->archived = (short)json_bool(js);
		else
			json_skip(js);
	}
}

/* an array of repositories; those whose names aren't proper are left out */
static int repo_list(JSON *js, GHREPO *out, int max)
{
	int n = 0;
	if (!json_array(js))
		return 0;
	while (json_next(js)) {
		GHREPO r;
		repo_from(js, &r);
		if (n < max && gh_valid_full(r.full))
			out[n++] = r;
	}
	return n;
}

/* a search's answer: total_count, and its items with each */
static int search(const char *what, const char *text, void *out, int max, long *total,
		  int (*items)(JSON *js, void *out, int max), char *err, int errlen)
{
	char q[200], path[300], key[24];
	SBUF b;
	JSON js;
	int n = 0, st;
	*total = 0;
	query_text(text, q, sizeof(q));
	if (!q[0]) {
		str_copy(err, "Type something to look for", errlen);
		return -1;
	}
	snprintf(path, sizeof(path), "/search/%s?q=%s%s&per_page=30", what, q,
		 !strcmp(what, "repositories") ? "+in:name&sort=stars" : "");
	sb_init(&b);
	st = ask(path, HTTP_GZIP, &b, MAXANSWER, err, errlen);
	if (st == 404) {
		str_copy(err, "GitHub can't search there", errlen);
		st = -1;
	}
	if (st < 0) {
		sb_free(&b);
		return -1;
	}
	json_init(&js, b.s, b.len);
	if (json_object(&js))
		while (json_key(&js, key, sizeof(key))) {
			if (!strcmp(key, "total_count"))
				*total = json_long(&js, 0);
			else if (!strcmp(key, "items"))
				n = items(&js, out, max);
			else
				json_skip(&js);
		}
	sb_free(&b);
	if (js.err) {
		str_copy(err, "GitHub's answer didn't make sense", errlen);
		return -1;
	}
	return n;
}

static int repo_items(JSON *js, void *out, int max)
{
	return repo_list(js, out, max);
}

static int owner_items(JSON *js, void *out, int max)
{
	GHUSER *u = out;
	int n = 0;
	char key[16], type[16];
	if (!json_array(js))
		return 0;
	while (json_next(js)) {
		GHUSER one;
		memset(&one, 0, sizeof(one));
		if (json_object(js))
			while (json_key(js, key, sizeof(key))) {
				if (!strcmp(key, "login")) {
					json_string(js, one.login, sizeof(one.login));
				} else if (!strcmp(key, "type")) {
					json_string(js, type, sizeof(type));
					one.org = !strcmp(type, "Organization");
				} else {
					json_skip(js);
				}
			}
		if (n < max && one.login[0])
			u[n++] = one;
	}
	return n;
}

int gh_find_repos(const char *text, GHREPO *out, int max, long *total, char *err, int errlen)
{
	return search("repositories", text, out, max, total, repo_items, err, errlen);
}

int gh_find_owners(const char *text, GHUSER *out, int max, long *total, char *err, int errlen)
{
	return search("users", text, out, max, total, owner_items, err, errlen);
}

int gh_owner_repos(const char *login, GHREPO *out, int max, char *err, int errlen)
{
	char path[120], q[100];
	SBUF b;
	JSON js;
	int n, st;
	query_text(login, q, sizeof(q));
	snprintf(path, sizeof(path), "/users/%s/repos?sort=pushed&per_page=100", q);
	sb_init(&b);
	st = ask(path, HTTP_GZIP, &b, MAXANSWER, err, errlen);
	if (st == 404) {
		snprintf(err, errlen, "GitHub has no one called %s", login);
		st = -1;
	}
	if (st < 0) {
		sb_free(&b);
		return -1;
	}
	json_init(&js, b.s, b.len);
	n = repo_list(&js, out, max);
	sb_free(&b);
	if (js.err) {
		str_copy(err, "GitHub's answer didn't make sense", errlen);
		return -1;
	}
	return n;
}

static void asset_from(JSON *js, GHASSET *a)
{
	char key[16];
	memset(a, 0, sizeof(*a));
	if (json_object(js))
		while (json_key(js, key, sizeof(key))) {
			if (!strcmp(key, "id"))
				a->id = json_long(js, 0);
			else if (!strcmp(key, "name"))
				json_string(js, a->name, sizeof(a->name));
			else if (!strcmp(key, "size"))
				a->size = json_long(js, -1);
			else
				json_skip(js);
		}
}

int gh_latest(const char *full, GHREL *rel, char *err, int errlen)
{
	char path[160], key[24], date[24];
	SBUF b;
	JSON js;
	int st;
	memset(rel, 0, sizeof(*rel));
	if (!gh_valid_full(full)) {
		str_copy(err, "not a repository's name", errlen);
		return -1;
	}
	snprintf(path, sizeof(path), "/repos/%s/releases/latest", full);
	sb_init(&b);
	st = ask(path, HTTP_GZIP, &b, MAXANSWER, err, errlen);
	if (st == 404) {
		/* no release, or no repository: GitHub says the same; ask */
		sb_reset(&b);
		snprintf(path, sizeof(path), "/repos/%s", full);
		st = ask(path, HTTP_GZIP, &b, MAXANSWER, err, errlen);
		sb_free(&b);
		if (st < 0)
			return -1;
		str_copy(err, st == 404 ? "GitHub has no such repository" : "it has no release yet", errlen);
		return 0;
	}
	if (st < 0) {
		sb_free(&b);
		return -1;
	}
	json_init(&js, b.s, b.len);
	if (json_object(&js))
		while (json_key(&js, key, sizeof(key))) {
			if (!strcmp(key, "tag_name"))
				json_string(&js, rel->tag, sizeof(rel->tag));
			else if (!strcmp(key, "name"))
				json_string(&js, rel->name, sizeof(rel->name));
			else if (!strcmp(key, "published_at")) {
				json_string(&js, date, sizeof(date));
				str_copy(rel->date, date, sizeof(rel->date));
			} else if (!strcmp(key, "assets") && json_array(&js)) {
				while (json_next(&js)) {
					GHASSET a;
					asset_from(&js, &a);
					if (rel->nasset < GH_MAXASSET && a.id > 0 && a.name[0] && a.size >= 0)
						rel->asset[rel->nasset++] = a;
				}
			} else {
				json_skip(&js);
			}
		}
	sb_free(&b);
	if (js.err || !rel->tag[0]) {
		str_copy(err, "GitHub's answer didn't make sense", errlen);
		return -1;
	}
	return 1;
}

typedef struct {
	int h;
	long n;
} TOFILE;

static int to_file(void *u, const char *p, long n)
{
	TOFILE *t = u;
	if (pf_write(t->h, p, n) != n)
		return 1;
	t->n += n;
	return 0;
}

int gh_download_file(const char *full, const GHASSET *a, const char *file, char *err, int errlen)
{
	char path[160];
	SBUF b;
	TOFILE t;
	int st;
	if (!gh_valid_full(full)) {
		str_copy(err, "not a repository's name", errlen);
		return 0;
	}
	t.h = pf_open(file, PF_WRITE);
	t.n = 0;
	if (t.h < 0) {
		snprintf(err, errlen, "can't write %.60s: is the disk full or locked?", file);
		return 0;
	}
	sb_init(&b);
	snprintf(path, sizeof(path), "/repos/%s/releases/assets/%ld", full, a->id);
	http_sink(to_file, &t);
	st = ask(path, HTTP_OCTET, &b, a->size, err, errlen);
	http_sink(0, 0);
	pf_close(t.h);
	sb_free(&b);
	if (st == 404) {
		str_copy(err, "the file is gone from GitHub", errlen);
		return 0;
	}
	if (st < 0)
		return 0;
	if (t.n != a->size) {
		str_copy(err, "the file came short", errlen);
		return 0;
	}
	return 1;
}

int gh_download(const char *full, const GHASSET *a, SBUF *out, long max, char *err, int errlen)
{
	char path[160];
	long before = out->len;
	int st;
	if (!gh_valid_full(full) || a->size > max) {
		str_copy(err, a->size > max ? "the file is too big for this Atari's memory" :
			 "not a repository's name", errlen);
		return 0;
	}
	/* the room it needs, at once: no doubling while it comes */
	if (!sb_reserve(out, a->size)) {
		str_copy(err, "not enough memory for the file", errlen);
		return 0;
	}
	snprintf(path, sizeof(path), "/repos/%s/releases/assets/%ld", full, a->id);
	st = ask(path, HTTP_OCTET, out, max, err, errlen);
	if (st == 404) {
		str_copy(err, "the file is gone from GitHub", errlen);
		return 0;
	}
	if (st < 0)
		return 0;
	if (out->len - before != a->size) {
		str_copy(err, "the file came short", errlen);
		return 0;
	}
	return 1;
}
