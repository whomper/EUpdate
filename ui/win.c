/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * win.c - EUpdate's window, EFTP's laid out for two lists.
 *
 * A toolbar along the top (Check, Update, Find; what is new; Stop), then
 * two panes: the repositories followed above, what a search found below.
 * The divider between them can be dragged, each pane has a header strip
 * and its own scroll bar, and the pane last clicked has the keyboard (its
 * header is drawn dark). The window's place and the divider go to
 * EUPDATE.INF.
 */
#include <string.h>
#include <stdio.h>
#include "ui.h"
#include "theme.h"
#include "util.h"
#include "bidi.h"
#include "iconify.h"
#include "panebar.h"

WIN w_repos, w_found;
static WIN *panes[] = { &w_repos, &w_found };
#define NPANE 2

short main_h = -1;
short main_iconified;			/* shown as EUpdate's icon (WM_ICONIFY) */
int running;
static GRECT mwork;			/* the window's work area */
static GRECT r_tool;			/* the toolbar */
static WIN *focus = &w_repos;
static char main_info[120];
static short sbw;			/* a pane bar's width: the AES's slider column */
static WIN *aes_pane = &w_repos;	/* the pane the window's own sliders scroll */
#define DIV 4				/* divider thickness */

/* the AES's sizer, never a corner of our own, and its slider column for
   the larger pane; the other has a bar of AES objects (common's
   panebar.c): the guidelines, 5.1 */
#define MAINKIND (NAME | CLOSER | FULLER | MOVER | SIZER | INFO | UPARROW | DNARROW | VSLIDE)

static int intersect(GRECT *a, GRECT *b)
{
	short x1 = a->x > b->x ? a->x : b->x;
	short y1 = a->y > b->y ? a->y : b->y;
	short x2 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
	short y2 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
	b->x = x1;
	b->y = y1;
	b->w = x2 - x1;
	b->h = y2 - y1;
	return b->w > 0 && b->h > 0;
}

