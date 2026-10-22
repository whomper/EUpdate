/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * dialogs.c - EUpdate's dialogs, built in code (no .RSC file needed),
 * with EFTP's and EMail's builder: Find, Follow and Get info (where the
 * files go), Settings, the keys.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ui.h"
#include "bidi.h"
#include "util.h"
#include "clip.h"
#include "field.h"
#include "theme.h"

#define DMAX 80
#define TMAX 24

static OBJECT tree[DMAX];
static void make_eyes(void);
static short npw;
static TEDINFO ted[TMAX];
static char tmpl[TMAX][64], valid[TMAX][64];
static short nobj, nted, last_child;
/* the fields that hold more than they show (field.c, the guidelines 5.4):
   the Follow dialog has ten */
#define NLF 12
static LFIELD lfs[NLF];
static short nlf;

/* ---------------- builder ---------------- */

static void d_begin(short w, short h)
{
	static short eyes_made;
	if (!eyes_made) {
		make_eyes();
		eyes_made = 1;
	}
	npw = 0;
	nlf = 0;
	lf_forget(tree);
	memset(tree, 0, sizeof(tree));
	nobj = 1;
	nted = 0;
	last_child = 0;
	tree[0].ob_next = -1;
	tree[0].ob_head = tree[0].ob_tail = -1;
	tree[0].ob_type = G_BOX;
	tree[0].ob_state = OUTLINED;
	tree[0].ob_spec = 0x00021100L;
	tree[0].ob_width = w;
	tree[0].ob_height = h;
}

static short d_add(short type, short flags, short state, long spec, short x, short y, short w, short h)
{
	short i = nobj++;
	OBJECT *o = &tree[i];
	o->ob_next = 0;
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_state = state;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
	if (last_child)
		tree[last_child].ob_next = i;
	else
		tree[0].ob_head = i;
	tree[0].ob_tail = i;
	last_child = i;
	return i;
}

static short d_text(short x, short y, const char *s)
{
	return d_add(G_STRING, 0, 0, (long)s, x, y, (short)strlen(s), 1);
}

/* an editable field showing show characters of buf, which holds cap
   (cap + 1 bytes); when that is more, the field scrolls (field.c) */
static short d_edit(short x, short y, char *buf, short show, short cap, char kind)
{
	TEDINFO *t = &ted[nted];
	short i, o;
	if (show > 60)
		show = 60;
	for (i = 0; i < show; i++) {
		tmpl[nted][i] = '_';
		valid[nted][i] = kind;
	}
	tmpl[nted][show] = valid[nted][show] = 0;
	if (cap > show && nlf == NLF)
		cap = show;			/* no room for one more: as GEM's */
	if (cap <= show)
		buf[cap] = 0;
	t->te_ptext = buf;
	t->te_ptmplt = tmpl[nted];
	t->te_pvalid = valid[nted];
	t->te_font = 3;
	t->te_just = 0;
	t->te_color = 0x1180;
	t->te_thickness = 0;
	t->te_txtlen = (cap < show ? cap : show) + 1;
	t->te_tmplen = show + 1;
	nted++;
	o = d_add(G_FTEXT, EDITABLE, 0, (long)t, x, y, show, 1);
	if (cap > show)
		lf_init(&lfs[nlf++], tree, o, buf, cap, 0);
	return o;
}

static short d_button(short x, short y, short w, const char *s, short flags)
{
	return d_add(G_BUTTON, SELECTABLE | flags, 0, (long)s, x, y, w, 1);
}

static void d_end(void)
{
	short i;
	tree[last_child].ob_next = 0;
	tree[nobj - 1].ob_flags |= LASTOB;
	for (i = 0; i < nobj; i++)
		rsrc_obfix(tree, i);
}

/* ---------------- password fields ----------------
 * GEM's text fields can't hide what they show, so a password field is a
 * long field (field.c) shown as stars; the eye button next to it shows
 * it in clear. form_loop() below is form_do() written out with
 * form_keybd/form_button/objc_edit, so the long fields take their keys
 * there. */

#define PWMAX 2

typedef struct {
	short obj, eye;		/* the text field and its eye button */
	LFIELD *lf;		/* the password, kept aside */
	short clear;		/* 1: shown in clear */
} PWFIELD;

static PWFIELD pwf[PWMAX];

/* the eye: 16 pixels wide, 16 rows (8 on ST medium), open and crossed */
static const char *const eye_art[16] = {
	"................",
	"................",
	"................",
	".....######.....",
	"...##......##...",
	"..#....##....#..",
	".#....####....#.",
	"#.....####.....#",
	".#....####....#.",
	"..#....##....#..",
	"...##......##...",
	".....######.....",
	"................",
	"................",
	"................",
	"................",
};
static unsigned short eye_open[16], eye_shut[16];

