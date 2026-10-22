/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.h - EUpdate's colours, EMail's and EFTP's way: a few, for meaning. With 256
 * colours EUpdate sets its own soft shades in palette entries the desktop
 * doesn't use (and puts them back at the end); with 16 it takes the
 * standard ones; on monochrome and 4-colour screens nothing changes.
 */
#ifndef THEME_H
#define THEME_H

enum {
	P_TEXT,		/* ordinary text */
	P_DIM,		/* sizes, dates, column titles, notes */
	P_LINE,		/* rules and frames */
	P_PANEL,	/* light background: dividers, idle title bars, bar tracks */
	P_HEAD,		/* the focused pane's title bar */
	P_SEL,		/* the selection bar */
	P_NEW,		/* a newer release: AERA's orange, the one thing that matters */
	P_OK,		/* current: AERA's teal */
	P_ERR,		/* failed */
	NPEN
};

extern short pen[NPEN];
extern int colour;		/* colours are on (else black and white) */

void theme_init(void);		/* after the VDI is open and opt is read */
void theme_exit(void);		/* the palette back as it was */

#endif
