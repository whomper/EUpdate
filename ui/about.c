/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * about.c - EUpdate's About box: the AERA programs' shared one
 * (common/ui/aboutbox.c), with its three status lines in the family's
 * words (aboutline.c; the guidelines, 1.3): the connection, what GitHub
 * still allows this hour, and the memory; and the iconified window.
 */
#include <string.h>
#include <stdio.h>
#include "ui.h"
#include "aboutbox.h"
#include "aboutline.h"
#include "iconify.h"
#include "icon.h"
#include "icon16.h"
#include "icon256.h"
#include "../src/plat.h"
#include "util.h"

extern const char app_version[];

void dlg_about(void)
{
	static char net[80], gh[60], mem[80];
	ABOUT a = { "EUpdate", app_version, "GitHub releases for the Atari Falcon and TT",
		    { net, gh, mem }, { "Git logo by Jason Long, CC BY 3.0", 0, 0 },
		    icon_bits, icon16_px, icon16_rgb, icon256_px, icon256_rgb };
	/* 1: the connection: online while EUpdate talks to GitHub, ready
	   between; with no network neither (the DSP's part is in EUPDATE.LOG) */
	about_net_line(net, sizeof(net), net_stack(), ABOUT_DIRECT,
		       !strcmp(net_stack(), "none") ? 0 : running ? ABOUT_ONLINE : ABOUT_READY);
	/* 2: EUpdate's own: what GitHub still allows this hour */
	if (gh_rate_left >= 0)
		snprintf(gh, sizeof(gh), "GitHub: %ld requests left this hour%s", gh_rate_left,
			 opt.token[0] ? " (token)" : "");
	else
		snprintf(gh, sizeof(gh), "%s", opt.token[0] ? "With a GitHub token" : "Without a GitHub token");
	/* 3: the memory */
	about_mem_line(mem, sizeof(mem));
	about_show(&a);
}

/* The iconified window: the icon, and how many have a newer release
   (common/ui/iconify.c). From a redraw message, area; from EUpdate
   itself (0), only when that changed */
void icon_window(GRECT *area)
{
	static char shown[12] = "-";
	char s[12] = "";
	int n = found_new_count();
	if (!main_iconified || main_h <= 0)
		return;
	if (running)
		str_copy(s, "busy", sizeof(s));
	else if (n)
		snprintf(s, sizeof(s), "%d new", n);
	if (!area && !strcmp(s, shown))
		return;
	strcpy(shown, s);
	icon_redraw(main_h, area, icon_bits, icon16_px, s);
}
