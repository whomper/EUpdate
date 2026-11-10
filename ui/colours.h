/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * colours.h - EUpdate's meaning colours, after the family's six (common's
 * theme.h, D42): a newer release in AERA's orange, a current one in teal,
 * a failure in red.
 */
#ifndef COLOURS_H
#define COLOURS_H

#include "theme.h"

enum {
	P_NEW = P_BASE,	/* a newer release: AERA's orange, the one thing that matters */
	P_OK,		/* current: AERA's teal */
	P_ERR,		/* failed */
	NPEN
};

/* the family's palette with EUpdate's three, from opt.colour; again after
   Settings changes it or the palette does */
void colours_init(void);

#endif
