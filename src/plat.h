/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat.h - what EUpdate's core needs from the machine: files, folders,
 * the clock, a millisecond timer and TCP. atari/plat_tos.c has it for
 * TOS, tests/host/plat_posix.c for Linux (the tests and eupd-cli). Only
 * what EUpdate and the common/ code it links use.
 */
#ifndef PLAT_H
#define PLAT_H

#include <stddef.h>

#ifdef EU_TOS
#define PF_SEP '\\'
#else
#define PF_SEP '/'
#endif

/* ---- files ---- */
#define PF_READ   0
#define PF_WRITE  1	/* create or truncate */
#define PF_APPEND 2

int  pf_open(const char *path, int mode);	/* handle >= 0, < 0 on error */
long pf_read(int h, void *buf, long n);
long pf_write(int h, const void *buf, long n);
void pf_close(int h);
long pf_seek(int h, long off);			/* from the start; the new place */
long pf_size(const char *path);			/* -1 if missing */
int  pf_mkdir(const char *path);		/* 0 ok or already there */
int  pf_remove(const char *path);
int  pf_rename(const char *from, const char *to);
int  pf_isdir(const char *path);

/* the hooks common's file reading takes (pf_load, TLS's files): one look
   at the directory a file (the guidelines, 6.5) */
void pf_init(void);
extern long (*pf_size_open)(int h);

/* whole-file helpers (common/src/futil.c): pf_load returns a malloc'ed
   NUL-terminated copy; pf_save writes NAME.IN$ and renames it over */
/* a big buffer straight from the system, given back whole when done (a
   release's zip, a file unpacked from it): kept out of the heap, which
   never gives its memory back and grows in ever bigger blocks; 0 when
   there is no room for n bytes in one piece */
void *pf_big_alloc(long n);
void  pf_big_free(void *p);

char *pf_load(const char *path, long *len);
int   pf_save(const char *path, const void *data, long len);
/* set: pf_save writes 32 KB at a time and calls it between (a program's
   turn for GEM; never an accessory's, which may not keep a file open) */
extern void (*pf_save_turn)(void);

/* ---- time ---- */
typedef struct {
	short year, mon, day, hour, min, sec;
} PFTIME;

void pf_now(PFTIME *t);		/* local wall clock */
unsigned long pf_ms(void);	/* monotonic milliseconds (5 ms steps on TOS) */
/* raw unpredictable bytes; tls.c and secret.c hash them */
void pf_entropy(unsigned char *buf, int n);

/* ---- TCP (common/atari/net_tos.c on the Atari) ---- */
int  net_init(void);			/* 1 if a TCP/IP stack is present */
const char *net_stack(void);		/* "STinG", "MiNTnet", "none" ... */
int  net_open(const char *host, unsigned short port, char *err, int errlen);
long net_write(int h, const void *buf, long n);	/* bytes sent or < 0 */
/* non-blocking: > 0 bytes read, 0 nothing yet, < 0 closed or failed */
long net_read(int h, void *buf, long max);
void net_close(int h);
void net_close_wait(int h, int seconds);

/* the UI keeps the screen alive while the network is waited for */
extern void (*pf_idle)(void);

#endif