/* a tick box, 16 pixels square, and the tick in it */
static const char *tick_art[16] = {
	"................",
	".##############.",
	".#............#.",
	".#..........#.#.",
	".#.........##.#.",
	".#........##..#.",
	".#.......##...#.",
	".#.##...##....#.",
	".#.###.##.....#.",
	".#..####......#.",
	".#...##.......#.",
	".#............#.",
	".#............#.",
	".#............#.",
	".##############.",
	"................",
};
static unsigned short tick_on[16], tick_off[16];
static BITBLK tick_on_blk, tick_off_blk;

static void make_ticks(void)
{
	/* half height on 8-pixel screens: every other line, and the frame */
	short tall = gl_hchar >= 16, rows = tall ? 16 : 8, r, c;
	for (r = 0; r < rows; r++) {
		const char *src = tick_art[tall ? r : (r == rows - 1 ? 14 : r * 2 + (r ? 0 : 1))];
		unsigned short v = 0, frame = 0;
		for (c = 0; c < 16; c++) {
			v = (unsigned short)((v << 1) | (src[c] == '#'));
			frame = (unsigned short)((frame << 1) | (src[c] == '#' && (c == 1 || c == 14 || r == 0 || r == rows - 1 ||
										(tall && (r == 1 || r == 14)))));
		}
		tick_on[r] = v;
		tick_off[r] = tall ? (r == 1 || r == 14 ? v : (r == 0 || r == 15 ? 0 : 0x4002)) : (r == 0 || r == rows - 1 ? v : 0x4002);
	}
	tick_on_blk.bi_pdata = tick_on;
	tick_off_blk.bi_pdata = tick_off;
	tick_on_blk.bi_wb = tick_off_blk.bi_wb = 2;
	tick_on_blk.bi_hl = tick_off_blk.bi_hl = rows;
	tick_on_blk.bi_color = tick_off_blk.bi_color = 1;
}
static BITBLK eye_blk[PWMAX];

static void make_eyes(void)
{
	short tall = gl_hchar >= 16, r, rows = tall ? 16 : 8, c;
	for (r = 0; r < rows; r++) {
		const char *src = eye_art[tall ? r : r * 2 + 1];
		unsigned short v = 0;
		for (c = 0; c < 16; c++)
			v = (v << 1) | (src[c] == '#');
		eye_open[r] = v;
		/* crossed out: a diagonal from bottom left to top right */
		eye_shut[r] = v | (0x8000 >> ((rows - 1 - r) * 16 / rows)) | (0x4000 >> ((rows - 1 - r) * 16 / rows));
	}
}

/* a password field showing show characters of real, which holds cap */
static short d_pass(short x, short y, char *real, short show, short cap)
{
	PWFIELD *p = &pwf[npw];
	BITBLK *b = &eye_blk[npw];
	short f = d_edit(x, y, real, show, cap, 'X');
	p->obj = f;
	p->clear = 0;
	p->lf = lf_of(tree, f);
	if (p->lf) {
		p->lf->mask = '*';
		lf_view(p->lf);
	}
	b->bi_pdata = eye_open;
	b->bi_wb = 2;
	b->bi_hl = gl_hchar >= 16 ? 16 : 8;
	b->bi_x = b->bi_y = 0;
	b->bi_color = 1;
	p->eye = d_add(G_IMAGE, TOUCHEXIT, 0, (long)b, x + show + 1, y, 2, 1);
	npw++;
	return f;
}

static void draw_obj(short obj)
{
	short x, y;
	objc_offset(tree, obj, &x, &y);
	objc_draw(tree, obj, 0, x - 2, y - 2, tree[obj].ob_width + 4, tree[obj].ob_height + 4);
}

static short first_editable(void)
{
	short i;
	for (i = 1; i < nobj; i++)
		if (tree[i].ob_flags & EDITABLE)
			return i;
	return 0;
}

/* what Esc and Undo press (the guidelines, 3.5): Cancel, else the only
   button that ends the dialog (OK in a box that only says something); 0
   when neither is there, and then they do nothing */
static short esc_button(void)
{
	short i, exits = 0, last = 0;
	for (i = 1; i < nobj; i++) {
		if (tree[i].ob_type != G_BUTTON || !(tree[i].ob_flags & EXIT))
			continue;
		if (!strcmp((const char *)tree[i].ob_spec, "Cancel"))
			return i;
		exits++;
		last = i;
	}
	return exits == 1 ? last : 0;
}

