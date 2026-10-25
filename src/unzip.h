/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * unzip.h - the files of a .zip archive held in memory (a release's
 * zip, as AERA's releases carry), with common's inflate.c. Stored and
 * deflated files; every size and offset is checked against the archive
 * and every file against its CRC-32. Encrypted files and ZIP64 are
 * refused.
 */
#ifndef UNZIP_H
#define UNZIP_H

/* each file in turn: its name as the archive has it ("icons/EFTP.RSC",
   folders left out) and its bytes. Non-zero stops the unpacking (the
   callback's error) */
typedef int (*UNZIP_FILE)(void *ud, const char *name, const unsigned char *data, long len);

/* the number of files handed over; -1 with err when the archive is
   damaged, or the callback stopped it (err is then left to it) */
int unzip(const unsigned char *zip, long n, UNZIP_FILE cb, void *ud, char *err, int errlen);

unsigned long crc32(const unsigned char *p, long n);

/* b made a buffer of n bytes (and the NUL) from pf_big_alloc, empty; 0
   when there is no room. What goes in must stay within n: it never grows.
   sb_big_free gives it back and empties b */
#include "util.h"
int sb_big(SBUF *b, long n);
void sb_big_free(SBUF *b);

#endif
