/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * update.h - checking a followed repository for a new release, and
 * putting the release's files into its folder: the zip unpacked, long
 * names made 8+3 for TOS, every file written safely (NAME.XY$, then
 * renamed over the old one: guidelines 2.3).
 */
#ifndef UPDATE_H
#define UPDATE_H

#include "gh.h"
#include "store.h"

/* what the job is doing, for the window's info line; or NULL */
extern void (*up_status)(const char *msg);
/* an AERA ST edition (ECALST.ACC) removed by up_install, as the release
   now has one program for every Atari (now: ECAL.ACC, the guidelines
   7.4); or NULL */
extern void (*up_replaced)(const REPO *r, const char *old, const char *now);

/* asks GitHub for r's latest release (rel); r->latest follows it. 1 when
   it is newer than what r has, 0 when it isn't or there is none, -1 on
   error; r->note says what went wrong (or that there is no release) */
int up_check(REPO *r, GHREL *rel);
/* rel's files that r takes into its folder: their indices into which[],
   and how many. TLS.OVL goes with a program, unless a rule leaves it out */
int up_chosen(const REPO *r, const GHREL *rel, short *which);
/* one file downloaded, into r's folder: a zip unpacked when r says so.
   0 with err when it can't be written */
int up_put(REPO *r, const char *name, const unsigned char *data, long len, char *err, int errlen);
/* downloads and writes them; then r has rel. 1, or 0 with r->note. An
   AERA ST edition the release no longer has, beside the program that
   takes its place, is removed then (up_replaced) */
int up_install(REPO *r, const GHREL *rel);

/* the rule of r that names a file (its own name, a zip's file by the
   last part), or NULL */
const RULE *up_rule(const REPO *r, const char *name);
/* r's rules from what rel has: a machine's edition and its ST twin
   (ECAL.ACC, ECALST.ACC) -> the other machine's left out; desk
   accessories -> the boot drive's root. st_machine: this is a 68000 */
void up_suggest(REPO *r, const GHREL *rel, int st_machine);
extern int up_boot_drive;	/* 0 = A:, 2 = C: (the Atari's; for up_suggest) */

/* name and pattern, case aside: '*' any run, '?' one character */
int wild_match(const char *pat, const char *name);
/* one name for TOS's disks: 8+3, without the characters FAT refuses
   ("EFTP-1.45.zip" -> "EFTP-1_4.ZIP" when opt.shorten) */
void disk_name(const char *name, char *out, int size);

#endif
