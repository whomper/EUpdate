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
void (*up_replaced)(const REPO *r, const char *old, const char *now);
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

static int is_program(const char *name)
{
	return wild_match("*.PRG", name) || wild_match("*.ACC", name);
}

int up_chosen(const REPO *r, const GHREL *rel, short *which)
{
	short i, n = 0, zips = 0, ovl = -1, prog = 0;
	for (i = 0; i < rel->nasset; i++)
		zips += is_zip(rel->asset[i].name);
	for (i = 0; i < rel->nasset; i++) {
		const char *name = rel->asset[i].name;
		if ((r->files[0] ? listed(r->files, name) : !zips || is_zip(name)) && !left_out(r, name)) {
			which[n++] = i;
			prog |= is_program(name);
		} else if (!strcasecmp(name, "TLS.OVL") && !left_out(r, name)) {
			ovl = i;
		}
	}
	/* an AERA program's TLS (the guidelines 7.4) goes with it, whatever
	   the patterns say: without it the program can't connect */
	if (prog && ovl >= 0)
		which[n++] = ovl;
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
	int fh;				/* the file being written (as NAME.IN$), or -1 */
	char path[260];			/* where it goes */
	long size, got;
} JOB;

/* an install's programs written, and every name its release has (its
   files and its zips' files, left out or not), for retire() */
#define NPROG 8
#define NSEEN 64
static struct {
	int on;
	short nprog, nseen;
	char prog[NPROG][260];
	char seen[NSEEN][13];
} inst;

static const char *base_of(const char *path)
{
	const char *b = path + strlen(path);
	while (b > path && b[-1] != '/' && b[-1] != '\\' && b[-1] != ':')
		b--;
	return b;
}

static void seen(const char *name)
{
	if (inst.on && inst.nseen < NSEEN)
		str_copy(inst.seen[inst.nseen++], base_of(name), sizeof(inst.seen[0]));
}

/* the AERA ST editions that end with TLS.OVL (the guidelines 7.4), and
   the program each one's machine takes now */
static const char *const st_edition[][2] = {
	{ "ECALST.ACC", "ECAL.ACC" },
	{ "ECONTST.ACC", "ECONTACT.ACC" },
	{ "EMAILST.PRG", "EMAIL.PRG" },
	{ "EFTPST.PRG", "EFTP.PRG" },
};

/* an ST edition on the disk beside the program that now takes its place,
   when the release no longer has it: removed, after the program is
   safely written. Switched off (.ACX, .PRX), it is left as it is */
static void retire(const REPO *r)
{
	short i, k, s;
	for (i = 0; i < inst.nprog; i++) {
		const char *now = base_of(inst.prog[i]);
		for (k = 0; k < (short)(sizeof(st_edition) / sizeof(st_edition[0])); k++) {
			char old[260];
			if (strcasecmp(now, st_edition[k][1]))
				continue;
			for (s = 0; s < inst.nseen && strcasecmp(inst.seen[s], st_edition[k][0]); s++)
				;
			if (s < inst.nseen)
				continue;	/* the release still has it */
			snprintf(old, sizeof(old), "%.*s%s", (int)(now - inst.prog[i]), inst.prog[i],
				 st_edition[k][0]);
			if (pf_size(old) < 0 || pf_remove(old))
				continue;
			say("%s replaced by %s", st_edition[k][0], now);
			if (up_replaced)
				up_replaced(r, st_edition[k][0], now);
		}
	}
}

/* a file of the archive (or the release) to dir\a\b\NAME: every part of
   its name a proper one (no "..", no drive: nothing lands outside the
   folder), the folders made on the way. 0 to write it, 2 left out (its
   rule), 1 with j->err */
static int out_path(JOB *j, const char *name)
{
	char *path = j->path, part[64];
	const char *s = name;
	const RULE *u = up_rule(j->r, name);
	long n;
	int depth = 0;
	if (u && !strcmp(u->folder, "-"))
		return 2;		/* left out */
	str_copy(path, u ? u->folder : j->r->folder, sizeof(j->path));
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
		if (n + 16 + (opt.shorten ? 12 : k) >= (long)sizeof(j->path)) {
			snprintf(j->err, sizeof(j->err), "the path is too long: %.40s", name);
			return 1;
		}
		if (n && path[n - 1] != PF_SEP)
			path[n++] = PF_SEP;
		disk_name(part, path + n, (int)(sizeof(j->path) - n));
		s += k;
		if (last)
			break;
		s++;
		if (pf_mkdir(path)) {
			snprintf(j->err, sizeof(j->err), "can't make the folder %.60s", path);
			return 1;
		}
	}
	return 0;
}

static void cant_write(JOB *j)
{
	snprintf(j->err, sizeof(j->err), "can't write %.60s: is the disk full or locked?", j->path);
}

/* a file begun: written in pieces as they come (an archive's file is
   never held whole: the guidelines' Speed, a stock Falcon's memory) */