/* the cursor into a field: where its text ends */
static void field_enter(short obj, short *idx)
{
	LFIELD *lf = lf_of(tree, obj);
	if (lf)
		lf_enter(lf, idx);
	else
		objc_edit(tree, obj, 0, idx, ED_INIT);
}

/* form_do, with fields longer than they show (passwords among them), the
   clipboard (^C ^X ^V), and Esc and Undo for Cancel (the AERA
   guidelines, 3.5): never anything that acts, and never clearing the
   field the cursor is in, as form_do's Esc does */
static short form_loop(short start)
{
	short next = start ? start : first_editable(), edit = 0, idx = 0, cont = 1;
	short m[8];
	AEVENT e;
	while (cont) {
		LFIELD *lf;
		if (next && next != edit) {
			edit = next;
			next = 0;
			field_enter(edit, &idx);
		}
		lf = edit ? lf_of(tree, edit) : 0;
		evnt_multi_(MU_KEYBD | MU_BUTTON, 2, 1, 1, 0, m, &e);
		if (e.which & MU_KEYBD) {
			short scan = KEY_SCAN(e.kreturn);
			short nav = scan == 0x1c || scan == 0x72 || scan == 0x0f || scan == 0x48 || scan == 0x50;
			/* the Hebrew keyboard (F10) types into the fields too, not
			   into a password */
			short ch = lf && lf->mask ? KEY_ASCII(e.kreturn) : key_char(e.kstate, e.kreturn);
			if (scan == 0x01 || scan == 0x61) {	/* Esc, Undo */
				if (esc_button()) {
					next = esc_button();	/* as if it were clicked */
					cont = 0;
				}
			} else if (lf && !nav && lf_key(lf, e.kreturn, ch, e.kstate, &idx)) {
				/* the long field took it */
			} else if (edit && !lf && clip_key(e.kreturn)) {
				clip_edit(tree, edit, &idx, e.kreturn);
			} else {
				short kr, k = (short)((e.kreturn & 0xff00) | (nav ? KEY_ASCII(e.kreturn) : ch));
				cont = form_keybd(tree, edit, next, k, &next, &kr);
				if (kr && !lf)		/* a long field's text isn't objc_edit's */
					objc_edit(tree, edit, kr, &idx, ED_CHAR);
			}
		}
		if (e.which & MU_BUTTON) {
			short obj = objc_find(tree, 0, 8, e.mx, e.my), i;
			PWFIELD *eye = 0;
			for (i = 0; i < npw; i++)
				if (pwf[i].eye == obj && obj > 0)
					eye = &pwf[i];
			if (eye) {
				/* show or hide the password; the dialog stays */
				if (edit)
					objc_edit(tree, edit, 0, &idx, ED_END);
				eye->clear = !eye->clear;
				eye_blk[eye - pwf].bi_pdata = eye->clear ? eye_shut : eye_open;
				if (eye->lf)
					lf_mask(eye->lf, eye->clear ? 0 : '*', &idx, 0);
				draw_obj(eye->eye);
				if (edit)
					field_enter(edit, &idx);
				evnt_timer_(150);
			} else if (obj < 0) {
				next = 0;
			} else {
				cont = form_button(tree, obj, e.breturn, &next);
			}
		}
		if (!cont || (next && next != edit))
			if (edit)
				objc_edit(tree, edit, 0, &idx, ED_END);
	}
	return next;
}

static short d_do(short edit)
{
	short x, y, w, h, r;
	form_center(tree, &x, &y, &w, &h);
	wind_update(BEG_UPDATE);
	wind_update(3);			/* BEG_MCTRL: the dialog owns the mouse */
	form_dial(FMD_START, x, y, w, h);
	objc_draw(tree, 0, 8, x, y, w, h);
	r = form_loop(edit) & 0x7fff;
	form_dial(FMD_FINISH, x, y, w, h);
	wind_update(2);
	wind_update(END_UPDATE);
	tree[r].ob_state &= ~SELECTED;
	npw = 0;
	return r;
}

/* ---------------- settings ---------------- */

/* ---------------- settings ----------------
 * EMail's Settings dialog: a black title band, a list of parts on the
 * left (the chosen one in a black bar) and that part's settings on the
 * right: ticks (a box that is crossed when on, clicked on the box or its
 * words), framed fields and choice buttons that open a list. */

enum {
	T_FETCH, T_SHORTEN, T_COLOUR, T_HEB, T_LOG, T_DSP, T_UNZIP, T_NOW,
	/* choices, one of a group: when to check; what Find looks for */
	T_NEVER, T_START, T_EVERY, T_REPOS, T_OWNERS, NT
};
static short s_on[NT];

