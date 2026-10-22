/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * lists.c - the window's two lists.
 *
 * Above, the repositories followed: what each has, the latest release
 * GitHub had at the last check, and whether it is current; rows are
 * selected for Update and the rest (a click, Shift+click for more, ^A).
 * Below, what Find found: repositories (a tick by those followed), or
 * owners, whose repositories a double click lists; Backspace goes back
 * to the owners.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "theme.h"
#include "util.h"
#include "clip.h"

/* ---------------- the repositories followed ---------------- */

static long cursor;			/* the row the keys move from */

int found_new_count(void)
{
	short i, n = 0;
	for (i = 0; i < nrepos; i++)
		if (repos[i]->latest[0] && !repo_current(repos[i]))
			n++;
	return n;
}

long repos_selected(void)
{
	short i, n = 0;
	for (i = 0; i < nrepos; i++)
		n += repos[i]->sel;
	return n;
}

/* a row's state, and its colour */
static const char *state_of(const REPO *r, short *p)
{
	*p = P_DIM;
	if (!r->latest[0]) {
		if (!r->note[0])
			return "Not checked";
		if (!strncmp(r->note, "it has no release", 17))
			return "No release";
		*p = P_ERR;
		return "Failed";
	}
	if (r->note[0]) {
		*p = P_ERR;
		return "Failed";
	}
	if (!r->have[0]) {
		*p = P_NEW;
		return "Not downloaded";
	}
	if (repo_current(r)) {
		*p = P_OK;
		return "Current";
	}
	*p = P_NEW;
	return "New release";
}

/* the columns, in characters: name, have, latest, state; the folder
   takes the rest when there is room */
#define C_HAVE   12
#define C_LATEST 12
#define C_STATE  15
static short col_name(short cols)
{
	short n = cols - C_HAVE - C_LATEST - C_STATE - 2;
	if (cols >= 90)
		n = 30;
	return n < 12 ? 12 : n;
}

static void column_titles(WIN *w, const char *const *t, const short *x, short n)
{
	short i, y = w->work.y;
	GRECT r = w->work;
	r.h = w->head_h;
	fill(&r, 0);
	for (i = 0; i < n; i++)
		text_pen_at(colour ? pen[P_DIM] : 1, w->work.x + x[i] * cw, y, t[i], (long)strlen(t[i]),
			    (short)strlen(t[i]), TX_LTR | (colour ? 0 : TX_LIGHT));
	line_col(w->work.x, y + w->head_h - 1, w->work.x + w->work.w - 1, y + w->head_h - 1, pen[P_LINE]);
}

/* one cell: text cut to its column, in a colour or inverted on the bar */
static void cell(short x, short y, const char *s, short width, short p, short sel, short flags)
{
	if (sel || !colour || p == P_TEXT)
		text_at(x, y, s, (long)strlen(s), width, flags | (sel ? TX_INVERSE : 0) |
			(!colour && p == P_DIM && !sel ? TX_LIGHT : 0));
	else
		text_pen_at(pen[p], x, y, s, (long)strlen(s), width, flags);
}

static void repos_draw(WIN *w, GRECT *clip)
{
	static const char *const t[] = { "Repository", "Have", "Latest", "State", "Folder" };
	short cols = w->work.w / cw, cn = col_name(cols), x[5], i;
	long row;
	x[0] = 1;
	x[1] = x[0] + cn + 1;
	x[2] = x[1] + C_HAVE;
	x[3] = x[2] + C_LATEST;
	x[4] = x[3] + C_STATE;
	column_titles(w, t, x, cols - x[4] >= 12 ? 5 : 4);
	for (row = w->top; row < w->top + win_rows(w) + 1; row++) {
		GRECT r;
		REPO *p;
		short y = w->work.y + w->head_h + (short)((row - w->top) * ch), sp, sel;
		const char *st;
		r.x = w->work.x;
		r.y = y;
		r.w = w->work.w;
		r.h = ch;
		if (row >= nrepos) {
			fill(&r, 0);
			continue;
		}
		p = repos[row];
		sel = p->sel;
		fill(&r, sel ? (colour ? pen[P_SEL] : 1) : 0);
		st = state_of(p, &sp);
		cell(r.x + x[0] * cw, y, p->full, cn, P_TEXT, sel, TX_LTR);
		cell(r.x + x[1] * cw, y, p->have[0] ? p->have : "-", C_HAVE - 1, P_TEXT, sel, TX_LTR);
		cell(r.x + x[2] * cw, y, p->latest[0] ? p->latest : "-", C_LATEST - 1, P_TEXT, sel, TX_LTR);
		cell(r.x + x[3] * cw, y, st, C_STATE - 1, sp, sel, TX_LTR | (sp == P_NEW ? TX_BOLD : 0));
		if (cols - x[4] >= 12)
			cell(r.x + x[4] * cw, y, p->folder, cols - x[4] - 1, P_DIM, sel, TX_LTR);
	}
	(void)clip;
	(void)i;
}

