/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * theme.c - EUpdate's colours (see theme.h), from EFTP and EMail.
 */
#include "ui.h"
#include "theme.h"

short pen[NPEN];
int colour;

/* soft shades for 256-colour screens, VDI 0-1000 RGB */
static const short shade[NPEN][3] = {
	{ 133, 145, 165 },	/* P_TEXT: near black, a little blue */
	{ 420, 447, 502 },	/* P_DIM: slate grey */
	{ 780, 800, 830 },	/* P_LINE */
	{ 922, 929, 941 },	/* P_PANEL: light grey */
	{ 200, 255, 333 },	/* P_HEAD: dark slate */
	{   0, 471, 851 },	/* P_SEL: blue */
	{ 745, 471, 216 },	/* P_NEW: AERA's orange (#D9A977) a shade deeper, to read on white */
	{  55, 486, 525 },	/* P_OK: AERA's teal, #0E7C86 */
	{ 863, 149, 149 },	/* P_ERR: red */
};
#define FIRST_PEN 16		/* past the desktop's sixteen */

/* with sixteen colours, the standard TOS ones as the ST's and Falcon's
   palette has them: the darker shades (navy, brown for the orange, dark
   cyan for the teal) rather than the loud full ones. Each takes the pen
   nearest to it in the palette there is: a TT's has light colours where
   these are dark, pale yellow on white for "New release" (Erez, 23 Oct
   2026: colours right in any environment; the hub's rule) */
static const short want16[NPEN][3] = {
	{ 0, 0, 0 }, { 533, 533, 533 }, { 533, 533, 533 }, { 733, 733, 733 }, { 0, 0, 0 },
	{ 0, 0, 667 }, { 667, 667, 0 }, { 0, 667, 667 }, { 1000, 0, 0 }
};

/* the pen nearest to want; dark: one that text can be read in on white
   (or white text on it). Never white, the background */
static short nearest(const short *want, int dark, short pal[16][3])
{
	short k, best = 1;
	long bd = 0x7fffffffL;
	for (k = 1; k < 16; k++) {
		long r = pal[k][0] - want[0], g = pal[k][1] - want[1], b = pal[k][2] - want[2];
		long d = r * r + g * g + b * b;
		if (dark && 299L * pal[k][0] + 587L * pal[k][1] + 114L * pal[k][2] > 600000L)
			continue;
		if (d < bd) {
			bd = d;
			best = k;
		}
	}
	return best;
}

static short saved[NPEN][3];
static int set_rgb;

void theme_init(void)
{
	short i;
	theme_exit();
	colour = opt.colour && planes >= 4;
	for (i = 0; i < NPEN; i++)
		pen[i] = i == P_PANEL ? 0 : 1;	/* black and white */
	if (!colour)
		return;
	if (planes >= 8) {
		for (i = 0; i < NPEN; i++) {
			short rgb[3];
			vq_color(vdi_h, FIRST_PEN + i, saved[i]);
			rgb[0] = shade[i][0];
			rgb[1] = shade[i][1];
			rgb[2] = shade[i][2];
			vs_color(vdi_h, FIRST_PEN + i, rgb);
			pen[i] = FIRST_PEN + i;
		}
		set_rgb = 1;
	} else {
		short pal[16][3];
		for (i = 0; i < 16; i++)
			vq_color(vdi_h, i, pal[i]);
		for (i = 0; i < NPEN; i++)
			pen[i] = nearest(want16[i], i != P_PANEL, pal);
		/* a state's colour never the dim one: an Update button in it
		   would look disabled. Then plain, as the word says it */
		for (i = P_NEW; i <= P_ERR; i++)
			if (pen[i] == pen[P_DIM])
				pen[i] = pen[P_TEXT];
	}
}

void theme_exit(void)
{
	short i;
	if (!set_rgb)
		return;
	for (i = 0; i < NPEN; i++)
		vs_color(vdi_h, FIRST_PEN + i, saved[i]);
	set_rgb = 0;
}