/* the group of choices a tick belongs to, 0 for none */
static int group_of(short var)
{
	return var >= T_REPOS ? 2 : var >= T_NEVER ? 1 : 0;
}
#define NTICK 12
static short tick_box[NTICK], tick_lbl[NTICK], tick_var[NTICK], nticks;
static short page_of[DMAX], npages, cur_page, page_tab[6];
static GRECT page_area;

static TEDINFO *d_ted(const char *text, short just, short color)
{
	TEDINFO *t = &ted[nted++];
	t->te_ptext = (char *)text;
	t->te_ptmplt = "";
	t->te_pvalid = "";
	t->te_font = 3;
	t->te_just = just;
	t->te_color = color;
	t->te_thickness = 0;
	t->te_txtlen = (short)strlen(text) + 1;
	t->te_tmplen = 1;
	return t;
}

/* black band across the top, white title */
static void d_band(short w, const char *title)
{
	d_add(G_BOXTEXT, 0, 0, (long)d_ted(title, 0, 0x1071), 0, 0, w, 1);
}

static short d_tick(short x, short y, const char *label, short var)
{
	short k = nticks++;
	tick_var[k] = var;
	if (!tick_on_blk.bi_pdata)
		make_ticks();
	tick_box[k] = d_add(G_IMAGE, TOUCHEXIT, 0, (long)(s_on[var] ? &tick_on_blk : &tick_off_blk), x, y, 2, 1);
	tick_lbl[k] = d_add(G_STRING, TOUCHEXIT, 0, (long)label, x + 3, y, (short)strlen(label), 1);
	return k;
}

static void tick_draw(short k)
{
	OBJECT *o = &tree[tick_box[k]];
	short x, y;
	o->ob_spec = (long)(s_on[tick_var[k]] ? &tick_on_blk : &tick_off_blk);
	objc_offset(tree, tick_box[k], &x, &y);
	objc_draw(tree, 0, 8, x, y, o->ob_width, o->ob_height);
}

/* a field in a thin frame */
static short d_field(short x, short y, char *buf, short show, short cap, char kind)
{
	short i = d_edit(x, y, buf, show, cap, kind);
	TEDINFO *t = (TEDINFO *)tree[i].ob_spec;
	tree[i].ob_type = G_FBOXTEXT;
	t->te_thickness = -1;
	t->te_color = 0x1180;
	return i;
}

/* rows of a part, with some air between them: row n of a part starting
   at line y is n lines and n gaps down (6 pixels with the 16-pixel font) */
static short row_y0, row_gap;
#define ROW(n) ((short)((row_y0 + (n)) | ((2 + (n) * row_gap) << 8)))

static void rows_at(short y)
{
	row_y0 = y;
	row_gap = (short)(gl_hchar * 3 / 8);
}

/* objects added from here on belong to page p (0: always shown) */
static short adding_page;

static void page_mark(short from)
{
	short i;
	for (i = from; i < nobj; i++)
		page_of[i] = adding_page;
}

static void show_page(short p, int draw)
{
	short i;
	cur_page = p;
	for (i = 1; i < nobj; i++) {
		if (!page_of[i])
			continue;
		if (page_of[i] == p) {
			tree[i].ob_flags &= ~HIDETREE;
			if (tree[i].ob_type == G_FTEXT || tree[i].ob_type == G_FBOXTEXT)
				tree[i].ob_flags |= EDITABLE;
		} else {
			tree[i].ob_flags |= HIDETREE;
			tree[i].ob_flags &= ~EDITABLE;
		}
	}
	for (i = 1; i <= npages; i++)
		tree[page_tab[i]].ob_state = i == p ? SELECTED : 0;
	if (draw) {
		short x, y, k;
		for (k = 1; k <= npages; k++) {
			objc_offset(tree, page_tab[k], &x, &y);
			objc_draw(tree, 0, 8, x - 2, y - 2, tree[page_tab[k]].ob_width + 4,
				  tree[page_tab[k]].ob_height + 4);
		}
		/* a framed field's frame lies a pixel or two outside it */
		objc_draw(tree, 0, 8, page_area.x - 2, page_area.y - 2, page_area.w + 4, page_area.h + 4);
	}
}

static short first_edit(void)
{
	short i;
	for (i = 1; i < nobj; i++)
		if ((tree[i].ob_flags & EDITABLE) && !(tree[i].ob_flags & HIDETREE))
			return i;
	return 0;
}

/* a dialog that stays open while its own buttons act (ticks, pages, a
   folder chosen): drawn once, then form_loop again and again */
static GRECT open_at;

static void open_box(void)
{
	form_center(tree, &open_at.x, &open_at.y, &open_at.w, &open_at.h);
	wind_update(BEG_UPDATE);
	wind_update(3);			/* BEG_MCTRL: the dialog owns the mouse */
	form_dial(FMD_START, open_at.x, open_at.y, open_at.w, open_at.h);
	objc_draw(tree, 0, 8, open_at.x, open_at.y, open_at.w, open_at.h);
}