static int inside(GRECT *r, short x, short y)
{
	return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

long win_rows(WIN *w)
{
	long r = (w->work.h - w->head_h) / ch;
	return r > 0 ? r : 1;
}

static short header_h(void)
{
	return ch + 4;
}

/* ---------------- the toolbar ---------------- */

enum { B_CHECK, B_UPDATE, B_FIND, B_STOP, NBUT };
static GRECT but[NBUT];
static GRECT r_note;			/* "2 new", or what runs */

static const char *label(int i)
{
	switch (i) {
	case B_CHECK: return "Check";
	case B_UPDATE: return "Update";
	case B_FIND: return "Find...";
	case B_STOP: return "Stop";
	}
	return "";
}

static int enabled(int i)
{
	switch (i) {
	case B_CHECK: return !running && nrepos > 0;
	case B_UPDATE: return !running && (repos_selected() > 0 || found_new_count() > 0);
	case B_FIND: return !running;
	case B_STOP: return running;
	}
	return 0;
}

static void tool_layout(void)
{
	short x, i, bw;
	r_tool = mwork;
	r_tool.h = ch + 8;
	x = mwork.x + 4;
	for (i = B_CHECK; i <= B_FIND; i++) {
		bw = ((short)strlen(label(i)) + 2) * cw;
		but[i].x = x;
		but[i].y = mwork.y + 3;
		but[i].w = bw;
		but[i].h = ch + 2;
		x += bw + (i == B_UPDATE ? cw : 2);
	}
	bw = 6 * cw;
	but[B_STOP].x = mwork.x + mwork.w - 4 - bw;
	but[B_STOP].y = mwork.y + 3;
	but[B_STOP].w = bw;
	but[B_STOP].h = ch + 2;
	r_note.x = x + cw;
	r_note.y = mwork.y + 4;
	r_note.h = ch;
	r_note.w = but[B_STOP].x - cw - r_note.x;
}

static void draw_button(int i)
{
	GRECT r = but[i];
	const char *l = label(i);
	short n = (short)strlen(l), on = enabled(i), tx = r.x + (r.w - n * cw) / 2;
	fill(&r, 0);
	frame(&r, on ? 1 : (colour ? pen[P_DIM] : 1));
	if ((i == B_STOP || (i == B_UPDATE && found_new_count())) && on && colour)
		text_pen_at(pen[i == B_STOP ? P_ERR : P_NEW], tx, r.y + 1, l, n, n, TX_LTR);
	else
		text_at(tx, r.y + 1, l, n, n, TX_LTR | (on ? 0 : TX_LIGHT));
}

static void draw_toolbar(GRECT *clip)
{
	GRECT r = r_tool;
	char t[60];
	short n, cols = r_note.w / cw, k = found_new_count();
	int i;
	if (!intersect(clip, &r))
		return;
	fill(&r, 0);
	for (i = 0; i < NBUT; i++)
		draw_button(i);
	/* how many have a newer release: orange, the one thing that matters;
	   the colour says it, not bold (Erez, 22 Oct 2026). Without colours
	   it is in plain letters, the rest light */
	if (k)
		snprintf(t, sizeof(t), "%d new release%s", k, k > 1 ? "s" : "");
	else
		snprintf(t, sizeof(t), nrepos ? "Following %d" : "Find a repository to follow", nrepos);
	n = (short)strlen(t);
	if (k && colour)
		text_pen_at(pen[P_NEW], r_note.x, r_note.y, t, n, cols, TX_LTR);
	else
		text_at(r_note.x, r_note.y, t, n, cols, TX_LTR | (k ? 0 : TX_LIGHT));
	hline(mwork.x, mwork.x + mwork.w - 1, mwork.y + r_tool.h - 1);
}

static void main_redraw(GRECT *area);

void toolbar_redraw(void)
{
	if (main_h > 0)
		main_redraw(&r_tool);
}

int toolbar_stop_hit(short mx, short my)
{
	return running && !main_iconified && inside(&but[B_STOP], mx, my);
}

/* a toolbar button: acts on the first click */
static int toolbar_click(short mx, short my)
{
	int i;
	for (i = 0; i < NBUT; i++) {
		GRECT r = but[i];
		short p[4];
		if (!inside(&r, mx, my))
			continue;
		if (!enabled(i) || i == B_STOP)
			return 1;
		/* pressed: inverted for a moment */
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, 0);
		clip_on(&r);
		p[0] = r.x + 1;
		p[1] = r.y + 1;
		p[2] = r.x + r.w - 2;
		p[3] = r.y + r.h - 2;
		vswr_mode(vdi_h, 3);
		vsf_interior(vdi_h, 1);
		vsf_color(vdi_h, 1);
		vr_recfl(vdi_h, p);
		vswr_mode(vdi_h, 1);
		graf_mouse(M_ON, 0);
		wind_update(END_UPDATE);
		evnt_timer_(80);
		main_redraw(&r);
		ui_command(i == B_CHECK ? C_CHECK : i == B_UPDATE ? C_UPDATE : C_FIND);
		return 1;
	}
	return inside(&r_tool, mx, my);
}

/* ---------------- layout ---------------- */

static void set_pane(WIN *w, short x, short y, short wd, short ht)
{
	w->box.x = x;
	w->box.y = y;
	w->box.w = wd;
	w->box.h = ht;
	w->work.x = x;
	w->work.y = y + header_h();
	w->work.w = wd - (w == aes_pane ? 0 : sbw);
	w->work.h = ht - header_h();
	if (w->work.h < 0)
		w->work.h = 0;
	w->head_h = ch;		/* the column titles */
}

/* the window's own sliders, from the pane they scroll (0-1000); what
   the AES has, so it isn't set again (-1: unknown, a new window) */
static short last_pos = -1, last_size = -1;

