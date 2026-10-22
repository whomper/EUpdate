/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * store.h - EUpdate's settings and the repositories it follows, kept in
 * EUPDATE.INF next to EUPDATE.PRG in the family's way: [OPTIONS], then
 * one [REPO] part per repository, every setting with a note above it,
 * the token encrypted with EUPDATE.KEY.
 */
#ifndef STORE_H
#define STORE_H

#define MAXREPO 64
#define NRULE 4

/* where some of a release's files go, instead of the repository's folder:
   names with * and ?, comma-separated ("*.ACC" or "ECALST.ACC,ECALST.PRG"),
   and a folder ("C:\\"), or "-" to leave them out (another machine's
   edition). The first rule that names a file wins; the files of a zip
   are named by their own names */
typedef struct {
	char names[40];
	char folder[120];
} RULE;

typedef struct {
	char full[80];		/* "owner/name" on GitHub */
	char folder[160];	/* where its files go on the Atari */
	char files[40];		/* which of a release's files: "" the zip if there
				   is one, else all; or names with * and ?, "*.PRG,*.HYP" */
	short unzip;		/* a .zip is unpacked into the folder, not kept */
	RULE rule[NRULE];	/* files that go elsewhere, or nowhere; names[0] 0: none */
	char have[40];		/* the release downloaded last: its tag, "" none */
	char have_date[11];	/* when: "2026-10-22", this Atari's date */
	char latest[40];	/* the latest release at the last check, "" unknown */
	char latest_date[11];	/* when GitHub published it */
	char note[120];		/* what the last check or update said, when it failed */
	short sel;		/* selected in the list (not kept) */
} REPO;

/* when EUpdate checks by itself */
#define CHECK_NEVER 0
#define CHECK_START 1		/* when it starts */
#define CHECK_EVERY 2		/* when it starts, and every so many hours while it runs */

typedef struct {
	short check;		/* CHECK_* */
	short hours;		/* CHECK_EVERY: every this many hours (1-168) */
	short fetch;		/* a check downloads what is new, not only says so */
	long last_check;	/* minutes since 1970 on this Atari's clock, 0 never */
	short shorten;		/* names made 8+3 for TOS's disks */
	char token[100];	/* a GitHub token, or "" */
	char api[64];		/* GitHub's API: https://api.github.com, or a GitHub
				   Enterprise server's https://HOST/api/v3 */
	short log;		/* write EUPDATE.LOG */
	short colour;		/* colours, on screens with 16 or more */
	short hebrew;		/* Hebrew keyboard on at start */
	short dsp;		/* the DSP does TLS's sums */
	short falcon;		/* TLS on this Atari: always, as there is no gateway
				   (common/ui/dsp.c reads it) */
	short main_x, main_y, main_w, main_h;	/* the window, 0 = not yet placed */
	short pane_h;		/* the repositories pane's height */
	short font_id, font_pt;	/* text font: VDI font id (1 = system) and size */
	short hebfont;		/* where the font has its Hebrew letters: HEB_* */
	char folder[160];	/* the folder chosen last, offered for the next */
	char workdir[200];	/* where EUPDATE.PRG is */
} OPTIONS;

/* where a screen font keeps the Hebrew letters (as EMail) */
#define HEB_ATARI 0
#define HEB_ISO   1
#define HEB_DOS   2

extern OPTIONS opt;
extern REPO *repos[MAXREPO];
extern short nrepos;
extern int store_lost_token;	/* EUPDATE.KEY didn't open the token */

int  store_init(const char *workdir);	/* the number of repositories */
int  store_save(void);			/* 0 when written */
REPO *repo_add(const char *full);	/* NULL when the list is full or has it */
REPO *repo_find(const char *full);
void repo_delete(REPO *r);
int  repo_current(const REPO *r);	/* has the latest release GitHub had */

/* minutes since 1970 on this Atari's clock (the schedule's) */
long store_now(void);

#endif
