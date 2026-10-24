/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * store.c - EUPDATE.INF: settings and repositories (see store.h). The
 * format and the way it is read and written are EFTP's and EMail's.
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "plat.h"
#include "store.h"
#include "secret.h"
#include "ctime.h"
#include "util.h"
#include "conn.h"
#include "tls.h"

OPTIONS opt;
REPO *repos[MAXREPO];
short nrepos;
int store_lost_token;

static char inf_path[220];

static void defaults(void)
{
	memset(&opt, 0, sizeof(opt));
	opt.check = CHECK_START;
	opt.hours = 24;
	opt.shorten = 1;
	opt.colour = 1;
	opt.dialogs3d = 1;
	opt.dsp = 1;
	opt.falcon = 1;
	opt.font_id = 1;
	str_copy(opt.api, "https://api.github.com", sizeof(opt.api));
}

long store_now(void)
{
	PFTIME t;
	pf_now(&t);
	return ct_make(t.year, t.mon, t.day, t.hour, t.min);
}

/* ---------------- the list ---------------- */

REPO *repo_find(const char *full)
{
	short i;
	for (i = 0; i < nrepos; i++)
		if (!strcasecmp(repos[i]->full, full))
			return repos[i];
	return 0;
}

REPO *repo_add(const char *full)
{
	REPO *r;
	if (nrepos >= MAXREPO || repo_find(full))
		return 0;
	r = calloc(1, sizeof(REPO));
	if (!r)
		return 0;
	str_copy(r->full, full, sizeof(r->full));
	r->unzip = 1;
	repos[nrepos++] = r;
	return r;
}

void repo_delete(REPO *r)
{
	short i, j;
	for (i = 0; i < nrepos; i++)
		if (repos[i] == r) {
			for (j = i; j < nrepos - 1; j++)
				repos[j] = repos[j + 1];
			nrepos--;
			free(r);
			return;
		}
}

int repo_current(const REPO *r)
{
	return r->have[0] && r->latest[0] && !strcmp(r->have, r->latest);
}

/* ---------------- reading ---------------- */

static void set_repo(REPO *r, const char *k, const char *v)
{
	if (!strcmp(k, "folder")) str_copy(r->folder, v, sizeof(r->folder));
	else if (!strcmp(k, "files")) str_copy(r->files, v, sizeof(r->files));
	else if (!strcmp(k, "unzip")) r->unzip = (short)(atoi(v) != 0);
	else if (!strcmp(k, "rule")) {
		/* "names;folder", one line a rule */
		const char *semi = strchr(v, ';');
		short i;
		for (i = 0; i < NRULE && r->rule[i].names[0]; i++)
			;
		if (semi && semi > v && semi[1] && i < NRULE) {
			long n = semi - v < (long)sizeof(r->rule[i].names) ? semi - v : (long)sizeof(r->rule[i].names) - 1;
			memcpy(r->rule[i].names, v, n);
			r->rule[i].names[n] = 0;
			str_copy(r->rule[i].folder, semi + 1, sizeof(r->rule[i].folder));
		}
	}
	else if (!strcmp(k, "have")) str_copy(r->have, v, sizeof(r->have));
	else if (!strcmp(k, "havedate")) str_copy(r->have_date, v, sizeof(r->have_date));
	else if (!strcmp(k, "latest")) str_copy(r->latest, v, sizeof(r->latest));
	else if (!strcmp(k, "latestdate")) str_copy(r->latest_date, v, sizeof(r->latest_date));
}

/* "a,b,c,d" -> up to n shorts */
static void shorts(const char *v, short *out, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		char *e;
		out[i] = (short)strtol(v, &e, 10);
		if (*e != ',')
			break;
		v = e + 1;
	}
}