static void set_sliders(void)
{
	long rows = win_rows(aes_pane), max = aes_pane->total - rows;
	short pos = max > 0 ? (short)(aes_pane->top * 1000 / max) : 0;
	short size = aes_pane->total > rows ? (short)(rows * 1000 / aes_pane->total) : 1000;
	if (main_h <= 0 || main_iconified)
		return;
	if (size < 1)
		size = 1;
	if (size != last_size)
		wind_set(main_h, WF_VSLSIZE, size, 0, 0, 0);
	if (pos != last_pos)
		wind_set(main_h, WF_VSLIDE, pos, 0, 0, 0);
	last_size = size;
	last_pos = pos;
}

/* the panes in the window, from opt.pane_h */
static void layout(void)
{
	short top, avail, rh, i, minh = header_h() + 3 * ch;
	sbw = pb_width();
	tool_layout();
	top = mwork.y + r_tool.h;
	avail = mwork.h - r_tool.h;
	rh = opt.pane_h ? opt.pane_h : avail / 2;
	if (rh > avail - minh - DIV)
		rh = avail - minh - DIV;
	if (rh < minh)
		rh = minh;
	opt.pane_h = rh;
	aes_pane = rh >= avail - rh - DIV ? &w_repos : &w_found;	/* the larger */
	set_pane(&w_repos, mwork.x, top, mwork.w, rh);
	set_pane(&w_found, mwork.x, top + rh + DIV, mwork.w, avail - rh - DIV);
	for (i = 0; i < NPANE; i++) {
		WIN *w = panes[i];
		long max = w->total - win_rows(w);
		if (w->top > max)
			w->top = max > 0 ? max : 0;
	}
	set_sliders();
}

static GRECT div_h(void)
{
	GRECT r;
	r.x = mwork.x;
	r.y = w_repos.box.y + w_repos.box.h;
	r.w = mwork.w;
	r.h = DIV;
	return r;
}

/* ---------------- drawing ---------------- */

static GRECT sb_rect(WIN *w)
{
	GRECT r;
	r.x = w->box.x + w->box.w - sbw;
	r.y = w->work.y;
	r.w = w == aes_pane ? 0 : sbw;		/* the AES's, beside the window */
	r.h = w->work.h;
	return r;
}

/* a pane's bar, as it stands */
static PANEBAR bar_of(WIN *w)
{
	PANEBAR b;
	memset(&b, 0, sizeof(b));	/* a bar down the pane's side */
	b.r = sb_rect(w);
	b.total = w->total;
	b.shown = win_rows(w);
	b.top = w->top;
	return b;
}

static void draw_header(WIN *w)
{
	GRECT h;
	short dark = (w == focus), cols = w->box.w / cw - 1;
	short il = (short)strlen(w->info), tl = (short)strlen(w->title);
	h.x = w->box.x;
	h.y = w->box.y;
	h.w = w->box.w;
	h.h = header_h();
	fill(&h, dark ? pen[P_HEAD] : pen[P_PANEL]);
	if (tl > cols - 2)
		tl = cols - 2;
	if (il > cols - tl - 2)
		il = cols - tl - 2;
	text_at(h.x + cw / 2, h.y + 2, w->title, tl, cols, TX_BOLD | TX_LTR | (dark ? TX_INVERSE : 0));
	if (il > 0) {
		if (dark || !colour)
			text_at(h.x + w->box.w - (il + 1) * cw, h.y + 2, w->info, il, il,
				TX_LTR | (dark ? TX_INVERSE : TX_LIGHT));
		else
			text_pen_at(pen[P_DIM], h.x + w->box.w - (il + 1) * cw, h.y + 2, w->info, il, il, TX_LTR);
	}
	hline(h.x, h.x + h.w - 1, h.y + h.h - 1);
}

static void draw_divider(GRECT *r)
{
	short p[4];
	if (colour) {
		fill(r, pen[P_PANEL]);
		return;
	}
	fill(r, 0);
	vsf_interior(vdi_h, 2);
	vsf_style(vdi_h, 4);
	vsf_color(vdi_h, 1);
	p[0] = r->x;
	p[1] = r->y;
	p[2] = r->x + r->w - 1;
	p[3] = r->y + r->h - 1;
	vswr_mode(vdi_h, 1);
	vr_recfl(vdi_h, p);
}

