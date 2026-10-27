/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * main.c - EUpdate: start-up, the menu bar, the event loop, the jobs
 * (finding on GitHub, following, checking, updating), the schedule, and
 * the commands behind menus, keys and the toolbar. Laid out as EFTP's.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "theme.h"
#include "conn.h"
#include "secret.h"
#include "../src/plat.h"
#include "../src/http.h"
#include "../src/update.h"
#include "util.h"
#include "tls.h"
#include "crash.h"
#include "help.h"
#include "av.h"
#include "clip.h"
#include "dd.h"
#include "aboutbox.h"
#include "mbar.h"
#include "dialog.h"
#include "notify.h"

extern const char app_version[];

short vdi_h, cw, ch, scr_w, scr_h, desk_x, desk_y, desk_w, desk_h, planes;
short gl_wchar, gl_hchar;
short ap_id;
short hebrew_kbd;
static short quit;

/* ---------------- menu ---------------- */

/* the menu bar, built by the AERA programs' shared builder
   (common/ui/mbar.c): titles, items, shortcuts, commands */
static const MBAR_ITEM m_desk[] = {
	{ "About EUpdate...", 0, C_ABOUT },
	{ 0, 0, 0 }
};
/* File: the program; what is done to repositories, Follow (its New) and
   Find among it, is in Repository (the guidelines, 3.1) */