static void set_opt(const char *k, const char *v)
{
	short n = (short)atoi(v);
	if (!strcmp(k, "check")) opt.check = n >= CHECK_NEVER && n <= CHECK_EVERY ? n : CHECK_START;
	else if (!strcmp(k, "hours")) opt.hours = n < 1 ? 1 : n > 168 ? 168 : n;
	else if (!strcmp(k, "fetch")) opt.fetch = n != 0;
	else if (!strcmp(k, "lastcheck")) opt.last_check = atol(v);
	else if (!strcmp(k, "shorten")) opt.shorten = n != 0;
	else if (!strcmp(k, "token")) {
		if (!secret_decode(v, opt.token, sizeof(opt.token))) {
			opt.token[0] = 0;
			store_lost_token = 1;
		}
	}
	else if (!strcmp(k, "apibase")) {
		if (!strncasecmp(v, "https://", 8) || !strncasecmp(v, "http://", 7))
			str_copy(opt.api, v, sizeof(opt.api));
	}
	else if (!strcmp(k, "log")) opt.log = n != 0;
	else if (!strcmp(k, "colour")) opt.colour = n != 0;
	else if (!strcmp(k, "hebrew")) opt.hebrew = n != 0;
	else if (!strcmp(k, "dialogs3d")) opt.dialogs3d = n != 0;
	else if (!strcmp(k, "dsp")) opt.dsp = n != 0;
	else if (!strcmp(k, "main")) shorts(v, &opt.main_x, 4);
	else if (!strcmp(k, "pane")) opt.pane_h = n;
	else if (!strcmp(k, "font")) shorts(v, &opt.font_id, 2);
	else if (!strcmp(k, "hebfont")) opt.hebfont = n >= 0 && n <= 2 ? n : 0;
	else if (!strcmp(k, "folder")) str_copy(opt.folder, v, sizeof(opt.folder));
}

int store_init(const char *workdir)
{
	char *buf, *line, *next, kf[220];
	REPO *cur = 0;
	short in_opts = 0;

	defaults();
	str_copy(opt.workdir, workdir, sizeof(opt.workdir));
	path_join(inf_path, sizeof(inf_path), workdir, "EUPDATE.INF");
	path_join(kf, sizeof(kf), workdir, "EUPDATE.KEY");
	secret_init(kf);
	store_lost_token = 0;
	/* TLS: the root certificates and the random seed next to EUPDATE.PRG */
	path_join(conn_cacert, sizeof(conn_cacert), workdir, "CACERT.PEM");
	path_join(tls_roots_cache, sizeof(tls_roots_cache), workdir, "ROOTS.DAT");
	path_join(tls_seed_path, sizeof(tls_seed_path), workdir, "SEED.DAT");
	buf = pf_load(inf_path, 0);
	if (!buf)
		return 0;
	for (line = buf; line && *line; line = next) {
		char *eq, *k, *key, *val, *sc;
		long i;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		k = str_trim(line);
		if (*k == ';' || !*k)
			continue;
		if (*k == '[') {
			in_opts = !strncasecmp(k, "[options]", 9);
			cur = 0;
			continue;
		}
		eq = strchr(k, '=');
		if (!eq)
			continue;
		*eq = 0;
		key = str_trim(k);
		val = eq + 1;
		for (i = 0; key[i]; i++)	/* names in any case */
			key[i] = (char)tolower((unsigned char)key[i]);
		if ((sc = strstr(val, " ;")) != 0)	/* "value ; a note" */
			*sc = 0;
		val = str_trim(val);
		if (!strcmp(key, "repo")) {	/* a [REPO] part begins with its name */
			cur = repo_add(val);
			in_opts = 0;
		} else if (cur) {
			set_repo(cur, key, val);
		} else if (in_opts) {
			set_opt(key, val);
		}
	}
	free(buf);
	return nrepos;
}

/* ---------------- writing ----------------
 * EUPDATE.INF explains itself: every setting is a ';' line saying what
 * it is, then the setting in capitals, then an empty line (guidelines
 * 6.1), so each reads as its own paragraph. */
static void put_str(SBUF *b, const char *note, const char *key, const char *val)
{
	if (note)
		sb_printf(b, "; %s\r\n", note);
	sb_printf(b, "%s=%s\r\n\r\n", key, val);
}

static void put_fmt(SBUF *b, const char *note, const char *key, const char *fmt, ...)
{
	char val[80];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(val, sizeof(val), fmt, ap);
	va_end(ap);
	put_str(b, note, key, val);
}