static void close_box(short r)
{
	form_dial(FMD_FINISH, open_at.x, open_at.y, open_at.w, open_at.h);
	wind_update(2);
	wind_update(END_UPDATE);
	tree[r].ob_state &= ~SELECTED;
	npw = 0;
}

/* a tick clicked (its box or its words): turned over, or, in a group
   of choices, chosen; 1 if r was one */
static int tick_hit(short r)
{
	short k, j;
	for (k = 0; k < nticks; k++)
		if (r == tick_box[k] || r == tick_lbl[k])
			break;
	if (k == nticks)
		return 0;
	tree[r].ob_state &= ~SELECTED;
	if (group_of(tick_var[k])) {
		for (j = 0; j < nticks; j++)
			if (group_of(tick_var[j]) == group_of(tick_var[k]) && s_on[tick_var[j]] != (j == k)) {
				s_on[tick_var[j]] = j == k;
				tick_draw(j);
			}
	} else {
		s_on[tick_var[k]] = !s_on[tick_var[k]];
		tick_draw(k);
	}
	evnt_timer_(120);
	return 1;
}

/* ---------------- a line of text ---------------- */

int dlg_ask(const char *title, const char *label, char *buf, short len)
{
	static char field[128];
	short f, ok, w, show = len > 46 ? 46 : len;
	if (len > (short)sizeof(field) - 1)
		len = (short)sizeof(field) - 1;
	w = show + 6 > 40 ? show + 6 : 40;
	str_copy(field, buf, sizeof(field));
	d_begin(w, 8);
	d_band(w, title);
	d_text(2, 2, label);
	f = d_field(2, 4, field, show, len, 'X');
	d_button(w - 24, 6, 10, "Cancel", EXIT);
	ok = d_button(w - 12, 6, 10, "OK", EXIT | DEFAULT);
	d_end();
	if (d_do(f) != ok || !str_trim(field)[0])
		return 0;
	str_copy(buf, field, len + 1);
	return 1;
}

/* ---------------- Find ---------------- */

int dlg_find(char *text, int size, int *owners)
{
	static char field[100];
	short f, ok;
	str_copy(field, text, sizeof(field));
	nticks = 0;
	s_on[T_REPOS] = !*owners;
	s_on[T_OWNERS] = *owners;
	d_begin(48, 10);
	d_band(48, " Find on GitHub");
	rows_at(2);
	d_add(G_STRING, 0, 0, (long)"Look for", 2, ROW(0), 8, 1);
	f = d_field(11, ROW(0), field, 34, (short)(size < (int)sizeof(field) ? size - 1 : (int)sizeof(field) - 1), 'X');
	d_tick(2, ROW(1), "Repositories with it in their names", T_REPOS);
	d_tick(2, ROW(2), "Owners: people and organisations", T_OWNERS);
	d_text(5, ROW(3), "(a double click lists their repositories)");
	d_button(24, 8, 10, "Cancel", EXIT);
	ok = d_button(36, 8, 10, "Find", EXIT | DEFAULT);
	d_end();
	open_box();
	for (;;) {
		short r = form_loop(f) & 0x7fff;
		if (tick_hit(r))
			continue;
		close_box(r);
		if (r != ok || !str_trim(field)[0])
			return 0;
		str_copy(text, field, size);
		*owners = s_on[T_OWNERS];
		return 1;
	}
}

/* ---------------- Follow, and Get info ----------------
 * Where a repository's files go: its folder, which of a release's files
 * (empty: the zip if there is one, else all), whether a zip is unpacked,
 * and up to four rules for files that go elsewhere (desk accessories to
 * the boot drive's root) or nowhere ("-": another machine's edition). */

/* as long as the settings are (store.h): the fields scroll */
static char fl_folder[160], fl_files[40], fl_names[NRULE][40], fl_rfolder[NRULE][120];
static char fl_l1[64], fl_l2[64], fl_l3[64];

/* the Atari folder, chosen with the file selector */
static int pick_folder(char *dir, int size, const char *title)
{
	char path[200], name[20] = "", *b;
	short btn = 0;
	if (dir[0] && dir[0] != '-')
		snprintf(path, sizeof(path), "%s%s*.*", dir, dir[strlen(dir) - 1] == '\\' ? "" : "\\");
	else
		snprintf(path, sizeof(path), "%c:\\*.*", 'A' + (int)Dgetdrv());
	if (!fsel_exinput(path, name, &btn, title) || !btn)
		return 0;
	b = strrchr(path, '\\');
	if (b)
		b[1] = 0;
	if (strlen(path) > 3)		/* "C:\NET\" -> "C:\NET"; a root keeps its '\' */
		path[strlen(path) - 1] = 0;
	str_copy(dir, path, size);
	return 1;
}