static int file_begin(void *ud, const char *name, long size)
{
	JOB *j = ud;
	int r;
	/* a turn for GEM between the files (an archive unpacked in one go
	   held a stock Falcon 7 s). Esc stops the job after the archive,
	   never half of it written */
	if (pf_idle)
		pf_idle();
	seen(name);
	r = out_path(j, name);
	if (r)
		return r;
	say("Writing %s...%s", j->path, "");
	j->fh = pf_wopen(j->path);
	j->size = size;
	j->got = 0;
	if (j->fh < 0) {
		cant_write(j);
		return 1;
	}
	return 0;
}

static int file_put(void *ud, const unsigned char *p, long n)
{
	JOB *j = ud;
	if (pf_wwrite(j->fh, p, n) != n) {	/* a turn every 32 KB (pf_save_turn) */
		cant_write(j);
		return 1;
	}
	j->got += n;
	return 0;
}

/* whole: renamed over the old one; else the part written taken away */
static int file_end(void *ud, int whole)
{
	JOB *j = ud;
	int keep = whole && j->got == j->size, r = pf_wclose(j->fh, j->path, keep);
	j->fh = -1;
	if (!keep)
		return 0;
	if (r) {
		cant_write(j);
		return 1;
	}
	if (inst.on && inst.nprog < NPROG && is_program(j->path))
		str_copy(inst.prog[inst.nprog++], j->path, sizeof(inst.prog[0]));
	return 0;
}

static const UNZIP_TO to_disk = { file_begin, file_put, file_end };

/* a release's file as it is, from memory or a file (src != 0), in pieces */
static int put_plain(JOB *j, const char *name, const unsigned char *data, const char *src, long len)
{
	unsigned char *piece = 0;
	int h = -1, r, ok = 1;
	long done = 0;
	r = file_begin(j, name, len);
	if (r == 2)
		return 1;
	if (r)
		return 0;
	if (src && ((h = pf_open(src, PF_READ)) < 0 || !(piece = pf_big_alloc(32768L)))) {
		str_copy(j->err, h < 0 ? "the download can't be read back" : "not enough memory for the file",
			 sizeof(j->err));
		ok = 0;
	}
	while (ok && done < len) {
		long k = len - done > 32768L ? 32768L : len - done;
		if (src && pf_read(h, piece, k) != k) {
			str_copy(j->err, "the download can't be read back", sizeof(j->err));
			ok = 0;
			break;
		}
		if (file_put(j, src ? piece : data + done, k)) {
			ok = 0;
			break;
		}
		done += k;
	}
	if (h >= 0)
		pf_close(h);
	pf_big_free(piece);
	return !file_end(j, ok) && ok;
}

static void job_init(JOB *j, REPO *r)
{
	memset(j, 0, sizeof(*j));
	j->r = r;
	j->fh = -1;
}

int up_put(REPO *r, const char *name, const unsigned char *data, long len, char *err, int errlen)
{
	JOB j;
	int ok;
	job_init(&j, r);
	if (r->unzip && is_zip(name)) {
		say("Unpacking %s...%s", name, "");
		ok = unzip_to(data, len, &to_disk, &j, j.err, sizeof(j.err)) >= 0;
	} else {
		ok = put_plain(&j, name, data, 0, len);
	}
	if (!ok)
		str_copy(err, j.err, errlen);
	return ok;
}

int up_put_file(REPO *r, const char *name, const char *src, char *err, int errlen)
{
	JOB j;
	int ok;
	job_init(&j, r);
	if (r->unzip && is_zip(name)) {
		say("Unpacking %s...%s", name, "");
		ok = unzip_file(src, &to_disk, &j, j.err, sizeof(j.err)) >= 0;
	} else {
		ok = put_plain(&j, name, 0, src, pf_size(src));
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
	memset(&inst, 0, sizeof(inst));
	inst.on = 1;
	for (i = 0; i < rel->nasset; i++)
		seen(rel->asset[i].name);
	for (i = 0; i < n; i++) {
		const GHASSET *a = &rel->asset[which[i]];
		char dl[260];
		int ok;
		say("Downloading %s from %s...", a->name, r->full);
		/* to the disk as it comes, in its folder, then from there into
		   place: a release's zip is never held whole (a stock Falcon with
		   4 MB had 510 KB free, and EMail's zip is 460 KB) */
		{
			static const char sep[2] = { PF_SEP, 0 };
			size_t fl = strlen(r->folder);
			snprintf(dl, sizeof(dl), "%s%sEUPDDL.$$$", r->folder,
				 fl && r->folder[fl - 1] == PF_SEP ? "" : sep);
		}
		ok = gh_download_file(r->full, a, dl, r->note, sizeof(r->note)) &&
		     up_put_file(r, a->name, dl, r->note, sizeof(r->note));
		pf_remove(dl);
		if (!ok) {
			inst.on = 0;
			return 0;
		}
	}
	retire(r);
	inst.on = 0;
	str_copy(r->have, rel->tag, sizeof(r->have));
	pf_now(&t);
	snprintf(r->have_date, sizeof(r->have_date), "%04d-%02d-%02d", t.year, t.mon, t.day);
	r->note[0] = 0;
	return 1;
}