static long repo_at(WIN *w, short my)
{
	long row = w->top + (my - w->work.y - w->head_h) / ch;
	return my < w->work.y + w->head_h || row >= nrepos ? -1 : row;
}

void repos_changed(void)
{
	char info[40];
	short k = found_new_count();
	w_repos.total = nrepos;
	if (cursor >= nrepos)
		cursor = nrepos ? nrepos - 1 : 0;
	if (k)
		snprintf(info, sizeof(info), "%d followed, %d new", nrepos, k);
	else
		snprintf(info, sizeof(info), "%d followed", nrepos);
	win_info(&w_repos, nrepos ? info : "");
	win_lines_changed(&w_repos);
	toolbar_redraw();
	menu_update();
}

void repo_redraw(REPO *r)
{
	short i;
	for (i = 0; i < nrepos; i++)
		if (repos[i] == r)
			win_redraw_lines(&w_repos, i, 1);
}

static void select_only(long row)
{
	short i;
	for (i = 0; i < nrepos; i++)
		if (repos[i]->sel != (i == row)) {
			repos[i]->sel = (short)(i == row);
			win_redraw_lines(&w_repos, i, 1);
		}
	if (row >= 0) {
		cursor = row;
		win_ensure_visible(&w_repos, row);
	}
	toolbar_redraw();
}

void repos_select_all(void)
{
	short i;
	for (i = 0; i < nrepos; i++)
		repos[i]->sel = 1;
	win_redraw(&w_repos, 0);
	toolbar_redraw();
}

void repos_copy(void)
{
	SBUF b;
	short i;
	sb_init(&b);
	for (i = 0; i < nrepos; i++)
		if (repos[i]->sel)
			sb_printf(&b, "%s\r\n", repos[i]->full);
	if (b.len)
		clip_put(b.s, b.len);
	sb_free(&b);
}

static void repos_click(WIN *w, short mx, short my, short clicks, short kstate)
{
	long row = repo_at(w, my);
	if (row < 0) {
		select_only(-1);
		return;
	}
	if (kstate & (K_LSHIFT | K_RSHIFT)) {
		repos[row]->sel = !repos[row]->sel;
		cursor = row;
		win_redraw_lines(w, row, 1);
		toolbar_redraw();
		return;
	}
	select_only(row);
	if (clicks >= 2)
		repo_info(repos[row]);
}

static void repos_rclick(WIN *w, short mx, short my)
{
	static const char *const items[] = {
		"Get info...          ^I", "-",
		"Check for a release  ^R", "Update               ^U", "Download again", "-",
		"Copy name            ^C", "-", "Unfollow            Del"
	};
	static const short cmds[] = { C_INFO, 0, C_CHECK, C_UPDATE, C_AGAIN, 0, C_COPY, 0, C_UNFOLLOW };
	long row = repo_at(w, my);
	short r;
	if (row < 0) {
		static const char *const empty[] = { "Find on GitHub...  ^F", "Follow...          ^N" };
		r = popup(mx, my, empty, 2);
		if (r >= 0)
			ui_command(r ? C_FOLLOW : C_FIND);
		return;
	}
	if (!repos[row]->sel)
		select_only(row);
	r = popup(mx, my, items, sizeof(items) / sizeof(items[0]));
	if (r >= 0 && cmds[r])
		ui_command(cmds[r]);
}

static int repos_key(WIN *w, short kstate, short key)
{
	short scan = KEY_SCAN(key);
	if (!nrepos)
		return 0;
	switch (scan) {
	case 0x48:			/* up */
		select_only(cursor > 0 ? cursor - 1 : 0);
		return 1;
	case 0x50:			/* down */
		select_only(cursor < nrepos - 1 ? cursor + 1 : nrepos - 1);
		return 1;
	case 0x1c:			/* Return */
	case 0x72:
		if (repos_selected() == 1)
			ui_command(C_INFO);
		return 1;
	case 0x53:			/* Delete */
		ui_command(C_UNFOLLOW);
		return 1;
	case 0x52:			/* Insert: the right-click menu, at the row */
		if (!repos_selected())
			select_only(cursor);
		repos_rclick(w, w->work.x + 4 * cw,
			     w->work.y + w->head_h + (short)((cursor - w->top) * ch) + ch);
		return 1;
	}
	return 0;
}

