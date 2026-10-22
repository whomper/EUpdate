/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * update.c - a repository's check and its update (see update.h).
 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "plat.h"
#include "util.h"
#include "unzip.h"
#include "update.h"

void (*up_status)(const char *msg);
int up_boot_drive = 2;		/* C: */

static void say(const char *fmt, const char *a, const char *b)
{
	char m[120];
	if (!up_status)
		return;
	snprintf(m, sizeof(m), fmt, a, b);
	up_status(m);
}

int up_check(REPO *r, GHREL *rel)
{
	char err[160];
	int got;
	say("Asking GitHub about %s...%s", r->full, "");
	got = gh_latest(r->full, rel, err, sizeof(err));
	if (got <= 0) {
		str_copy(r->note, err, sizeof(r->note));
		if (!got) {		/* none: nothing to be current with */
			r->latest[0] = 0;
			r->latest_date[0] = 0;
		}
		return got < 0 ? -1 : 0;
	}
	r->note[0] = 0;
	str_copy(r->latest, rel->tag, sizeof(r->latest));
	str_copy(r->latest_date, rel->date, sizeof(r->latest_date));
	return !repo_current(r);
}

int wild_match(const char *p, const char *s)
{
	for (; *p; p++, s++) {
		if (*p == '*') {
			while (p[1] == '*')
				p++;
			if (!p[1])
				return 1;
			for (; *s; s++)
				if (wild_match(p + 1, s))
					return 1;
			return 0;
		}
		if (!*s || (*p != '?' && tolower((unsigned char)*p) != tolower((unsigned char)*s)))
			return 0;
	}
	return !*s;
}

static int is_zip(const char *name)
{
	long n = (long)strlen(name);
	return n > 4 && !strcasecmp(name + n - 4, ".zip");
}

/* one of the patterns "*.PRG, *.HYP" */
static int listed(const char *list, const char *name)
{
	char pat[40];
	const char *p = list;
	while (*p) {
		long k = (long)strcspn(p, ",");
		if (k < (long)sizeof(pat)) {
			memcpy(pat, p, k);
			pat[k] = 0;
			if (wild_match(str_trim(pat), name))
				return 1;
		}
		p += k;
		if (*p)
			p++;
	}
	return 0;
}

const RULE *up_rule(const REPO *r, const char *name)
{
	const char *base = name + strlen(name);
	short i;
	while (base > name && base[-1] != '/' && base[-1] != '\\')
		base--;
	for (i = 0; i < NRULE; i++)
		if (r->rule[i].names[0] && r->rule[i].folder[0] && listed(r->rule[i].names, base))
			return &r->rule[i];
	return 0;
}

static int left_out(const REPO *r, const char *name)
{
	const RULE *u = up_rule(r, name);
	return u && !strcmp(u->folder, "-");
}

int up_chosen(const REPO *r, const GHREL *rel, short *which)
{
	short i, n = 0, zips = 0;
	for (i = 0; i < rel->nasset; i++)
		zips += is_zip(rel->asset[i].name);
	for (i = 0; i < rel->nasset; i++) {
		const char *name = rel->asset[i].name;
		if ((r->files[0] ? listed(r->files, name) : !zips || is_zip(name)) && !left_out(r, name))
			which[n++] = i;
	}
	return n;
}

/* a release with an edition and its ST twin (AERA's ECAL.ACC and
   ECALST.ACC): the twin's name, else 0 */
static const char *st_twin(const GHREL *rel, const char *name, char *out, int size)
{
	const char *dot = strrchr(name, '.');
	short i;
	if (!dot || dot == name)
		return 0;
	snprintf(out, size, "%.*sST%s", (int)(dot - name), name, dot);
	for (i = 0; i < rel->nasset; i++)
		if (!strcasecmp(rel->asset[i].name, out))
			return out;
	return 0;
}

void up_suggest(REPO *r, const GHREL *rel, int st_machine)
{
	char skip[40] = "", twin[64];
	short i, k = 0, acc = 0;
	for (i = 0; i < rel->nasset; i++) {
		const char *name = rel->asset[i].name, *t = st_twin(rel, name, twin, sizeof(twin));
		long n = (long)strlen(skip);
		acc |= wild_match("*.ACC", name);
		if (!t)
			continue;
		/* the other machine's edition is left out */
		t = st_machine ? name : t;
		if (n + (long)strlen(t) + 2 < (long)sizeof(skip))
			snprintf(skip + n, sizeof(skip) - n, "%s%s", n ? "," : "", t);
	}
	memset(r->rule, 0, sizeof(r->rule));
	if (skip[0]) {
		str_copy(r->rule[k].names, skip, sizeof(r->rule[k].names));
		str_copy(r->rule[k].folder, "-", sizeof(r->rule[k].folder));
		k++;
	}
	if (acc) {
		/* desk accessories load from the boot drive's root */
		str_copy(r->rule[k].names, "*.ACC", sizeof(r->rule[k].names));
		snprintf(r->rule[k].folder, sizeof(r->rule[k].folder), "%c:%c", 'A' + up_boot_drive, PF_SEP);
	}
}

/* ---------------- names on the Atari's disk ---------------- */

