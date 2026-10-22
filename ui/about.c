/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * about.c - EUpdate's About box: the AERA programs' shared one
 * (common/ui/aboutbox.c), with the network, what GitHub still allows
 * this hour, and the memory it runs in; and the iconified window.
 */
#include <string.h>
#include <stdio.h>
#include "ui.h"
#include "aboutbox.h"
#include "iconify.h"
#include "icon.h"
#include "icon16.h"
#include "../src/plat.h"
#include "util.h"

extern const char app_version[];

void dlg_about(void)
{
	static char net[80], gh[60];
	ABOUT a = { "EUpdate", app_version, "GitHub releases for the Atari Falcon and TT",
		    { net, gh, about_ram() }, { "Git logo by Jason Long, CC BY 3.0", 0, 0 },
		    icon_bits, icon16_px, icon16_rgb };
	snprintf(net, sizeof(net), "%s  \xF9  TLS on the %s",
		 strcmp(net_stack(), "none") ? net_stack() : "no network", falcon_dsp_state());
	if (gh_rate_left >= 0)
		snprintf(gh, sizeof(gh), "GitHub answers %ld more this hour%s", gh_rate_left,
			 opt.token[0] ? " (token)" : "");
	else
		snprintf(gh, sizeof(gh), "%s", opt.token[0] ? "With a GitHub token" : "Without a GitHub token");
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