int dlg_follow(REPO *r, const char *desc, int is_new, int *now)
{
	short f_folder, f_rule[NRULE], b_folder, b_rule[NRULE], ok, k, i, y;
	str_copy(fl_folder, r->folder, sizeof(fl_folder));
	str_copy(fl_files, r->files, sizeof(fl_files));
	for (k = 0; k < NRULE; k++) {
		str_copy(fl_names[k], r->rule[k].names, sizeof(fl_names[k]));
		str_copy(fl_rfolder[k], r->rule[k].folder, sizeof(fl_rfolder[k]));
	}
	snprintf(fl_l1, sizeof(fl_l1), "%.62s", desc && desc[0] ? desc : r->full);
	if (r->latest[0])
		snprintf(fl_l2, sizeof(fl_l2), "Latest release %.20s, published %s", r->latest, r->latest_date);
	else
		snprintf(fl_l2, sizeof(fl_l2), "%.62s", r->note[0] ? r->note : "Its releases aren't known yet");
	if (r->have[0])
		snprintf(fl_l3, sizeof(fl_l3), "Downloaded %.20s on %s", r->have, r->have_date);
	else
		str_copy(fl_l3, "Nothing downloaded yet", sizeof(fl_l3));
	nticks = 0;
	s_on[T_UNZIP] = r->unzip;
	s_on[T_NOW] = *now;
	d_begin(64, 21);
	{
		static char band[90];
		snprintf(band, sizeof(band), " %s %s", is_new ? "Follow" : "", r->full);
		d_band(64, is_new ? band : band + 1);
	}
	rows_at(2);
	d_text(2, ROW(0), fl_l1);
	d_text(2, ROW(1), fl_l2);
	y = 2;
	if (!is_new) {
		d_text(2, ROW(y), fl_l3);
		y++;
	}
	d_add(G_STRING, 0, 0, (long)"Folder", 2, ROW(y), 6, 1);
	f_folder = d_field(10, ROW(y), fl_folder, 40, sizeof(fl_folder) - 1, 'X');
	b_folder = d_button(52, ROW(y), 10, "Choose...", EXIT);
	y++;
	d_add(G_STRING, 0, 0, (long)"Files", 2, ROW(y), 5, 1);
	d_field(10, ROW(y), fl_files, 24, sizeof(fl_files) - 1, 'X');
	d_text(36, ROW(y), "empty: the zip, or all");
	y++;
	d_tick(2, ROW(y), "Unpack a .zip into the folder", T_UNZIP);
	y++;
	d_text(2, ROW(y), "These go elsewhere (\"-\": left out), the first that names them:");
	y++;
	for (k = 0; k < NRULE; k++, y++) {
		d_field(2, ROW(y), fl_names[k], 24, sizeof(fl_names[k]) - 1, 'X');
		d_text(28, ROW(y), "->");
		f_rule[k] = d_field(32, ROW(y), fl_rfolder[k], 22, sizeof(fl_rfolder[k]) - 1, 'X');
		b_rule[k] = d_button(56, ROW(y), 6, "...", EXIT);
	}
	if (is_new)
		d_tick(2, 19, "Download the latest release now", T_NOW);
	d_button(40, 19, 10, "Cancel", EXIT);
	ok = d_button(52, 19, 10, is_new ? "Follow" : "OK", EXIT | DEFAULT);
	d_end();
	open_box();
	for (;;) {
		short r2 = form_loop(f_folder) & 0x7fff;
		if (tick_hit(r2))
			continue;
		if (r2 == b_folder || (r2 >= b_rule[0] && r2 <= b_rule[NRULE - 1])) {
			tree[r2].ob_state &= ~SELECTED;
			for (k = 0; k < NRULE && r2 != b_rule[k]; k++)
				;
			if (k == NRULE)
				pick_folder(fl_folder, sizeof(fl_folder), "Where its files go");
			else
				pick_folder(fl_rfolder[k], sizeof(fl_rfolder[k]), "Where these files go");
			{
				LFIELD *lf = lf_of(tree, k == NRULE ? f_folder : f_rule[k]);
				if (lf)
					lf_end(lf);	/* the folder chosen, shown */
			}
			objc_draw(tree, 0, 8, open_at.x, open_at.y, open_at.w, open_at.h);
			continue;
		}
		close_box(r2);
		if (r2 != ok)
			return 0;
		break;
	}
	str_copy(r->folder, str_trim(fl_folder), sizeof(r->folder));
	str_copy(r->files, str_trim(fl_files), sizeof(r->files));
	r->unzip = s_on[T_UNZIP];
	*now = s_on[T_NOW];
	memset(r->rule, 0, sizeof(r->rule));
	for (k = i = 0; k < NRULE; k++)
		if (str_trim(fl_names[k])[0] && str_trim(fl_rfolder[k])[0]) {
			str_copy(r->rule[i].names, fl_names[k], sizeof(r->rule[i].names));
			str_copy(r->rule[i].folder, fl_rfolder[k], sizeof(r->rule[i].folder));
			i++;
		}
	return 1;
}