static const MBAR_ITEM m_file[] = {
	{ "Quit", "^Q", C_QUIT },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_edit[] = {
	{ "Copy", "^C", C_COPY },
	{ "-", 0, 0 },
	{ "Select all", "^A", C_SELALL },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_repo[] = {
	{ "Find on GitHub...", "^F", C_FIND },
	{ "Follow...", "^N", C_FOLLOW },
	{ "-", 0, 0 },
	{ "Check for releases", "^R", C_CHECK },
	{ "Update", "^D", C_UPDATE },
	{ "Update all that are new", "\x01^D", C_UPDNEW },
	{ "Download again", 0, C_AGAIN },
	{ "-", 0, 0 },
	{ "Get info...", "^I", C_INFO },
	{ "Unfollow", "Del", C_UNFOLLOW },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_opts[] = {
	{ "Settings...", "^,", C_SETTINGS },
	{ "Font...", 0, C_FONT },
	{ "-", 0, 0 },
	{ "Hebrew keyboard", "F10", C_HEBREW },
	{ "Protocol log", 0, C_PLOG },
	{ 0, 0, 0 }
};
static const MBAR_ITEM m_help[] = {
	{ "EUpdate help", "Help", C_HELP },
	{ "Keys...", 0, C_KEYS },
	{ 0, 0, 0 }
};

static const MBAR_TITLE bar[] = {
	{ " EUpdate ", m_desk },
	{ " File ", m_file },
	{ " Edit ", m_edit },
	{ " Repository ", m_repo },
	{ " Options ", m_opts },
	{ " Help ", m_help },
	{ 0, 0 }
};

static MBAR mb;

static void enable(short cmd, int on)
{
	short i = mbar_object(&mb, cmd);
	if (i >= 0)
		menu_ienable(mb.tree, i, on ? 1 : 0);
}

/* the menus as they are: the AES is asked only when one changes (13
   calls after every key took 10 ms on a stock Falcon) */
void menu_update(void)
{
	static const short cmds[] = { C_FIND, C_FOLLOW, C_COPY, C_SELALL, C_CHECK, C_UPDATE, C_UPDNEW,
				      C_AGAIN, C_INFO, C_UNFOLLOW, C_SETTINGS };
	static long shown = -1;
	long sel = repos_selected(), now;
	int idle = !running, news = found_new_count() > 0, i;
	short on[13];
	on[0] = idle;
	on[1] = idle;
	on[2] = win_focused() == &w_repos ? sel > 0 : w_found.total > 0;
	on[3] = nrepos > 0;
	on[4] = idle && nrepos > 0;
	on[5] = idle && (sel > 0 || news);
	on[6] = idle && news;
	on[7] = idle && sel > 0;
	on[8] = idle && sel == 1;
	on[9] = idle && sel > 0;
	on[10] = idle;
	on[11] = hebrew_kbd != 0;
	on[12] = opt.log != 0;
	for (now = 0, i = 0; i < 13; i++)
		now |= (long)on[i] << i;
	for (i = 0; i < 11; i++)
		if (shown < 0 || ((shown ^ now) >> i & 1))
			enable(cmds[i], on[i]);
	if (shown < 0 || ((shown ^ now) >> 11 & 1))
		menu_icheck(mb.tree, mbar_object(&mb, C_HEBREW), on[11]);
	if (shown < 0 || ((shown ^ now) >> 12 & 1))
		menu_icheck(mb.tree, mbar_object(&mb, C_PLOG), on[12]);
	shown = now;
}

/* ---------------- status and helpers ---------------- */

void status(const char *msg)
{
	char tmp[120];
	crash_note(msg);		/* for CRASH.LOG, should EUpdate crash */
	snprintf(tmp, sizeof(tmp), " %s", msg);
	main_status(*msg ? tmp : "");
}

void size_str(long n, char *out, int size)
{
	if (n < 1024)
		snprintf(out, size, "%ld bytes", n);
	else if (n < 1024L * 1024)
		snprintf(out, size, "%ld KB", (n + 512) / 1024);
	else
		snprintf(out, size, "%ld.%ld MB", n / (1024L * 1024), n % (1024L * 1024) * 10 / (1024L * 1024));
}

static void set_logging(void)
{
	conn_log_flush();
	if (opt.log)
		path_join(conn_logfile, sizeof(conn_logfile), opt.workdir, "EUPDATE.LOG");
	else
		conn_logfile[0] = 0;
}

/* Israeli SI-1452 layout by key position (scancode 0x10-0x35), giving
 * Atari character codes; 0 = key not remapped. From Claude ST, as in
 * EMail and EFTP: a search can be typed in Hebrew too. */
static const u8 hebrew_keys[0x36 - 0x10] = {
	/* 10 q..p */ '/', '\'', 0xD4, 0xD5, 0xC2, 0xCA, 0xC7, 0xD8, 0xDA, 0xD2,
	/* 1a [ ] ret ctrl */ 0, 0, 0, 0,
	/* 1e a..' */ 0xD6, 0xC5, 0xC4, 0xCC, 0xD1, 0xCB, 0xC9, 0xCD, 0xD9, 0xDB, ',',
	/* 29 ` lshift \ */ 0, 0, 0,
	/* 2c z../ */ 0xC8, 0xD0, 0xC3, 0xC6, 0xCF, 0xCE, 0xD3, 0xD7, 0xDC, '.'
};

unsigned char key_char(short kstate, short key)
{
	short scan = KEY_SCAN(key);
	unsigned char ascii = KEY_ASCII(key);
	if (hebrew_kbd && !(kstate & (K_LSHIFT | K_RSHIFT | K_ALT)) && !key_ctrl(kstate, key) &&
	    scan >= 0x10 && scan < 0x36 && hebrew_keys[scan - 0x10])
		ascii = hebrew_keys[scan - 0x10];
	return ascii;
}

void set_hebrew_kbd(short on)
{
	hebrew_kbd = on;
	menu_update();
	status(on ? "Hebrew keyboard (F10 switches back)" : "");
}

/* ---------------- while a job runs ----------------
 * A job runs in one go, but the screen stays alive: while the network
 * is waited for (pf_idle), GEM gets a turn every 20 ms: windows are
 * redrawn, and Esc, Undo or the Stop button stop the job. */
static unsigned long gem_at, said_at;
static char job_msg[100];

static void idle(void)
{
	short msg[8], which;
	AEVENT ev;
	if (pf_ms() - gem_at < 20)
		return;
	which = evnt_multi_(MU_MESAG | MU_KEYBD | MU_BUTTON | MU_TIMER, 1, 1, 1, 0, msg, &ev);
	gem_at = pf_ms();
	if ((which & MU_MESAG) && !notify_message(msg)) {
		DROP d;
		switch (msg[0]) {
		case MN_SELECTED:
			menu_tnormal(mb.tree, msg[3], 1);	/* not now */
			break;
		case AP_TERM:
			conn_stop = 1;
			quit = 1;
			break;
		default:
			/* a drop now is refused, not left unanswered */
			if (dd_message(msg, 0, &d) != DD_NONE)
				dd_done(&d);
			else
				win_message(msg);
		}
	}
	if (which & MU_KEYBD) {
		short scan = KEY_SCAN(ev.kreturn);
		if (scan == 0x01 || scan == 0x61)	/* Esc, Undo */
			conn_stop = 1;
	}
	if ((which & MU_BUTTON) && toolbar_stop_hit(ev.mx, ev.my))
		conn_stop = 1;
	win_sliders();
}

/* a step of the job in EUPDATE.LOG, written out at once: should TOS
   go down, the log's last line says where EUpdate was */
static void log_step(const char *msg)
{
	if (!opt.log)
		return;
	conn_log("EUPD", " -- ", msg, strlen(msg));
	conn_log_flush();
}

/* what the job does (update.c), with how far a download has come */
static void job_status(const char *msg)
{
	str_copy(job_msg, msg, sizeof(job_msg));
	aes_profile_tag(job_msg);
	status(msg);
	log_step(msg);
}

static void job_progress(long got, long total)
{
	char m[160], a[16], b[16];
	static long logged;
	if (got < logged)
		logged = 0;		/* the next download */
	if (pf_ms() - said_at < 250 || total < 16384)
		return;
	said_at = pf_ms();
	size_str(got, a, sizeof(a));
	size_str(total, b, sizeof(b));
	snprintf(m, sizeof(m), "%.80s  %s of %s   (Esc stops)", job_msg, a, b);
	status(m);
	if (got - logged >= 256L * 1024) {
		logged = got;
		snprintf(m, sizeof(m), "%s of %s", a, b);
		log_step(m);
	}
}

/* TOS's version, from its header (_sysbase), and whether it is EmuTOS
('ETOS' at $2C); supervisor mode */
static unsigned short tos_ver, emutos;

static void read_tos(void)
{
	const unsigned char *h;
	__asm__("move.l 0x4F2.w,%0" : "=a"(h));
	if (!h)
		return;
	tos_ver = *(const unsigned short *)(h + 2);
	emutos = *(const unsigned long *)(h + 0x2C) == 0x45544F53UL;
}

/* the machine in EUPDATE.LOG at each job: what a crash on it is read with */
static void log_machine(void)
{
	char m[200];
	long st = (long)Mxalloc(-1L, 0), tt = (long)Mxalloc(-1L, 1);
	if (!opt.log)
		return;
	if (!tos_ver)
		Supexec(read_tos);
	snprintf(m, sizeof(m), "EUpdate %s on %s %x.%02x, AES %x.%02x, %s; NVDI %s, _FRB %s, FPU %s; "
		 "free: ST RAM %ld KB, TT RAM %ld KB", app_version, emutos ? "EmuTOS, header" : "TOS",
		 tos_ver >> 8, tos_ver & 0xff,
		 (unsigned short)aes_version() >> 8, aes_version() & 0xff, net_stack(),
		 get_cookie(0x4E564449L) ? "yes" : "no", get_cookie(0x5F465242L) ? "yes" : "no",
		 get_cookie(0x5F465055L) ? "yes" : "no", st > 0 ? st / 1024 : 0, tt > 0 ? tt / 1024 : 0);
	log_step(m);
}

/* a job may start: the network is there, and the certificates TLS
   checks GitHub's against */
static int job_begin(void)
{
	if (running)
		return 0;
	if (!strcmp(net_stack(), "none")) {
		alert(1, "[3][No network: EUpdate needs STinG|(or MiNTnet) to be loaded.][ OK ]");
		return 0;
	}
	if (pf_size(conn_cacert) < 0) {
		alert(1, "[3][EUpdate needs the root|certificates: put CACERT.PEM and|ROOTS.DAT next to EUPDATE.PRG.][ OK ]");
		return 0;
	}
	/* what the dialog that asked for it covered, drawn again first */
	{
		short msg[8], n = 0;
		AEVENT ev;
		while (n++ < 16 && (evnt_multi_(MU_MESAG | MU_TIMER, 0, 0, 0, 0, msg, &ev) & MU_MESAG))
			win_message(msg);
	}
	running = 1;
	conn_stop = 0;
	graf_mouse(BUSYBEE, 0);
	toolbar_redraw();
	menu_update();
	icon_window(0);
	conn_log_hold(1);
	log_machine();
	return 1;
}

/* an ST edition replaced by its app's one program (update.c): logged,
   and an accessory's said at the job's end, as it is loaded until the
   Atari restarts */
#define NREPLACED 4
static char replaced[NREPLACED][160];
static short nreplaced;

/* EUpdate's window drawn again while a dialog window is open over it */
static void dialog_redraw(const short *msg)
{
	short m[8];
	memcpy(m, msg, sizeof(m));
	win_message(m);
}

static void on_replaced(const REPO *r, const char *old, const char *now)
{
	const char *app = strchr(r->full, '/');
	app = app ? app + 1 : r->full;	/* the log has it already (update.c's say) */
	if (nreplaced < NREPLACED && wild_match("*.ACC", now))
		snprintf(replaced[nreplaced++], sizeof(replaced[0]),
			 "[1][%s's accessory is now %s (%s is gone). Restart the Atari to load it; "
			 "the Desk menu still says %s.][ OK ]", app, now, old, app);
}

static void job_end(void)
{
	short msg[8], n = 0;
	AEVENT ev;
	conn_log_hold(0);
	running = 0;
	graf_mouse(ARROW, 0);
	/* the keys typed meanwhile (an Esc held) answer nothing after it */
	while (n++ < 32 && (evnt_multi_(MU_KEYBD | MU_TIMER, 1, 1, 1, 0, msg, &ev) & MU_KEYBD))
		;
	toolbar_redraw();
	menu_update();
	icon_window(0);
	for (n = 0; n < nreplaced; n++)
		alert(1, "%s", replaced[n]);
	nreplaced = 0;
}

/* the answer to a failed job: said, unless the user stopped it */
static void job_failed(const char *what, const char *err)
{
	status("");
	if (conn_stop)
		status("Stopped.");
	else
		alert(1, "[1][%s:|%s][ OK ]", what, err);
}

/* ---------------- the jobs ---------------- */

#define MAXFOUND 100		/* GitHub's searches give 30 a page, an owner's list 100 */

void find_on_github(const char *text, int owners)
{
	GHREPO *found = 0;
	GHUSER *who = 0;
	char err[160], title[80];
	long total;
	int n;
	if (owners ? !(who = malloc(sizeof(GHUSER) * 30)) : !(found = malloc(sizeof(GHREPO) * MAXFOUND))) {
		alert(1, "[3][Not enough memory.][ OK ]");
		return;
	}
	if (!job_begin()) {
		free(who);
		free(found);
		return;
	}
	status(owners ? "Looking for owners on GitHub...   (Esc stops)" :
		 "Looking for repositories on GitHub...   (Esc stops)");
	n = owners ? gh_find_owners(text, who, 30, &total, err, sizeof(err)) :
	    gh_find_repos(text, found, MAXFOUND, &total, err, sizeof(err));
	job_end();
	if (n < 0) {
		free(who);
		free(found);
		job_failed("GitHub can't be asked", err);
		return;
	}
	snprintf(title, sizeof(title), owners ? "Owners like \"%.30s\"" : "Repositories named \"%.30s\"", text);
	if (owners)
		found_show_owners(title, who, n, total);
	else
		found_show_repos(title, found, n, total, 0);
	status(n ? (owners ? "Double-click an owner for their repositories." :
		    "Double-click a repository to follow it.") : "GitHub found none.");
}

void owner_repos(const char *login)
{
	GHREPO *list = malloc(sizeof(GHREPO) * MAXFOUND);
	char err[160], title[80];
	int n;
	if (!list) {
		alert(1, "[3][Not enough memory.][ OK ]");
		return;
	}
	if (!job_begin()) {
		free(list);
		return;
	}
	snprintf(title, sizeof(title), "Asking GitHub for %.30s's repositories...   (Esc stops)", login);
	status(title);
	n = gh_owner_repos(login, list, MAXFOUND, err, sizeof(err));
	job_end();
	if (n < 0) {
		free(list);
		job_failed("GitHub can't be asked", err);
		return;
	}
	snprintf(title, sizeof(title), "%.30s's repositories", login);
	found_show_repos(title, list, n, n, 1);
	status(n ? "Double-click a repository to follow it; Backspace: the owners again." :
	       "They have no public repositories.");
}

/* the jobs' list: the selected rows, all of them, or those with a newer
   release; how many */
static int pick(int which, REPO **out)
{
	short i, n = 0;
	for (i = 0; i < nrepos; i++) {
		REPO *r = repos[i];
		if (which == J_ALL || (which == J_SELECTED && r->sel) ||
		    (which == J_NEW && r->latest[0] && !repo_current(r)))
			out[n++] = r;
	}
	return n;
}

/* the summary after a check or an update */
static void summary(const char *done, int n, int news, int got, int failed)
{
	char m[120];
	if (conn_stop)
		snprintf(m, sizeof(m), "Stopped.");
	else if (failed)
		snprintf(m, sizeof(m), "%s %d: %d failed (the row's tip says why).", done, n, failed);
	else if (got)
		snprintf(m, sizeof(m), "%s %d: %d updated.", done, n, got);
	else if (news)
		snprintf(m, sizeof(m), "%s %d: %d with a new release (Update downloads it).", done, n, news);
	else
		snprintf(m, sizeof(m), "%s %d: all current.", done, n);
	status(m);
}

/* why one failed, in EUPDATE.LOG too (the row's tip says it on the
   screen): a report of a failed update can then say why */
static int failed_one(const REPO *r)
{
	char m[200];
	snprintf(m, sizeof(m), "%s failed: %s", r->full, r->note[0] ? r->note : "?");
	log_step(m);
	return 1;
}

/* what the last check found new, for a notification: "ECal v2.29,
   EMail v1.62", and how many */
static char news_text[120];
static int news_n;

static void news_add(const REPO *r, const GHREL *rel)
{
	const char *name = strchr(r->full, '/') + 1;
	long n = (long)strlen(news_text);
	if (news_n++ < 3 && n + strlen(name) + strlen(rel->tag) + 4 < sizeof(news_text))
		snprintf(news_text + n, sizeof(news_text) - n, "%s%s %s", n ? ", " : "", name, rel->tag);
}

/* asks GitHub for each one's latest release; fetch: and downloads what
   is new */
void check_repos(int which, int fetch)
{
	static REPO *list[MAXREPO];
	GHREL rel;
	int n = pick(which, list), i, news = 0, got = 0, failed = 0;
	news_text[0] = 0;
	news_n = 0;
	if (!n || !job_begin())
		return;
	for (i = 0; i < n && !conn_stop; i++) {
		REPO *r = list[i];
		int k = up_check(r, &rel);
		if (k < 0)
			failed += failed_one(r);
		if (k > 0) {
			news++;
			news_add(r, &rel);
			if (fetch) {
				if (up_install(r, &rel))
					got++;
				else
					failed += failed_one(r);
				idle();		/* a turn between its files and the next one */
			}
		}
		repo_redraw(r);
		idle();
	}
	if (which == J_ALL && !conn_stop)
		opt.last_check = store_now();
	store_save();
	job_end();
	repos_changed();
	summary("Checked", n, news, got, failed);
}

/* downloads the latest release of each one that hasn't it (again: of
   each, whether it has it or not) */
void update_repos(int which, int again)
{
	static REPO *list[MAXREPO];
	GHREL rel;
	int n, i, got = 0, failed = 0;
	if (which == J_SELECTED && !repos_selected())
		which = J_NEW;
	n = pick(which, list);
	if (!n || !job_begin())
		return;
	for (i = 0; i < n && !conn_stop; i++) {
		REPO *r = list[i];
		int k = up_check(r, &rel);
		if (k < 0 || (!k && !rel.tag[0])) {
			if (k < 0)
				failed += failed_one(r);
		} else if (k > 0 || again) {
			if (up_install(r, &rel))
				got++;
			else
				failed += failed_one(r);
			idle();
		}
		store_save();		/* what was written is known at once */
		repo_redraw(r);
		idle();
	}
	job_end();
	repos_changed();
	summary("Updated", n, 0, got, failed);
}

/* a new repository's folder: in the folder chosen last, by its name */
static void default_folder(REPO *r)
{
	const char *name = strchr(r->full, '/') + 1;
	char part[16];
	disk_name(name, part, sizeof(part));
	if (opt.folder[0])
		snprintf(r->folder, sizeof(r->folder), "%s%s%s", opt.folder,
			 opt.folder[strlen(opt.folder) - 1] == '\\' ? "" : "\\", part);
	else
		snprintf(r->folder, sizeof(r->folder), "%c:\\%s", 'A' + up_boot_drive, part);
}

/* the parent of a folder: offered for the next one */
static void keep_parent(const char *folder)
{
	char *s;
	str_copy(opt.folder, folder, sizeof(opt.folder));
	s = strrchr(opt.folder, '\\');
	if (s && s - opt.folder > 2)
		*s = 0;
	else if (s)
		s[1] = 0;
}

void follow(const char *full, const char *desc)
{
	static REPO tmp;
	GHREL rel;
	REPO *r = repo_find(full);
	int now = 1;
	if (r) {
		repo_info(r);
		return;
	}
	if (!gh_valid_full(full)) {
		alert(1, "[1][That isn't a GitHub repository:|owner/name, as whomper/EFTP.][ OK ]");
		return;
	}
	if (nrepos >= MAXREPO) {
		alert(1, "[1][EUpdate follows 64 at most:|unfollow one first.][ OK ]");
		return;
	}
	/* its latest release first: what it has says where its files go */
	memset(&tmp, 0, sizeof(tmp));
	str_copy(tmp.full, full, sizeof(tmp.full));
	tmp.unzip = 1;
	default_folder(&tmp);
	memset(&rel, 0, sizeof(rel));
	if (job_begin()) {
		job_status("Asking GitHub for its latest release...   (Esc stops)");
		if (up_check(&tmp, &rel) >= 0 && rel.tag[0])
			up_suggest(&tmp, &rel, 0);	/* EUpdate runs on a Falcon or TT */
		job_end();
		status("");
		if (conn_stop)
			return;
	}
	if (!dlg_follow(&tmp, desc, 1, &now))
		return;
	if (!tmp.folder[0]) {
		alert(1, "[1][Choose a folder for its files.][ OK ]");
		return;
	}
	r = repo_add(full);
	if (!r)
		return;
	*r = tmp;
	r->sel = 0;
	keep_parent(r->folder);
	store_save();
	repos_changed();
	win_redraw(&w_found, 0);	/* its tick */
	if (now && rel.tag[0] && job_begin()) {
		int ok = up_install(r, &rel);
		store_save();
		job_end();
		repos_changed();
		if (ok)
			status("Downloaded: it is current.");
		else
			job_failed("The download didn't finish", r->note);
	}
}

void repo_info(REPO *r)
{
	static REPO tmp;
	int now = 0;
	tmp = *r;
	if (!dlg_follow(&tmp, 0, 0, &now))
		return;
	str_copy(r->folder, tmp.folder, sizeof(r->folder));
	str_copy(r->files, tmp.files, sizeof(r->files));
	r->unzip = tmp.unzip;
	memcpy(r->rule, tmp.rule, sizeof(r->rule));
	keep_parent(r->folder);
	store_save();
	repo_redraw(r);
}

void unfollow_selected(void)
{
	long n = repos_selected();
	short i;
	if (!n)
		return;
	if (alert(2, "[2][Unfollow %ld repositor%s?|Their files stay where they are.][Unfollow|Cancel]",
		  n, n > 1 ? "ies" : "y") != 1)
		return;
	for (i = nrepos - 1; i >= 0; i--)
		if (repos[i]->sel)
			repo_delete(repos[i]);
	store_save();
	repos_changed();
	win_redraw(&w_found, 0);	/* the ticks */
}

/* ---------------- the schedule ---------------- */

/* what a check by the schedule found, as a notification where BGEM.NG
   runs (the guidelines, 4.5): news nobody asked for just then. Open
   shows the list; the same news is never told twice. Elsewhere
   notify_send gives 0, and the status line says it, as before */
static void tell_news(int fetched)
{
	static char told[sizeof(news_text)];
	char text[160], title[32];
	if (!news_n || !strcmp(told, news_text))
		return;
	str_copy(told, news_text, sizeof(told));
	snprintf(title, sizeof(title), fetched ? "Updated" : news_n > 1 ? "New releases" : "A new release");
	if (news_n > 3)
		snprintf(text, sizeof(text), "%s|and %d more", news_text, news_n - 3);
	else
		str_copy(text, news_text, sizeof(text));
	notify_send(title, text, "Open|Later", 1);
}

/* a notification's button: Open (1) brings the window, the list in it */
static void on_notify(short id, short button)
{
	if (id != 1 || button != 1)
		return;
	if (main_iconified) {
		short m[8] = { WM_UNICONIFY, 0, 0, 0, 0, 0, 0, 0 };
		m[3] = main_h;
		m[4] = opt.main_x;
		m[5] = opt.main_y;
		m[6] = opt.main_w;
		m[7] = opt.main_h;
		win_message(m);
	}
	main_open();
	win_focus(&w_repos);
	menu_update();
}

/* a check is due: at the start, then every opt.hours while EUpdate runs
   (CHECK_EVERY). A clock set back counts as due */
static void schedule(void)
{
	long since = store_now() - opt.last_check;
	if (running || !nrepos || opt.check != CHECK_EVERY)
		return;
	if (since < 0 || since >= (long)opt.hours * 60) {
		check_repos(J_ALL, opt.fetch);
		tell_news(opt.fetch);
	}
}

/* ---------------- commands ---------------- */

static void do_quit(void)
{
	if (!running)
		quit = 1;
}

void main_closed(void);
void main_closed(void)
{
	do_quit();
}

static void help_key_pressed(void)
{
	if (help_key(opt.workdir, "EUPDATE.HYP", 0, "Keys"))
		dlg_keys();
}

/* "owner/name", "github.com/owner/name/...", "https://github.com/owner/name":
   the repository's owner/name in out; 0 if it isn't one */
static int repo_of(const char *s, char *out, int size)
{
	const char *g;
	char tmp[100];
	long n;
	while (*s == ' ')
		s++;
	g = str_istr(s, "github.com/");
	if (g)
		s = g + 11;
	n = (long)strcspn(s, " \t\r\n?#");
	str_copy(tmp, s, n + 1 < (long)sizeof(tmp) ? n + 1 : (long)sizeof(tmp));
	/* only the first two parts of a longer address */
	g = strchr(tmp, '/');
	if (g && (g = strchr(g + 1, '/')) != 0)
		tmp[g - tmp] = 0;
	n = (long)strlen(tmp);
	if (n > 4 && !strcasecmp(tmp + n - 4, ".git"))
		tmp[n - 4] = 0;
	if (!gh_valid_full(tmp))
		return 0;
	str_copy(out, tmp, size);
	return 1;
}

static void command(short cmd)
{
	if (running)
		return;
	switch (cmd) {
	case C_ABOUT: dlg_about(); break;
	case C_FIND: {
		static char text[100];
		static int owners;
		if (dlg_find(text, sizeof(text), &owners))
			find_on_github(text, owners);
		break;
	}
	case C_FOLLOW: {
		char buf[128] = "", full[90];
		if (!dlg_ask(" Follow", "A repository: owner/name or its address", buf, sizeof(buf) - 1))
			break;
		if (repo_of(buf, full, sizeof(full)))
			follow(full, "");
		else
			alert(1, "[1][That isn't a GitHub repository:|owner/name, as whomper/EFTP.][ OK ]");
		break;
	}
	case C_QUIT: do_quit(); break;
	case C_COPY:
		if (win_focused() == &w_repos)
			repos_copy();
		else
			found_copy();
		break;
	case C_SELALL:
		repos_select_all();
		win_focus(&w_repos);
		break;
	case C_CHECK: check_repos(repos_selected() ? J_SELECTED : J_ALL, 0); break;
	case C_UPDATE: update_repos(J_SELECTED, 0); break;
	case C_UPDNEW: update_repos(J_NEW, 0); break;
	case C_AGAIN: update_repos(J_SELECTED, 1); break;
	case C_INFO: {
		short i;
		for (i = 0; i < nrepos; i++)
			if (repos[i]->sel) {
				repo_info(repos[i]);
				break;
			}
		break;
	}
	case C_UNFOLLOW: unfollow_selected(); break;
	case C_SETTINGS:
		if (dlg_settings()) {
			str_copy(gh_token, opt.token, sizeof(gh_token));
			set_logging();
			falcon_dsp_init();
			falcon_dsp_ec_init();
			store_save();
			theme_init();
			win_redraw_all();
		}
		break;
	case C_FONT: font_menu(); break;
	case C_HEBREW: set_hebrew_kbd(!hebrew_kbd); break;
	case C_PLOG:
		opt.log = !opt.log;
		set_logging();
		store_save();
		break;
	case C_HELP: help_key_pressed(); break;
	case C_KEYS: dlg_keys(); break;
	}
	menu_update();
}

void ui_command(short cmd)
{
	command(cmd);
}

/* ---------------- events ---------------- */

static int shortcut(short kstate, short key)
{
	short scan = KEY_SCAN(key);
	if (scan == 0x44) {		/* F10 */
		set_hebrew_kbd(!hebrew_kbd);
		return 1;
	}
	if (crash_test(scan))		/* test builds only: F1-F5 crash on purpose */
		return 1;
	if (scan == 0x62) {		/* Help */
		help_key_pressed();
		return 1;
	}
	/* key_ctrl: a ^key typed while EUpdate was busy comes with kstate 0 */
	if (key_ctrl(kstate, key)) {
		short cmd = C_NONE;
		switch (scan) {
		case 0x21: cmd = C_FIND; break;		/* F */
		case 0x31: cmd = C_FOLLOW; break;	/* N */
		case 0x10: cmd = C_QUIT; break;		/* Q */
		/* U closes the window, which in EUpdate, a program of one
		   window, is quitting; W tops the next window, and with one
		   there is none (the guidelines, 3.2) */
		case 0x16: cmd = C_QUIT; break;		/* U */
		case 0x2e: cmd = C_COPY; break;		/* C */
		case 0x1e: cmd = C_SELALL; break;	/* A */
		case 0x13: cmd = C_CHECK; break;	/* R */
		case 0x20: cmd = kstate & (K_LSHIFT | K_RSHIFT) ? C_UPDNEW : C_UPDATE; break;	/* D: download */
		case 0x17: cmd = C_INFO; break;		/* I */
		case 0x33: cmd = C_SETTINGS; break;	/* , */
		}
		if (cmd != C_NONE) {
			command(cmd);
			return 1;
		}
	}
	if (scan == 0x0f) {		/* Tab: the other list */
		win_focus(win_focused() == &w_repos ? &w_found : &w_repos);
		menu_update();
		return 1;
	}
	return 0;
}

static void wait_release(void)
{
	short mx, my, mb, ks;
	for (;;) {
		graf_mkstate(&mx, &my, &mb, &ks);
		if (!(mb & 3))
			return;
		evnt_timer_(10);
	}
}

/* the folder EUPDATE.PRG was started from */
static void work_dir(char *out, int size)
{
	char path[160];
	short drv = (short)Dgetdrv();
	path[0] = 0;
	Dgetpath(path, 0);
	snprintf(out, size, "%c:%s", 'A' + drv, path);
	if (out[strlen(out) - 1] == '\\')
		out[strlen(out) - 1] = 0;
}

/* ---------------- tooltips (EFTP's) ----------------
 * shown when the mouse rests on a toolbar button or a row, gone when it
 * moves, a key is pressed or a button clicked */
static short tip_on, tip_tried, hover_x = -1, hover_y = -1;
static unsigned long hover_at;
static GRECT tip_r;

static void tip_hide(void)
{
	if (!tip_on)
		return;
	tip_on = 0;
	win_redraw_area(&tip_r);
}

static void tip_show(void)
{
	char text[200], *l2;
	short n1, n2, n;
	GRECT r, scr;
	tip_tried = 1;
	if (!win_tip(hover_x, hover_y, text, sizeof(text)))
		return;
	l2 = strchr(text, '|');
	if (l2)
		*l2++ = 0;
	n1 = (short)strlen(text);
	n2 = l2 ? (short)strlen(l2) : 0;
	if (n1 > 64)
		n1 = 64;
	if (n2 > 64)
		n2 = 64;
	n = n1 > n2 ? n1 : n2;
	r.w = (n + 2) * cw;
	r.h = (l2 ? 2 : 1) * ch + 6;
	r.x = hover_x + cw;
	r.y = hover_y + ch + 2;
	/* on the screen: to the left or above the mouse at the edges */
	if (r.x + r.w > desk_x + desk_w)
		r.x = hover_x - r.w - cw / 2;
	if (r.x < desk_x)
		r.x = desk_x;
	if (r.y + r.h > desk_y + desk_h)
		r.y = hover_y - r.h - 2;
	if (r.y < desk_y)
		r.y = desk_y;
	tip_r = r;
	tip_r.w++;
	tip_r.h++;
	scr.x = scr.y = 0;
	scr.w = scr_w;
	scr.h = scr_h;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	clip_on(&scr);
	fill(&r, 0);
	frame(&r, 1);
	text_at(r.x + cw, r.y + 3, text, n1, n1, (line_rtl(text, n1) ? TX_RIGHT : 0));
	if (l2)
		text_pen_at(pen[P_DIM], r.x + cw, r.y + 3 + ch, l2, n2, n2, 0);
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
	tip_on = 1;
}

/* EUpdate crashed and CRASH.LOG is written (common/atari/crash.c): say so
   and leave GEM tidy. Only the AES and VDI: the heap may be what went
   wrong */
static void crash_quit(const char *what)
{
	char a[200];
	graf_mouse(ARROW, 0);
	snprintf(a, sizeof(a), "[3][EUpdate has crashed|(%s).|What happened is in|CRASH.LOG, in EUpdate's folder.][ Quit ]",
		 what);
	form_alert(1, a);
	if (main_h > 0) {
		wind_close(main_h);
		wind_delete(main_h);
	}
	theme_exit();
	menu_bar(mb.tree, 0);
	v_clsvwk(vdi_h);
	appl_exit();
}

/* VA_START (common/ui/av.c) or text dropped on the window: a repository
   to follow ("whomper/EFTP", or its address); empty: the window to the
   front */
static void av_start(const char *s)
{
	char full[90];
	main_open();
	while (*s == ' ' || *s == '"')
		s++;
	if (!*s)
		return;
	if (running)
		return;
	if (repo_of(s, full, sizeof(full)))
		follow(full, "");
	else
		alert(1, "[1][EUpdate takes a GitHub|repository: owner/name, or|its address.][ OK ]");
}

/* a password (the token) that can't be encrypted goes into EUPDATE.INF
   as it is, and the user is told, once (the guidelines, 7.2) */
static void warn_plain(const char *keyfile)
{
	alert(1, "[1][EUpdate can't write its key file, %s, so the token in EUPDATE.INF is kept unencrypted. "
		 "Is the disk locked or full?][ OK ]", keyfile);
}

int main(void)
{
	short work_out[57], msg[8], d;
	char dir[200];
	AEVENT ev;
	unsigned long sched_at;

	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	/* where the AES runs several programs at once, dialogs are windows
	   with EUpdate's name, and EUpdate's window is drawn while one is
	   open (the guidelines, 3.5) */
	dlg_title = "EUpdate";
	dlg_redraw_hook = dialog_redraw;
	/* TLS needs a 68020 or better: built for the 68030, EUpdate runs on a Falcon or TT */
	if (get_cookie(0x5F435055L) < 20) {	/* _CPU */
		form_alert(1, "[3][EUpdate needs a 68020 or|better: a Falcon or a TT.][ Quit ]");
		appl_exit();
		return 1;
	}
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	{
		/* the workstation: NVDI needs memory for it; 0 is none, and is
		   said, never drawn with (the guidelines, 2.1; as EFTP). The
		   alert says how much was free, for a report */
		short aes_h = vdi_h, tries;
		long st = (long)Mxalloc(-1L, 0), tt = (long)Mxalloc(-1L, 1);
		for (tries = 0; tries < 2; tries++) {
			vdi_h = v_opnvwk_(aes_h, work_out);
			if (vdi_h > 0)
				break;
			evnt_timer_(500);
		}
		if (vdi_h <= 0) {
			char a[200];
			snprintf(a, sizeof(a), "[3][EUpdate can't open its screen:|the VDI has no memory for it.|%ld KB were free; the|accessories hold the rest.|Load fewer, or add memory.][ Quit ]",
				 ((st > 0 ? st : 0) + (tt > 0 ? tt : 0)) / 1024);
			form_alert(1, a);
			appl_exit();
			return 1;
		}
	}
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	if (!screen_check("EUpdate", scr_w)) {	/* not in ST low */
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	vst_alignment(vdi_h, 0, 5);
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	graf_mouse(BUSYBEE, 0);

	work_dir(dir, sizeof(dir));
	crash_install(dir, "EUpdate", app_version);
	help_init();			/* memory the help viewer may read */
	av_init("EUPDATE");		/* the desktop's AV server, if there is one */
	notify_init("EUPDATE ");	/* BGEM.NG's notifications, where it runs (4.5) */
	notify_clicked = on_notify;
	notify_ask();			/* sent only once BGEM.NG said it shows them */
	crash_cleanup = crash_quit;
	crash_report = conn_log_append;	/* the report in EUPDATE.LOG too */
	secret_warn = warn_plain;	/* before anything is saved */
	pf_init();			/* one look at the directory a file */
	store_init(dir);
	str_copy(gh_token, opt.token, sizeof(gh_token));
	str_copy(gh_base, opt.api, sizeof(gh_base));
	snprintf(http_user_agent, sizeof(http_user_agent), "EUpdate/%s (Atari)", app_version);
	up_boot_drive = boot_drive();
	theme_init();
	font_apply();
	hebrew_kbd = opt.hebrew;
	set_logging();
	net_init();
	falcon_dsp_init();
	falcon_dsp_ec_init();
	pf_idle = idle;
	pf_save_turn = idle;		/* big files saved in pieces, a turn between */
	up_status = job_status;
	up_replaced = on_replaced;
	http_progress = job_progress;

	lists_init();
	if (mbar_build(&mb, bar, gl_wchar, gl_hchar, scr_w, scr_h)) {
		form_alert(1, "[3][EUpdate: not enough memory.][ Quit ]");
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	menu_bar(mb.tree, 1);
	graf_mouse(ARROW, 0);

	main_open();
	repos_changed();
	if (!strcmp(net_stack(), "none"))
		status("No network: load STinG (or run MiNT) to reach GitHub.");
	else if (!nrepos)
		status("Find on GitHub (^F) the repositories to follow.");

	crash_offer("EUpdate", "in EUpdate's folder");
	if (store_lost_token)
		alert(1, "[1][EUPDATE.KEY is missing or new: type the GitHub token again in Settings.][ OK ]");
	/* the check at the start, after the window is there */
	if (opt.check != CHECK_NEVER && nrepos && strcmp(net_stack(), "none"))
		check_repos(J_ALL, opt.fetch);
	{
		/* opened with a repository's address: the basepage's line, or
		   MiNT's and MagiC's ARGV (the guidelines, 4.3) */
		const char *cmd = cmd_line();
		if (*cmd)
			av_start(cmd);
	}
	sched_at = pf_ms();
	evnt_set_m1(1, 0, 0, 1, 1);

	while (!quit) {
		short which, hovering;
		unsigned long wait = 1000;
		evnt_set_m1(1, hover_x, hover_y, 1, 1);
		hovering = !tip_tried && hover_x >= 0 && !main_iconified;
		if (hovering)
			wait = 100;
		if (win_sliders_owed())
			wait = 0;		/* a scroll's slider, in a stretch of its own */
		/* 0x102/3/0: wake on any button press, left or right, the
		   clicks counted up to two */
		which = evnt_multi_(MU_MESAG | MU_KEYBD | MU_BUTTON | MU_TIMER | MU_M1, 0x102, 3, 0,
				    wait, msg, &ev);
		/* what the next stretch is: EUPDATE.LOG's slowest at quit, the
		   speed rig's AESGAP lines (tests/hatari/speed.py) */
		if (!(which & (MU_KEYBD | MU_BUTTON | MU_MESAG)))
			win_sliders();
		aes_profile_tag(which & MU_KEYBD ? "key" : which & MU_BUTTON ? "click" :
				!(which & MU_MESAG) ? "timer" : msg[0] == WM_REDRAW ? "redraw" :
				msg[0] == WM_ARROWED ? "arrowed" : msg[0] == WM_VSLID ? "vslid" :
				msg[0] == MN_SELECTED ? "menu" : "message");
		if (which & (MU_M1 | MU_KEYBD | MU_BUTTON | MU_MESAG)) {
			short k;
			if ((which & MU_M1) && !main_iconified) {
				graf_mkstate(&hover_x, &hover_y, &k, &k);
				win_hover(hover_x, hover_y);
			}
			hover_at = pf_ms();
			tip_tried = (which & (MU_KEYBD | MU_BUTTON)) != 0;
			tip_hide();
		} else if (hovering && pf_ms() - hover_at >= 700UL) {
			tip_show();
		}
		if ((which & MU_MESAG) && !notify_message(msg)) {	/* BGEM.NG's: its own */
			const char *va;
			switch (msg[0]) {
			case MN_SELECTED:
				command(mbar_command(&mb, msg[4]));
				menu_tnormal(mb.tree, msg[3], 1);
				break;
			case AP_TERM:
				quit = 1;
				break;
			case COLORS_CHANGED:	/* the palette: each colour's pen again */
				theme_init();
				win_redraw_all();
				break;
			default: {
				DROP dr;
				/* text dropped: a repository's address, as VA_START's */
				switch (dd_message(msg, DD_WANT_TEXT, &dr)) {
				case DD_TEXT: av_start(dr.data); break;
				case DD_NONE:
					if (av_message(msg, &va) == AV_START)
						av_start(va);
					else
						win_message(msg);
				}
				dd_done(&dr);
			}
			}
		}
		if (main_iconified)	/* an icon takes no keys or clicks */
			which &= ~(MU_KEYBD | MU_BUTTON);
		if (which & MU_KEYBD) {
			WIN *top = win_focused();
			if (!shortcut(ev.kstate, ev.kreturn) && top && top->key)
				top->key(top, ev.kstate, ev.kreturn);
			menu_update();
		}
		if (which & MU_BUTTON) {
			short h = wind_find(ev.mx, ev.my), t, dd;
			wind_get(0, WF_TOP, &t, &dd, &dd, &dd);
			if (h > 0 && h != t)	/* a click in a window behind: bring it up too */
				wind_set(h, WF_TOP, 0, 0, 0, 0);
			if (win_mouse(ev.mx, ev.my, ev.mbutton, ev.breturn >= 2 ? 2 : 1, ev.kstate))
				menu_update();
			wait_release();
		}
		/* the schedule, once a minute; no disk while idle (6.5) */
		if (pf_ms() - sched_at >= 60000UL) {
			sched_at = pf_ms();
			schedule();
		}
	}

	graf_mouse(BUSYBEE, 0);
	http_close_all();
	{
		/* where this machine kept the user waiting longest (the
		   guidelines' Speed): in the log, for Erez's reports */
		const char *slow = aes_slowest();
		char m[200];
		if (*slow) {
			snprintf(m, sizeof(m), "the longest waits: %s", slow);
			log_step(m);
		}
	}
	conn_log_flush();
	main_close();
	store_save();
	theme_exit();			/* the palette back as it was */
	menu_bar(mb.tree, 0);
	av_exit();
	v_clsvwk(vdi_h);
	crash_remove();
	appl_exit();
	return 0;
}