void disk_name(const char *name, char *out, int size)
{
	const char *dot = strrchr(name, '.'), *s;
	int n = 0, k;
	if (!opt.shorten) {
		str_copy(out, name, size);
		return;
	}
	if (dot == name)
		dot = 0;		/* ".profile": a name, no extension */
	for (s = name, k = 0; *s && s != dot && k < 8 && n < size - 1; s++, k++)
		out[n++] = (unsigned char)*s <= ' ' || strchr("\"*+,./:;<=>?[\\]|", *s) ? '_' :
			   (char)toupper((unsigned char)*s);
	if (dot && dot[1] && n < size - 1) {
		out[n++] = '.';
		for (s = dot + 1, k = 0; *s && k < 3 && n < size - 1; s++, k++)
			out[n++] = (unsigned char)*s <= ' ' || strchr("\"*+,./:;<=>?[\\]|", *s) ? '_' :
				   (char)toupper((unsigned char)*s);
	}
	if (!n)
		out[n++] = '_';
	out[n] = 0;
}

/* a folder and those above it, as needed: 0 when it is there */
static int make_dirs(const char *dir)
{
	char p[200];
	long i, n = (long)strlen(dir);
	if (n >= (long)sizeof(p))
		return -1;
	if (pf_isdir(dir))
		return 0;
	strcpy(p, dir);
	for (i = 3; i < n; i++)		/* past "C:\" */
		if (p[i] == '\\' || p[i] == '/') {
			p[i] = 0;
			if (pf_mkdir(p))
				return -1;
			p[i] = dir[i];
		}
	return pf_mkdir(p);
}

typedef struct {
	REPO *r;
	char err[120];
} JOB;

/* a file of the archive (or the release) to dir\a\b\NAME: every part of
   its name a proper one (no "..", no drive: nothing lands outside the
   folder), the folders made on the way */
static int put_file(void *ud, const char *name, const unsigned char *data, long len)
{
	JOB *j = ud;
	char path[260], part[64];
	const char *s = name;
	const RULE *u = up_rule(j->r, name);
	long n;
	int depth = 0;
	if (u && !strcmp(u->folder, "-"))
		return 0;		/* left out */
	str_copy(path, u ? u->folder : j->r->folder, sizeof(path));
	if (u) {
		/* a rule's folder takes the file by its name alone */
		const char *b = name + strlen(name);
		while (b > name && b[-1] != '/' && b[-1] != '\\')
			b--;
		s = b;
		if (make_dirs(u->folder)) {
			snprintf(j->err, sizeof(j->err), "can't make the folder %.60s", u->folder);
			return 1;
		}
	}
	while (*s) {
		long k = (long)strcspn(s, "/\\");
		int last = !s[k];
		if (!k || (k == 1 && s[0] == '.') || (k == 2 && s[0] == '.' && s[1] == '.') ||
		    memchr(s, ':', k) || k >= (long)sizeof(part) || ++depth > 8) {
			snprintf(j->err, sizeof(j->err), "a file in it has a name EUpdate won't write: %.40s", name);
			return 1;
		}
		memcpy(part, s, k);
		part[k] = 0;
		n = (long)strlen(path);
		if (n + 16 + (opt.shorten ? 12 : k) >= (long)sizeof(path)) {
			snprintf(j->err, sizeof(j->err), "the path is too long: %.40s", name);
			return 1;
		}
		if (n && path[n - 1] != PF_SEP)
			path[n++] = PF_SEP;
		disk_name(part, path + n, (int)(sizeof(path) - n));
		s += k;
		if (last)
			break;
		s++;
		if (pf_mkdir(path)) {
			snprintf(j->err, sizeof(j->err), "can't make the folder %.60s", path);
			return 1;
		}
	}
	say("Writing %s...%s", path, "");
	if (pf_save(path, data, len)) {
		snprintf(j->err, sizeof(j->err), "can't write %.60s: is the disk full or locked?", path);
		return 1;
	}
	return 0;
}

int up_put(REPO *r, const char *name, const unsigned char *data, long len, char *err, int errlen)
{
	JOB j;
	int ok;
	memset(&j, 0, sizeof(j));
	j.r = r;
	if (r->unzip && is_zip(name)) {
		say("Unpacking %s...%s", name, "");
		ok = unzip(data, len, put_file, &j, j.err, sizeof(j.err)) >= 0;
	} else {
		ok = !put_file(&j, name, data, len);
	}
	if (!ok)
		str_copy(err, j.err, errlen);
	return ok;
}

int up_install(REPO *r, const GHREL *rel)
{
	short which[GH_MAXASSET];
	int n = up_chosen(r, rel, which), i;
	PFTIME t;
	if (!n) {
		str_copy(r->note, rel->nasset ? "none of the release's files is chosen (Get info)" :
			 "the release has no files", sizeof(r->note));
		return 0;
	}
	if (!r->folder[0] || make_dirs(r->folder)) {
		snprintf(r->note, sizeof(r->note), "can't make its folder: %.60s", r->folder);
		return 0;
	}
	for (i = 0; i < n; i++) {
		const GHASSET *a = &rel->asset[which[i]];
		SBUF b;
		int ok;
		say("Downloading %s from %s...", a->name, r->full);
		sb_init(&b);
		ok = gh_download(r->full, a, &b, a->size, r->note, sizeof(r->note)) &&
		     up_put(r, a->name, (const unsigned char *)b.s, b.len, r->note, sizeof(r->note));
		sb_free(&b);
		if (!ok)
			return 0;
	}
	str_copy(r->have, rel->tag, sizeof(r->have));
	pf_now(&t);
	snprintf(r->have_date, sizeof(r->have_date), "%04d-%02d-%02d", t.year, t.mon, t.day);
	r->note[0] = 0;
	return 1;
}
