/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ui.h - EUpdate's GEM interface: one window, as EFTP's: a toolbar along
 * the top, the repositories followed above (what each has, what GitHub
 * has, current or not), what a search found below. A menu bar, dialogs
 * built in code, right-click menus.
 */
#ifndef UI_H
#define UI_H

#include "tos.h"
#include "gem.h"
#include "../src/store.h"
#include "../src/gh.h"

typedef struct { short x, y, w, h; } GRECT;

typedef struct WIN WIN;
struct WIN {
	GRECT box;			/* its whole area, header and scroll bar included */
	char title[80];
	char info[80];
	GRECT work;
	long top;			/* first visible line */
	long total;			/* lines of content */
	short head_h;			/* fixed pixels above the scrolled lines (column titles) */
	void (*draw)(WIN *w, GRECT *clip);
	void (*click)(WIN *w, short mx, short my, short clicks, short kstate);
	int  (*key)(WIN *w, short kstate, short key);	/* 1 = handled */
	void (*rclick)(WIN *w, short mx, short my);	/* right button: context menu */
	/* a tooltip for the point: 1 and the text in out */
	int  (*tip)(WIN *w, short mx, short my, char *out, int size);
};

/* screen and font */
extern short vdi_h, cw, ch, scr_w, scr_h, desk_x, desk_y, desk_w, desk_h, planes;
extern short gl_wchar, gl_hchar;	/* the AES's system font cell (menus, dialogs) */
extern short ap_id;

/* the window (win.c) */
extern WIN w_repos, w_found;
extern short main_h;
extern short main_iconified;		/* shown as the icon */
extern int running;			/* a job is going: only Stop and Esc act */
void main_open(void);
void main_close(void);
void main_status(const char *s);	/* the window's info line */
void win_info(WIN *w, const char *info);
void win_title(WIN *w, const char *title);
void win_redraw(WIN *w, GRECT *area);	/* area NULL = its lines */
void win_redraw_lines(WIN *w, long first, long n);
void win_redraw_all(void);
void win_batch(int on);			/* on: redraws kept, those touching joined; off: drawn */
void win_redraw_area(GRECT *area);	/* any part of the window, on the screen */
void win_scroll_to(WIN *w, long top);
void win_ensure_visible(WIN *w, long line);
long win_rows(WIN *w);			/* whole lines that fit */
void win_message(short *msg);		/* WM_* handling */
WIN *win_focused(void);			/* the pane with the keyboard */
void win_focus(WIN *w);
int  win_mouse(short mx, short my, short button, short clicks, short kstate);
void win_hover(short mx, short my);
void win_relayout(void);		/* after a font change */
void win_lines_changed(WIN *w);		/* total changed: the scroll bar, all lines */
int  win_tip(short mx, short my, char *out, int size);
void toolbar_redraw(void);
int  toolbar_stop_hit(short mx, short my);
void icon_window(GRECT *area);		/* the iconified window: the icon, "2 new" */

/* drawing (draw.c) */
#define TX_BOLD     1
#define TX_INVERSE  2
#define TX_RIGHT    4	/* align right-to-left text to the right edge */
#define TX_LIGHT    8
#define TX_LTR     16	/* left-to-right paragraph even when it starts in Hebrew */
#define TX_ON_WHITE 32	/* on colour 0: drawn in replace mode, the VDI's fastest */
#define TX_DIM     TX_LIGHT	/* what can't be used: the shared popup's disabled items */
void fill(GRECT *r, short color);
extern short text_pen;		/* the colour of text (theme.h pens); set, draw, set back to 1 */
#define text_pen_at(p, x, y, s, n, cols, fl) \
	do { text_pen = (p); text_at(x, y, s, n, cols, fl); text_pen = 1; } while (0)
void clip_on(GRECT *r);
void clip_off(void);
void *st_ram(long n);		/* a bitmap's room in ST RAM, for the blitter (n <= 512) */
void text_at(short x, short y, const char *s, long n, short cols, short flags);
void hline(short x1, short x2, short y);
void frame(GRECT *r, short color);
void line_col(short x1, short y1, short x2, short y2, short color);
short line_rtl(const char *s, long n);
void heb_font_map(char *s, long n, short where);

/* the two lists (lists.c) */
void lists_init(void);
void repos_changed(void);		/* the list or a row changed: count, draw */
void repo_redraw(REPO *r);
long repos_selected(void);		/* how many rows are selected */
void repos_select_all(void);
void repos_copy(void);			/* ^C: the selected names, a line each */
void found_copy(void);
/* list: malloc'ed, the list's from now on; an_owners: an owner's, found
   from the owners shown */
void found_show_repos(const char *title, GHREPO *list, int n, long total, int an_owners);
void found_show_owners(const char *title, GHUSER *list, int n, long total);	/* list: as above */
int  found_back(void);			/* Backspace: the owners again; 0 if none */
int  found_new_count(void);		/* repositories with a newer release */

/* jobs (main.c) */
void find_on_github(const char *text, int owners);
void owner_repos(const char *login);
void follow(const char *full, const char *desc);	/* the Follow dialog, then perhaps the download */
void check_repos(int which, int fetch);		/* J_* below */
void update_repos(int which, int again);
#define J_ALL      0
#define J_SELECTED 1
#define J_NEW      2	/* those with a newer release */
void repo_info(REPO *r);
void unfollow_selected(void);

/* dialogs (dialogs.c) */
int  dlg_ask(const char *title, const char *label, char *buf, short len);
int  dlg_find(char *text, int size, int *owners);
int  dlg_follow(REPO *r, const char *desc, int is_new, int *now);	/* 1 = OK */
int  dlg_settings(void);
void dlg_about(void);
void dlg_keys(void);
#include "alert.h"		/* alert(): common/ui */

/* menu commands (main.c) */
enum {
	C_NONE, C_ABOUT,
	C_FOLLOW, C_FIND, C_QUIT,
	C_COPY, C_SELALL,
	C_CHECK, C_UPDATE, C_UPDNEW, C_AGAIN, C_INFO, C_UNFOLLOW,
	C_SETTINGS, C_FONT, C_HEBREW, C_PLOG,
	C_HELP, C_KEYS
};
void ui_command(short cmd);
void menu_update(void);

/* right-click menus (common/ui/popup.c): the chosen item, or -1 */
#include "popup.h"

/* font.c */
void font_apply(void);		/* opt.font_id/pt -> cw, ch */
void font_sys_push(void);	/* the system font, for a popup or the About box */
void font_sys_pop(void);	/* back to the window's font */
void font_reselect(void);	/* the text font again, after drawing in another */
#define HM_MAP     0
#define HM_UNICODE 1
#define HM_SYSTEM  2
extern short heb_mode;
extern unsigned short heb_idx[27];
void font_menu(void);

/* main.c */
void status(const char *msg);
void set_hebrew_kbd(short on);
extern short hebrew_kbd;
unsigned char key_char(short kstate, short key);	/* honours the Hebrew layout */
/* "980 bytes", "12 KB", "1.4 MB" */
void size_str(long n, char *out, int size);

#define K_ALT 0x08
#define KEY_SCAN(k)  (((unsigned short)(k)) >> 8)
#define KEY_ASCII(k) ((unsigned char)((k) & 0xff))

/* the Falcon's DSP for TLS (common/ui/dsp.c, dsp_ec.c) */
void falcon_dsp_init(void);
void falcon_dsp_ec_init(void);
const char *falcon_dsp_state(void);	/* "DSP" or "68030" */

#endif
