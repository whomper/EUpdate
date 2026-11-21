/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * HOLD.PRG - a help viewer's stand-in for the rigs: a GEM program started
 * at boot that takes memory as one loading a guide does (several blocks,
 * the program's own, TT RAM preferred), opens a window and stays, so the
 * program tried next starts with memory taken and split (a FireBee
 * tester's HypView, 21 Nov 2026). C:\HOLD.TXT: the blocks' sizes in KB.
 */
#include <string.h>
#include "tos.h"
#include "gem.h"

long nf_on = 1;

int main(void)
{
	static char t[200];
	short msg[8], wh;
	long h, n, kb, got = 0;
	char *p;
	AEVENT ev;
	appl_init();
	h = Fopen("C:\\HOLD.TXT", 0);
	n = h >= 0 ? Fread((short)h, sizeof(t) - 1, t) : 0;
	if (h >= 0)
		Fclose((short)h);
	t[n > 0 ? n : 0] = 0;
	for (p = t; *p;) {
		for (kb = 0; *p >= '0' && *p <= '9'; p++)
			kb = kb * 10 + *p - '0';
		if (kb) {
			char *m = (char *)Mxalloc(kb * 1024, 3);
			if ((long)m == -32)
				m = (char *)Malloc(kb * 1024);
			if ((long)m > 0) {
				memset(m, 0x55, kb * 1024);
				got += kb;
			}
		}
		while (*p && (*p < '0' || *p > '9'))
			p++;
	}
	wh = wind_create(NAME | MOVER | CLOSER, 0, 0, 640, 480);
	if (wh > 0) {
		wind_set_str(wh, WF_NAME, " Guide ");
		wind_open(wh, 300, 250, 300, 180);
	}
	nf_debug(got ? "HOLD holds its memory\n" : "HOLD got no memory\n");
	for (;;) {
		evnt_multi_(MU_MESAG, 0, 0, 0, 0, msg, &ev);
		if (msg[0] == WM_CLOSED || msg[0] == AP_TERM)
			break;
	}
	if (wh > 0) {
		wind_close(wh);
		wind_delete(wh);
	}
	appl_exit();
	return 0;
}