/* ---------------- settings ----------------
 * The family's Settings dialog (EMail's, EFTP's): a black title band,
 * the parts in a list on the left, the chosen part's settings on the
 * right. */

static char s_hours[4], s_token[100];

static void part_checks(short x, short y)
{
	rows_at(y);
	d_add(G_STRING, 0, 0, (long)"Look for new releases", x, ROW(0), 21, 1);
	d_tick(x + 2, ROW(1), "Only when I ask (Check, ^R)", T_NEVER);
	d_tick(x + 2, ROW(2), "When EUpdate starts", T_START);
	d_tick(x + 2, ROW(3), "Then every     hours too", T_EVERY);
	d_field(x + 16, ROW(3), s_hours, 3, 3, '9');
	d_tick(x, ROW(4), "Download what is new, not only say so", T_FETCH);
	d_add(G_STRING, 0, 0, (long)"(only while EUpdate runs: let the", x + 3, ROW(5), 33, 1);
	d_add(G_STRING, 0, 0, (long)"desktop start it, to keep to them)", x + 3, ROW(6), 34, 1);
}

static void part_files(short x, short y)
{
	rows_at(y);
	d_tick(x, ROW(0), "Shorten long names to 8+3", T_SHORTEN);
	d_add(G_STRING, 0, 0, (long)"for TOS: EFTP-1.45.zip -> EFTP-1_4.ZIP", x + 3, ROW(1), 38, 1);
	d_add(G_STRING, 0, 0, (long)"(off with MiNT or MagiC and long names)", x + 3, ROW(2), 39, 1);
}

static void part_github(short x, short y)
{
	rows_at(y);
	d_add(G_STRING, 0, 0, (long)"Token", x, ROW(0), 5, 1);
	d_pass(x + 6, ROW(0), s_token, 30, sizeof(s_token) - 1);
	d_add(G_STRING, 0, 0, (long)"Without one GitHub answers 60 times", x, ROW(1), 35, 1);
	d_add(G_STRING, 0, 0, (long)"an hour; with one, 5,000. Make one on", x, ROW(2), 37, 1);
	d_add(G_STRING, 0, 0, (long)"github.com: Settings > Developer", x, ROW(3), 32, 1);
	d_add(G_STRING, 0, 0, (long)"settings > Tokens, with no rights.", x, ROW(4), 34, 1);
	d_add(G_STRING, 0, 0, (long)"It is kept encrypted (EUPDATE.KEY).", x, ROW(5), 35, 1);
}

static void part_look(short x, short y)
{
	rows_at(y);
	d_tick(x, ROW(0), "Use colours", T_COLOUR);
	d_add(G_STRING, 0, 0, (long)"on screens with 16 colours or more:", x + 3, ROW(1), 35, 1);
	d_add(G_STRING, 0, 0, (long)"new in orange, current in teal", x + 3, ROW(2), 30, 1);
	d_tick(x, ROW(3), "Start with the Hebrew keyboard", T_HEB);
}

static void part_advanced(short x, short y)
{
	rows_at(y);
	d_tick(x, ROW(0), "Check certificates on the DSP", T_DSP);
	d_add(G_STRING, 0, 0, (long)"(else the 68030 does it all)", x + 3, ROW(1), 28, 1);
	d_tick(x, ROW(2), "Write a protocol log (EUPDATE.LOG)", T_LOG);
	d_add(G_STRING, 0, 0, (long)"the token never goes into it", x + 3, ROW(3), 28, 1);
}

#define SW 13			/* the list's width */
#define SX (SW + 2)		/* where the settings start */