/* what didn't fit, or what went wrong */
static int repos_tip(WIN *w, short mx, short my, char *out, int size)
{
	long row = repo_at(w, my);
	REPO *r;
	if (row < 0)
		return 0;
	r = repos[row];
	if (r->note[0])
		snprintf(out, size, "%s|%s", r->full, r->note);
	else if (r->latest[0])
		snprintf(out, size, "%s: %s published on %s|%s%s%s", r->full, r->latest, r->latest_date,
			 r->have[0] ? r->have : "", r->have[0] ? " downloaded on " : "Nothing downloaded yet",
			 r->have_date);
	else
		snprintf(out, size, "%s|Into %s", r->full, r->folder);
	return 1;
}

/* ---------------- what Find found ---------------- */

/* what was found, the job's memory handed over (malloc'ed, kept until
   the next search: in TT RAM, not in the program's BSS) */
static GHREPO *f_repos;
static GHUSER *f_owners;
static short nf_owners, showing_owners, have_owners;
static long f_sel = -1;
static char owners_title[80];

static void found_set(const char *title, const char *info, long total)
{
	win_title(&w_found, title);
	win_info(&w_found, info);
	w_found.total = total;
	w_found.top = 0;
	f_sel = total ? 0 : -1;
	win_lines_changed(&w_found);
	win_focus(&w_found);
}

void found_show_repos(const char *title, GHREPO *list, int n, long total, int an_owners)
{
	char info[40];
	free(f_repos);
	f_repos = list;
	showing_owners = 0;
	have_owners &= an_owners;	/* Backspace goes back to the owners only from theirs */
	if (total > n)
		snprintf(info, sizeof(info), "the first %d of %ld", n, total);
	else
		snprintf(info, sizeof(info), "%d", n);
	found_set(title, n ? info : "none", n);
}

void found_show_owners(const char *title, GHUSER *list, int n, long total)
{
	char info[40];
	free(f_owners);
	f_owners = list;
	nf_owners = (short)n;
	showing_owners = have_owners = 1;
	str_copy(owners_title, title, sizeof(owners_title));
	if (total > n)
		snprintf(info, sizeof(info), "the first %d of %ld", n, total);
	else
		snprintf(info, sizeof(info), "%d", n);
	found_set(title, n ? info : "none", n);
}

int found_back(void)
{
	char info[16];
	if (showing_owners || !have_owners)
		return 0;
	showing_owners = 1;
	snprintf(info, sizeof(info), "%d", nf_owners);
	found_set(owners_title, info, nf_owners);
	return 1;
}

void found_copy(void)
{
	char line[90];
	if (f_sel < 0 || f_sel >= w_found.total)
		return;
	snprintf(line, sizeof(line), "%s", showing_owners ? f_owners[f_sel].login : f_repos[f_sel].full);
	clip_put(line, (long)strlen(line));
}

static void found_draw(WIN *w, GRECT *clip)
{
	static const char *const tr[] = { "Repository", "Stars", "Changed", "What it is" };
	static const char *const to[] = { "Owner" };
	short cols = w->work.w / cw, x[4], cn = cols >= 90 ? 32 : 26;
	long row;
	x[0] = 3;
	x[1] = x[0] + cn + 1;
	x[2] = x[1] + 7;
	x[3] = x[2] + 12;
	if (showing_owners)
		column_titles(w, to, x, 1);
	else
		column_titles(w, tr, x, 4);
	for (row = w->top; row < w->top + win_rows(w) + 1; row++) {
		GRECT r;
		short y = w->work.y + w->head_h + (short)((row - w->top) * ch), sel = row == f_sel;
		char t[16];
		r.x = w->work.x;
		r.y = y;
		r.w = w->work.w;
		r.h = ch;
		fill(&r, row < w->total && sel ? (colour ? pen[P_SEL] : 1) : 0);
		if (row >= w->total)
			continue;
		if (showing_owners) {
			const GHUSER *u = &f_owners[row];
			cell(r.x + x[0] * cw, y, u->login, cn, P_TEXT, sel, TX_LTR);
			cell(r.x + (x[0] + cn + 1) * cw, y, u->org ? "organisation" : "person", 14, P_DIM, sel, TX_LTR);
			continue;
		}
		{
			const GHREPO *g = &f_repos[row];
			/* a tick by what is followed already (the system font's check mark) */
			if (repo_find(g->full))
				cell(r.x + cw, y, "\x08", 1, P_OK, sel, TX_LTR);
			cell(r.x + x[0] * cw, y, g->full, cn, P_TEXT, sel, TX_LTR);
			snprintf(t, sizeof(t), "%6ld", g->stars);
			cell(r.x + x[1] * cw, y, t, 6, P_DIM, sel, TX_LTR);
			cell(r.x + x[2] * cw, y, g->pushed, 10, P_DIM, sel, TX_LTR);
			if (cols - x[3] > 4)
				cell(r.x + x[3] * cw, y, g->desc, cols - x[3] - 1, P_TEXT, sel, 0);
		}
	}
	(void)clip;
}

