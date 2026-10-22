/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat_tos.c - files, folders and time on GEMDOS (EFTP's, cut to what
 * EUpdate uses). TCP is common/atari/net_tos.c.
 */
#include <string.h>
#include "tos.h"
#include "../src/plat.h"
#include "bigbuf.h"
#include "tls.h"

void (*pf_idle)(void);

int pf_open(const char *path, int mode)
{
	long h;
	if (mode == PF_WRITE)
		return (int)Fcreate(path, 0);
	h = Fopen(path, mode == PF_APPEND ? 2 : 0);
	if (h < 0 && mode == PF_APPEND)
		h = Fcreate(path, 0);
	else if (h >= 0 && mode == PF_APPEND)
		Fseek(0, (short)h, 2);
	return (int)h;
}

long pf_read(int h, void *buf, long n) { return Fread((short)h, n, buf); }

/* TT RAM to the disk: straight when the driver bounces DMA itself (the
   _FRB cookie: HDDRIVER, TOS 3.06 and 4 with TT RAM), else through a
   piece of ST RAM (the guidelines, 6.5) */
long pf_write(int h, const void *buf, long n)
{
	static char *st;
	const char *p = buf;
	long done = 0;
	if ((unsigned long)buf < 0x01000000UL || big_direct())
		return Fwrite((short)h, n, buf);
	if (!st) {
		st = (char *)Mxalloc(16384, 0);
		if ((long)st <= 0) {
			st = 0;
			return Fwrite((short)h, n, buf);
		}
	}
	while (done < n) {
		long k = n - done > 16384 ? 16384 : n - done, r;
		memcpy(st, p + done, k);
		r = Fwrite((short)h, k, st);
		if (r != k)
			return r < 0 ? r : done + r;
		done += k;
	}
	return done;
}

void pf_close(int h) { Fclose((short)h); }
long pf_seek(int h, long off) { return Fseek(off, (short)h, 0); }

static DTA dta;

long pf_size(const char *path)
{
	void *old = Fgetdta();
	long r;
	Fsetdta(&dta);
	r = Fsfirst(path, 0x07) == 0 ? (long)dta.d_length : -1;
	Fsetdta(old);
	return r;
}

int pf_mkdir(const char *path)
{
	long r = Dcreate(path);
	return (r == 0 || r == -36 || pf_isdir(path)) ? 0 : (int)r;
}

int pf_remove(const char *path) { return (int)Fdelete(path); }
int pf_rename(const char *from, const char *to) { return (int)Frename(from, to); }

int pf_isdir(const char *path)
{
	void *old = Fgetdta();
	long r;
	char p[220];
	size_t n = strlen(path);
	if (n == 3 && path[1] == ':')
		return 1;		/* a drive's root */
	if (n >= sizeof(p))
		return 0;
	strcpy(p, path);
	if (n && p[n - 1] == '\\')
		p[n - 1] = 0;
	Fsetdta(&dta);
	r = Fsfirst(p, 0x17);
	Fsetdta(old);
	return r == 0 && (dta.d_attrib & 0x10);
}

/* an open file's size, read from the start again (pf_load) */
static long size_open(int h)
{
	long n = Fseek(0, (short)h, 2);
	Fseek(0, (short)h, 0);
	return n;
}

/* CACERT.PEM's date and size from one Fsfirst (TLS's ROOTS.DAT check) */
static unsigned long file_info(const char *path, long *size)
{
	void *old = Fgetdta();
	long r;
	Fsetdta(&dta);
	r = Fsfirst(path, 0x07);
	Fsetdta(old);
	if (r) {
		*size = -1;
		return 0;
	}
	*size = (long)dta.d_length;
	return ((unsigned long)dta.d_date << 16) | dta.d_time;
}

void pf_init(void)
{
	pf_size_open = size_open;
	tls_file_info = file_info;
}

void pf_now(PFTIME *t)
{
	unsigned short d = (unsigned short)Tgetdate();
	unsigned short tm = (unsigned short)Tgettime();
	t->year = 1980 + (d >> 9);
	t->mon = (d >> 5) & 15;
	t->day = d & 31;
	t->hour = tm >> 11;
	t->min = (tm >> 5) & 63;
	t->sec = (tm & 31) * 2;
}

/* the clock: TOS's 200 Hz counter, measured against MFP timer C and
   corrected where it runs fast (common/atari/tos.c) */
unsigned long pf_ms(void)
{
	return tos_ms();
}

/* Timer jitter: the MFP's timer C counts every 26 us and the video
 * chip's line counter every 32 us; read in a loop whose length depends
 * on the previous readings, they drift against the CPU clock, the bus
 * and interrupts. Each byte folds in several readings. (EFTP's) */
#pragma GCC diagnostic ignored "-Warray-bounds"	/* fixed hardware addresses */
static unsigned char *ent_buf;
static int ent_n;

static void sup_entropy(void)
{
	volatile unsigned char *tcdr = (volatile unsigned char *)0xfffffa23L;
	volatile unsigned long *hz200 = (volatile unsigned long *)0x4baL;
	unsigned long acc = *hz200;
	int i, k;
	for (i = 0; i < ent_n; i++) {
		for (k = 0; k < 8; k++) {
			volatile int spin, j = (int)(acc & 31) + 8;
			for (spin = 0; spin < j; spin++)
				;
			acc = (acc << 5) ^ (acc >> 27) ^ *tcdr ^ (*hz200 << 3);
		}
		ent_buf[i] ^= (unsigned char)(acc ^ (acc >> 8) ^ (acc >> 16) ^ (acc >> 24));
	}
}

void pf_entropy(unsigned char *buf, int n)
{
	int i;
	unsigned short d = (unsigned short)Tgetdate(), tm = (unsigned short)Tgettime();
	for (i = 0; i < n; i++)
		buf[i] = (unsigned char)(i * 151);
	buf[0] ^= (unsigned char)tm;
	buf[1] ^= (unsigned char)(tm >> 8);
	buf[2] ^= (unsigned char)d;
	buf[3] ^= (unsigned char)(d >> 8);
	ent_buf = buf;
	ent_n = n;
	Supexec(sup_entropy);
	/* XBIOS Random(), and where the stack happens to be */
	for (i = 0; i + 2 < n; i += 3) {
		unsigned long r = (unsigned long)trap14_w(17);
		buf[i] ^= (unsigned char)r;
		buf[i + 1] ^= (unsigned char)(r >> 8);
		buf[i + 2] ^= (unsigned char)(r >> 16);
	}
	buf[n - 1] ^= (unsigned char)((unsigned long)&i >> 3);
}

