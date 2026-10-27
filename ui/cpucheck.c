/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * cpucheck.c - EUpdate's first code, built for the 68000 (the Makefile's
 * -m68000 for this file alone): on an ST it says that EUpdate needs a
 * Falcon or a TT, and goes. Everything else is built for the 68030, the
 * AES's binding in common's gem.c among them (a bitfield instruction in
 * its first lines), so on a 68000 the program died at appl_init with an
 * illegal instruction before its own check could say anything (the hub,
 * 27 Oct 2026, on TOS 1.04). Here nothing but traps: its own Supexec for
 * the cookie jar and its own AES binding for the alert.
 */

int eupdate_main(void);

/* XBIOS 38, Supexec: f in supervisor mode, its d0 back */
static long supexec(long (*f)(void))
{
	register long r __asm__("d0");
	__asm__ volatile("move.l %1,-(%%sp)\n\t"
			 "move.w #38,-(%%sp)\n\t"
			 "trap #14\n\t"
			 "addq.l #6,%%sp"
			 : "=r"(r) : "g"(f) : "d1", "d2", "a0", "a1", "a2", "memory", "cc");
	return r;
}

/* _CPU from the cookie jar (_p_cookies at 0x5A0), 0 where there is none:
   TOS 1.x has no jar, and only a 68000 */
static long cpu_cookie(void)
{
	unsigned long at = 0x5A0;
	long *jar;
	__asm__("" : "+r"(at));		/* an address, not an object GCC knows */
	jar = *(long **)at;
	if (!jar)
		return 0;
	for (; jar[0]; jar += 2)
		if (jar[0] == 0x5F435055L)	/* _CPU */
			return jar[1];
	return 0;
}

/* the AES, its arrays and trap #2, for the three calls the alert needs */
static short control[5], global[15], intin[16], intout[7];
static void *addrin[2], *addrout[1];
static void *aespb[6] = { control, global, intin, intout, addrin, addrout };

static short aes(short op, short nin, short nout, short nain)
{
	control[0] = op;
	control[1] = nin;
	control[2] = nout;
	control[3] = nain;
	control[4] = 0;
	__asm__ volatile("move.l %0,%%d1\n\t"
			 "move.w #200,%%d0\n\t"
			 "trap #2"
			 : : "g"(aespb) : "d0", "d1", "d2", "a0", "a1", "a2", "memory", "cc");
	return intout[0];
}

int main(void)
{
	if (supexec(cpu_cookie) < 20) {
		if (aes(10, 0, 1, 0) >= 0) {	/* appl_init */
			intin[0] = 1;
			addrin[0] = "[3][EUpdate needs a 68020 or|better: a Falcon or a TT.][ Quit ]";
			aes(52, 1, 1, 1);	/* form_alert */
			aes(19, 0, 1, 0);	/* appl_exit */
		}
		return 1;
	}
	return eupdate_main();
}