static long found_at(WIN *w, short my)
{
	long row = w->top + (my - w->work.y - w->head_h) / ch;
	return my < w->work.y + w->head_h || row >= w->total ? -1 : row;
}

static void found_select(long row)
{
	long old = f_sel;
	f_sel = row;
	if (old >= 0)
		win_redraw_lines(&w_found, old, 1);
	if (row >= 0) {
		win_redraw_lines(&w_found, row, 1);
		win_ensure_visible(&w_found, row);
	}
}

/* the double click's action: an owner's repositories, or Follow */
static void found_open(long row)
{
	if (row < 0)
		return;
	if (showing_owners)
		owner_repos(f_owners[row].login);
	else
		follow(f_repos[row].full, f_repos[row].desc);
}

static void found_click(WIN *w, short mx, short my, short clicks, short kstate)
{
	long row = found_at(w, my);
	found_select(row);
	if (clicks >= 2)
		found_open(row);
}

static void found_rclick(WIN *w, short mx, short my)
{
	const char *items[5];
	short n = 0, r, back = -1;
	long row = found_at(w, my);
	if (row < 0) {
		static const char *const empty[] = { "Find on GitHub...  ^F" };
		if (popup(mx, my, empty, 1) == 0)
			ui_command(C_FIND);
		return;
	}
	found_select(row);
	items[n++] = showing_owners ? "Show the repositories" : "Follow...";
	items[n++] = "Copy name       ^C";
	if (!showing_owners && have_owners) {
		items[n++] = "-";
		back = n;
		items[n++] = "Back to the owners  Backspace";
	}
	r = popup(mx, my, items, n);
	if (r == 0)
		found_open(row);
	else if (r == 1)
		found_copy();
	else if (r >= 0 && r == back)
		found_back();
}

static int found_key(WIN *w, short kstate, short key)
{
	short scan = KEY_SCAN(key);
	switch (scan) {
	case 0x48:
		if (w->total)
			found_select(f_sel > 0 ? f_sel - 1 : 0);
		return 1;
	case 0x50:
		if (w->total)
			found_select(f_sel < w->total - 1 ? f_sel + 1 : w->total - 1);
		return 1;
	case 0x1c:
	case 0x72:
		found_open(f_sel);
		return 1;
	case 0x0e:			/* Backspace */
		found_back();
		return 1;
	case 0x52:			/* Insert */
		if (f_sel >= 0)
			found_rclick(w, w->work.x + 4 * cw, w->work.y + w->head_h + (short)((f_sel - w->top) * ch) + ch);
		return 1;
	}
	return 0;
}

static int found_tip(WIN *w, short mx, short my, char *out, int size)
{
	long row = found_at(w, my);
	if (row < 0 || showing_owners)
		return 0;
	snprintf(out, size, "%s%s|%s", f_repos[row].full, f_repos[row].archived ? " (archived)" :
		 f_repos[row].fork ? " (a fork)" : "", f_repos[row].desc[0] ? f_repos[row].desc : "No description");
	return 1;
}

void lists_init(void)
{
	str_copy(w_repos.title, "Following", sizeof(w_repos.title));
	w_repos.draw = repos_draw;
	w_repos.click = repos_click;
	w_repos.rclick = repos_rclick;
	w_repos.key = repos_key;
	w_repos.tip = repos_tip;
	w_repos.total = nrepos;
	str_copy(w_found.title, "Find", sizeof(w_found.title));
	str_copy(w_found.info, "^F looks on GitHub", sizeof(w_found.info));
	w_found.draw = found_draw;
	w_found.click = found_click;
	w_found.rclick = found_rclick;
	w_found.key = found_key;
	w_found.tip = found_tip;
}