/* everything of the window inside clip (already set as VDI clip) */
static void draw_main(GRECT *clip)
{
	short i;
	GRECT r;
	draw_toolbar(clip);
	for (i = 0; i < NPANE; i++) {
		WIN *w = panes[i];
		r = w->box;
		if (!intersect(clip, &r))
			continue;
		r = w->box;
		r.h = header_h();
		if (intersect(clip, &r))
			draw_header(w);
		r = w->work;
		if (intersect(clip, &r) && w->draw) {
			clip_on(&r);
			w->draw(w, &r);
			clip_on(clip);
		}
		r = sb_rect(w);
		if (intersect(clip, &r)) {
			PANEBAR b = bar_of(w);
			pb_draw(&b, &r);
		}
	}
	r = div_h();
	if (intersect(clip, &r))
		draw_divider(&r);
}

/* redraw area of the window through its rectangle list */
static void main_redraw(GRECT *area)
{
	GRECT r, a;
	if (main_h <= 0)
		return;
	if (main_iconified) {	/* the icon, not the panes (area is the old window's) */
		icon_window(0);
		return;
	}
	a = area ? *area : mwork;
	if (!intersect(&mwork, &a))
		return;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	wind_get(main_h, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		GRECT c = r;
		if (intersect(&a, &c)) {
			clip_on(&c);
			draw_main(&c);
		}
		wind_get(main_h, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
}

void win_redraw(WIN *w, GRECT *area)
{
	GRECT a = area ? *area : w->work;
	if (intersect(&w->box, &a))
		main_redraw(&a);
}

void win_redraw_all(void)
{
	main_redraw(0);
}

void win_redraw_lines(WIN *w, long first, long n)
{
	GRECT a;
	long y = (first - w->top) * ch;
	if (y < 0) {
		n += y / ch;
		y = 0;
	}
	if (n <= 0)
		return;
	a.x = w->work.x;
	a.y = w->work.y + w->head_h + (short)y;
	a.w = w->work.w;
	a.h = (short)(n * ch);
	win_redraw(w, &a);
}

/* the number of lines changed: the scroll bar and all of the lines */
void win_lines_changed(WIN *w)
{
	long max = w->total - win_rows(w);
	if (w->top > max)
		w->top = max > 0 ? max : 0;
	win_redraw(w, 0);
	if (w == aes_pane) {
		set_sliders();
	} else {
		GRECT s = sb_rect(w);
		main_redraw(&s);
	}
}

/* the window's info line: GEM draws it as stored, so Hebrew is laid out
   right to left first */
void main_status(const char *s)
{
	char v[sizeof(main_info)];
	short n = (short)strlen(s);
	if (n >= (short)sizeof(v))
		n = sizeof(v) - 1;
	if (n > BIDI_MAX)
		n = BIDI_MAX;
	if (bidi_has_rtl(s, n)) {
		bidi_visual(s, n, 0, v);
		v[n] = 0;
	} else {
		str_copy(v, s, sizeof(v));
	}
	heb_font_map(v, (long)strlen(v), opt.hebfont);
	if (!strcmp(v, main_info))
		return;
	str_copy(main_info, v, sizeof(main_info));
	if (main_h > 0)
		wind_set_str(main_h, WF_INFO, main_info);
}

static void redraw_header(WIN *w)
{
	GRECT h = w->box;
	h.h = header_h();
	main_redraw(&h);
}

void win_title(WIN *w, const char *title)
{
	if (strcmp(w->title, title)) {
		str_copy(w->title, title, sizeof(w->title));
		redraw_header(w);
	}
}

void win_info(WIN *w, const char *info)
{
	if (strcmp(w->info, info)) {
		str_copy(w->info, info, sizeof(w->info));
		redraw_header(w);
	}
}

/* ---------------- opening and closing ---------------- */

static void get_main_work(void)
{
	wind_get(main_h, WF_WORKXYWH, &mwork.x, &mwork.y, &mwork.w, &mwork.h);
}

static void save_main_place(void)
{
	wind_get(main_h, WF_CURRXYWH, &opt.main_x, &opt.main_y, &opt.main_w, &opt.main_h);
}

void main_open(void)
{
	short x = opt.main_x, y = opt.main_y, wd = opt.main_w, ht = opt.main_h;
	if (main_h > 0) {
		wind_set(main_h, WF_TOP, 0, 0, 0, 0);
		return;
	}
	/* the saved place, if it still fits this screen */
	if (wd < 50 * cw || ht < 16 * ch || x < 0 || y < desk_y ||
	    x + wd > desk_x + desk_w + 4 || y + ht > desk_y + desk_h + 4) {
		x = desk_x;
		y = desk_y;
		wd = desk_w;
		ht = desk_h;
	}
	main_h = wind_create(icon_kind(MAINKIND), desk_x, desk_y, desk_w, desk_h);
	main_iconified = 0;
	if (main_h < 0) {
		alert(1, "[3][No more windows available.][ OK ]");
		return;
	}
	wind_set_str(main_h, WF_NAME, " EUpdate ");
	wind_set_str(main_h, WF_INFO, main_info);
	wind_open(main_h, x, y, wd, ht);
	last_pos = last_size = -1;
	get_main_work();
	save_main_place();
	layout();
}

void main_close(void)
{
	if (main_h <= 0)
		return;
	if (!main_iconified)	/* the place was kept when it was iconified */
		save_main_place();
	wind_close(main_h);
	wind_delete(main_h);
	main_h = -1;
}

/* the font changed: lay the panes out again */
void win_relayout(void)
{
	if (main_h > 0 && !main_iconified) {
		layout();
		main_redraw(0);
	}
}

/* keyboard focus to a pane (its header goes dark) */
void win_focus(WIN *w)
{
	WIN *old = focus;
	if (w == focus)
		return;
	focus = w;
	redraw_header(old);
	redraw_header(w);
}

WIN *win_focused(void)
{
	return focus;
}

static int win_is_top(void)
{
	short t, d;
	wind_get(0, WF_TOP, &t, &d, &d, &d);
	return main_h > 0 && t == main_h;
}

/* ---------------- scrolling ---------------- */

void win_scroll_to(WIN *w, long top)
{
	long max = w->total - win_rows(w);
	GRECT a, s;
	if (max < 0)
		max = 0;
	if (top > max)
		top = max;
	if (top < 0)
		top = 0;
	if (top == w->top)
		return;
	if (w == aes_pane) {
		w->top = top;
		set_sliders();
		s.w = 0;
	} else {
		PANEBAR b = bar_of(w);
		long old = w->top;
		b.top = w->top = top;
		s = pb_moved(&b, old);
	}
	if (s.w > 0)
		main_redraw(&s);
	a = w->work;
	a.y += w->head_h;
	a.h -= w->head_h;
	win_redraw(w, &a);
}

void win_ensure_visible(WIN *w, long line)
{
	long rows = win_rows(w);
	if (line < w->top)
		win_scroll_to(w, line);
	else if (line >= w->top + rows)
		win_scroll_to(w, line - rows + 1);
}

/* the pointer shape win_hover() last set; drags set it back to ARROW */
static short hover_shape = ARROW;

static short button_down(short *mx, short *my)
{
	short mb, ks;
	graf_mkstate(mx, my, &mb, &ks);
	return mb & 3;
}

/* a press on a pane's bar: pb_click does the arrows, the track and the
   slider, through these two */
static WIN *bar_pane;
static PANEBAR bar_pressed;

static int bar_scroll(long n)
{
	long old = bar_pane->top;
	win_scroll_to(bar_pane, old + n);
	bar_pressed.top = bar_pane->top;
	return bar_pane->top != old;
}

static int bar_goto(long t)
{
	win_scroll_to(bar_pane, t);
	bar_pressed.top = bar_pane->top;
	return 1;
}

static void scrollbar_press(WIN *w, short mx, short my)
{
	bar_pane = w;
	bar_pressed = bar_of(w);
	pb_click(&bar_pressed, mx, my, bar_scroll, bar_goto);
}

/* drag the divider between the two lists */
static void divider_drag(void)
{
	short mx, my, last = -1, p[4], held;
	GRECT lim = mwork;
	wind_update(BEG_UPDATE);
	wind_update(3);
	graf_mouse(FLAT_HAND, 0);
	clip_on(&lim);
	vswr_mode(vdi_h, 3);			/* XOR */
	vsl_color(vdi_h, 1);
	/* do..while: the press may have been reported only after the
	   release, then the pointer's place is where it was dropped */
	do {
		held = button_down(&mx, &my);
		if (my != last) {
			graf_mouse(M_OFF, 0);
			if (last >= 0)
				v_pline(vdi_h, 2, p);
			p[0] = mwork.x;
			p[2] = mwork.x + mwork.w - 1;
			p[1] = p[3] = my;
			v_pline(vdi_h, 2, p);
			graf_mouse(M_ON, 0);
			last = my;
		}
		evnt_timer_(10);
	} while (held);
	if (last >= 0) {
		graf_mouse(M_OFF, 0);
		v_pline(vdi_h, 2, p);
		graf_mouse(M_ON, 0);
	}
	vswr_mode(vdi_h, 1);
	graf_mouse(ARROW, 0);
	hover_shape = ARROW;
	wind_update(2);
	wind_update(END_UPDATE);
	if (last < 0)
		return;
	opt.pane_h = last - (mwork.y + r_tool.h);
	layout();
	main_redraw(0);
}

/* where the divider can be grabbed: a little wider than it is drawn */
static int on_divider(short mx, short my)
{
	GRECT r = div_h();
	if (main_h <= 0)
		return 0;
	r.y -= 3;
	r.h += 6;
	return inside(&r, mx, my);
}

/* the pointer shows the flat hand over the divider */
void win_hover(short mx, short my)
{
	short want = ARROW;
	if (wind_find(mx, my) == main_h && win_is_top() && !running && on_divider(mx, my))
		want = FLAT_HAND;
	if (want != hover_shape) {
		graf_mouse(want, 0);
		hover_shape = want;
	}
	/* the next move wakes us again */
	evnt_set_m1(1, mx, my, 1, 1);
}

/* a mouse press anywhere; returns 1 if it was ours */
int win_mouse(short mx, short my, short button, short clicks, short kstate)
{
	short i;
	GRECT r;
	if (main_h <= 0 || wind_find(mx, my) != main_h)
		return 0;
	if (toolbar_click(mx, my))
		return 1;
	if (on_divider(mx, my)) {
		divider_drag();
		return 1;
	}
	for (i = 0; i < NPANE; i++) {
		WIN *w = panes[i];
		if (!inside(&w->box, mx, my))
			continue;
		win_focus(w);
		r = sb_rect(w);
		if (inside(&r, mx, my)) {
			scrollbar_press(w, mx, my);
		} else if (inside(&w->work, mx, my)) {
			if ((button & 2) && w->rclick)
				w->rclick(w, mx, my);
			else if (w->click)
				w->click(w, mx, my, clicks, kstate);
		}
		return 1;
	}
	return 1;
}

/* ---------------- tooltips ---------------- */

static const char *const btip[NBUT] = {
	"Ask GitHub for the latest releases of the repositories (^R)",
	"Download the newer releases: of those selected, else of all (^U)",
	"Find repositories by name, or owners and their repositories (^F)",
	"Stop (Esc)",
};

int win_tip(short mx, short my, char *out, int size)
{
	int i;
	if (!win_is_top())
		return 0;
	for (i = 0; i < NBUT; i++)
		if (inside(&but[i], mx, my)) {
			str_copy(out, btip[i], size);
			return 1;
		}
	for (i = 0; i < NPANE; i++)
		if (inside(&panes[i]->work, mx, my) && panes[i]->tip)
			return panes[i]->tip(panes[i], mx, my, out, size);
	return 0;
}

/* ---------------- AES messages ---------------- */

void main_closed(void);

void win_message(short *msg)
{
	GRECT r;
	if (msg[3] != main_h || main_h <= 0)
		return;
	switch (msg[0]) {
	case WM_REDRAW:
		r.x = msg[4];
		r.y = msg[5];
		r.w = msg[6];
		r.h = msg[7];
		if (main_iconified)
			icon_window(&r);
		else
			main_redraw(&r);
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (main_iconified)
			break;
		save_main_place();
		main_iconified = 1;
		wind_set(main_h, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
		break;
	case WM_UNICONIFY:
		if (!main_iconified)
			break;
		wind_set(main_h, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
		main_iconified = 0;
		last_pos = last_size = -1;
		get_main_work();
		layout();
		main_redraw(0);
		break;
	case WM_TOPPED:
	case WM_NEWTOP: {
		short mx, my, mb, ks;
		wind_set(main_h, WF_TOP, 0, 0, 0, 0);
		/* the AES tops a window on a click in its contents and keeps
		   the click: act on it as well, as a modern program does */
		graf_mkstate(&mx, &my, &mb, &ks);
		if (msg[0] == WM_TOPPED && !running && !main_iconified && inside(&mwork, mx, my))
			win_mouse(mx, my, mb ? mb : 1, 1, ks);
		break;
	}
	case WM_CLOSED:
		if (!running)
			main_closed();
		break;
	case WM_ARROWED:		/* the window's own sliders: the larger pane */
		if (!main_iconified) {
			long page = win_rows(aes_pane) - 1;
			if (page < 1)
				page = 1;
			if (msg[4] >= WA_UPPAGE && msg[4] <= WA_DNLINE)
				win_scroll_to(aes_pane, aes_pane->top + (msg[4] == WA_UPPAGE ? -page :
					      msg[4] == WA_DNPAGE ? page : msg[4] == WA_UPLINE ? -1 : 1));
		}
		break;
	case WM_VSLID:
		if (!main_iconified) {
			long max = aes_pane->total - win_rows(aes_pane);
			win_scroll_to(aes_pane, max > 0 ? (msg[4] * max + 500) / 1000 : 0);
		}
		break;
	case WM_BOTTOMED:
		wind_set(main_h, WF_BOTTOM, 0, 0, 0, 0);
		break;
	case WM_FULLED: {
		GRECT c, f, p;
		if (main_iconified)
			break;
		wind_get(main_h, WF_CURRXYWH, &c.x, &c.y, &c.w, &c.h);
		wind_get(main_h, WF_FULLXYWH, &f.x, &f.y, &f.w, &f.h);
		if (c.x == f.x && c.y == f.y && c.w == f.w && c.h == f.h) {
			wind_get(main_h, WF_PREVXYWH, &p.x, &p.y, &p.w, &p.h);
			wind_set(main_h, WF_CURRXYWH, p.x, p.y, p.w, p.h);
		} else {
			wind_set(main_h, WF_CURRXYWH, f.x, f.y, f.w, f.h);
		}
		get_main_work();
		save_main_place();
		layout();
		main_redraw(0);
		break;
	}
	case WM_SIZED:
	case WM_MOVED:
		if (main_iconified) {	/* the icon goes where it is put, as it is */
			wind_set(main_h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			break;
		}
		if (msg[6] < 50 * cw)
			msg[6] = 50 * cw;
		if (msg[7] < 16 * ch)
			msg[7] = 16 * ch;
		wind_set(main_h, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
		get_main_work();
		save_main_place();
		layout();
		if (msg[0] == WM_SIZED)
			main_redraw(0);
		break;
	}
	if (!running)
		menu_update();
}