int dlg_settings(void)
{
	static const char *names[] = { "  Checks", "  Files", "  GitHub", "  Look", "  Advanced" };
	short k, box, from, gap = (short)(gl_hchar / 2), ok, r;
	snprintf(s_hours, sizeof(s_hours), "%d", opt.hours);
	str_copy(s_token, opt.token, sizeof(s_token));
	s_on[T_NEVER] = opt.check == CHECK_NEVER;
	s_on[T_START] = opt.check == CHECK_START;
	s_on[T_EVERY] = opt.check == CHECK_EVERY;
	s_on[T_FETCH] = opt.fetch;
	s_on[T_SHORTEN] = opt.shorten;
	s_on[T_COLOUR] = opt.colour;
	s_on[T_HEB] = opt.hebrew;
	s_on[T_LOG] = opt.log;
	s_on[T_DSP] = opt.dsp;
	d_begin(SX + 42, 16);
	nticks = 0;
	npages = 0;
	adding_page = 0;
	memset(page_of, 0, sizeof(page_of));
	d_band(SX + 42, " Settings");
	/* the list: plain words on white, the chosen one in a black bar, a
	   line between it and the settings */
	d_add(G_BOX, 0, 0, 0x00001171L, SW, 1, (short)(0 | (1 << 8)), 15);
	for (k = 0; k < 5; k++) {
		npages++;
		page_tab[npages] = d_add(G_BOXTEXT, TOUCHEXIT, 0, (long)d_ted(names[k], 0, 0x1180),
					 (short)(0 | (2 << 8)), (short)((2 + k * 2) | ((k * gap) << 8)),
					 (short)((SW - 1) | (6 << 8)), 1);
	}
	box = d_add(G_IBOX, 0, 0, 0, SX - 1, 2, 42, 12);
	for (k = 1; k <= 5; k++) {
		from = nobj;
		adding_page = k;
		d_add(G_STRING, 0, 0, (long)(names[k - 1] + 2), SX, 2, (short)strlen(names[k - 1] + 2), 1);
		d_add(G_BOX, 0, 0, 0x00001171L, SX, (short)(3 | (2 << 8)), 40, (short)(0 | (1 << 8)));
		switch (k) {
		case 1: part_checks(SX, 4); break;
		case 2: part_files(SX, 4); break;
		case 3: part_github(SX, 4); break;
		case 4: part_look(SX, 4); break;
		default: part_advanced(SX, 4);
		}
		page_mark(from);
	}
	adding_page = 0;
	d_button(SX + 18, 14, 10, "Cancel", EXIT);
	ok = d_button(SX + 30, 14, 10, "OK", EXIT | DEFAULT);
	d_end();
	show_page(1, 0);
	open_box();
	objc_offset(tree, box, &page_area.x, &page_area.y);
	page_area.w = tree[box].ob_width;
	page_area.h = tree[box].ob_height;
	for (;;) {
		r = form_loop(first_edit()) & 0x7fff;
		if (tick_hit(r))
			continue;
		for (k = 1; k <= npages && r != page_tab[k]; k++)
			;
		if (k <= npages) {
			if (k != cur_page)
				show_page(k, 1);
			continue;
		}
		break;
	}
	close_box(r);
	if (r != ok)
		return 0;
	opt.check = s_on[T_EVERY] ? CHECK_EVERY : s_on[T_START] ? CHECK_START : CHECK_NEVER;
	opt.hours = (short)atoi(s_hours);
	if (opt.hours < 1)
		opt.hours = 1;
	if (opt.hours > 168)
		opt.hours = 168;
	opt.fetch = s_on[T_FETCH];
	opt.shorten = s_on[T_SHORTEN];
	opt.colour = s_on[T_COLOUR];
	opt.hebrew = s_on[T_HEB];
	opt.log = s_on[T_LOG];
	opt.dsp = s_on[T_DSP];
	str_copy(opt.token, str_trim(s_token), sizeof(opt.token));
	return 1;
}

/* ---------------- the keys (Help) ---------------- */

void dlg_keys(void)
{
	static const char *lines[] = {
		"^F  find on GitHub         ^N  follow owner/name",
		"^R  check for releases     ^U  update (the new ones)",
		"Return, double-click: Get info; in Find, follow",
		"   a repository or list an owner's repositories",
		"Backspace  back to the owners   Tab  the other list",
		"^I  get info: folder, files, where they go",
		"Del unfollow               ^A  select all",
		"Shift+click  one more      Insert  the row's menu",
		"^C  copy the names         (in dialogs ^C ^X ^V)",
		"^,  Settings               F10 Hebrew keyboard",
		"While EUpdate works: Esc or Stop ends it",
		"^W, ^Q  quit               Help  the user guide"
	};
	short i, n = (short)(sizeof(lines) / sizeof(lines[0]));
	d_begin(56, n + 6);
	d_add(G_STRING, 0, 0, (long)"Keys", 2, 1, 4, 1);
	for (i = 0; i < n; i++)
		d_text(3, 3 + i, lines[i]);
	d_button(44, n + 4, 10, "OK", EXIT | DEFAULT);
	d_end();
	d_do(0);
}