int store_save(void)
{
	SBUF b;
	short i;
	int r;
	char sealed[300];
	sb_init(&b);
	sb_adds(&b, "; EUpdate settings, written by EUPDATE.PRG. Lines starting with ; are notes.\r\n"
		    "; Most of these are set in Options > Settings; see EUPDATE.HYP (Help).\r\n"
		    "; 1 means on, 0 means off.\r\n\r\n[OPTIONS]\r\n");
	put_fmt(&b, "check for new releases: 0 only when asked, 1 when EUpdate starts, 2 at the start "
		    "and every HOURS while it runs", "CHECK", "%d", opt.check);
	put_fmt(&b, "the hours between checks (1-168)", "HOURS", "%d", opt.hours);
	put_fmt(&b, "1 = a check downloads what is new; 0 = it only says so", "FETCH", "%d", opt.fetch);
	put_fmt(&b, "the last check, in minutes since 1970 by this Atari's clock", "LASTCHECK", "%ld",
		opt.last_check);
	put_fmt(&b, "1 = long names are shortened to 8+3 for TOS (EFTP-1.45.zip -> EFTP-1_4.ZIP); "
		    "0 = kept (MiNT, MagiC)", "SHORTEN", "%d", opt.shorten);
	secret_encode(opt.token, sealed, sizeof(sealed));
	put_str(&b, "a GitHub token, encrypted with EUPDATE.KEY: more questions an hour (Settings)",
		"TOKEN", sealed);
	put_str(&b, "GitHub's API: https://api.github.com, or a GitHub Enterprise server's https://HOST/api/v3",
		"APIBASE", opt.api);
	put_fmt(&b, "1 = write the conversation with GitHub to EUPDATE.LOG (the token hidden)", "LOG", "%d",
		opt.log);
	put_fmt(&b, "1 = colours on screens with 16 or more", "COLOUR", "%d", opt.colour);
	put_fmt(&b, "1 = start with the Hebrew keyboard (F10 switches)", "HEBREW", "%d", opt.hebrew);
	put_fmt(&b, "1 = dialogs in the 3D look where the AES has one (TOS 4, MagiC, XaAES, EmuTOS in colour); 0 = flat",
		"DIALOGS3D", "%d", opt.dialogs3d);
	put_fmt(&b, "1 = the DSP does the sums of the secure connections; 0 = the 68030 does", "DSP", "%d",
		opt.dsp);
	put_fmt(&b, "the window: x,y,width,height in pixels; 0,0,0,0 = let EUpdate place it", "MAIN",
		"%d,%d,%d,%d", opt.main_x, opt.main_y, opt.main_w, opt.main_h);
	put_fmt(&b, "the repositories pane's height, in pixels", "PANE", "%d", opt.pane_h);
	put_fmt(&b, "text font: GDOS font id (1 = system font), size (system font: 0, 8 small, 16 large)",
		"FONT", "%d,%d", opt.font_id, opt.font_pt);
	put_fmt(&b, "where a bitmap font keeps its Hebrew letters: 0 = Atari places, 1 = ISO-8859-8, "
		    "2 = DOS 862", "HEBFONT", "%d", opt.hebfont);
	put_str(&b, "the folder chosen last", "FOLDER", opt.folder);
	for (i = 0; i < nrepos; i++) {
		REPO *p = repos[i];
		if (!i)
			sb_adds(&b, "; one [REPO] part per repository followed, up to 64\r\n\r\n");
		sb_adds(&b, "[REPO]\r\n");
		put_str(&b, i ? 0 : "owner/name on GitHub", "REPO", p->full);
		put_str(&b, i ? 0 : "where its files go", "FOLDER", p->folder);
		put_str(&b, i ? 0 : "which files: empty = the .zip if there is one, else all; or names with "
			    "* and ?, as *.PRG,*.HYP", "FILES", p->files);
		put_fmt(&b, i ? 0 : "1 = a .zip is unpacked into the folder", "UNZIP", "%d", p->unzip);
		{
			short k;
			for (k = 0; k < NRULE; k++)
				if (p->rule[k].names[0]) {
					char v[170];
					snprintf(v, sizeof(v), "%s;%s", p->rule[k].names, p->rule[k].folder);
					put_str(&b, i || k ? 0 : "files that go elsewhere: names;folder, or names;- to leave "
						    "them out (the first that names a file wins)", "RULE", v);
				}
		}
		put_str(&b, i ? 0 : "the release downloaded last, and when", "HAVE", p->have);
		put_str(&b, 0, "HAVEDATE", p->have_date);
		put_str(&b, i ? 0 : "the latest release at the last check, and when GitHub published it",
			"LATEST", p->latest);
		put_str(&b, 0, "LATESTDATE", p->latest_date);
	}
	r = b.s ? pf_save(inf_path, b.s, b.len) : -1;
	sb_free(&b);
	return r;
}
