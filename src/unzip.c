/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * unzip.c - the files of a .zip archive (see unzip.h), after PKWARE's
 * APPNOTE: the central directory at the end says where each file is.
 */
#include <string.h>
#include "util.h"
#include "inflate.h"
#include "unzip.h"

static unsigned long crc_tab[256];

unsigned long crc32(const unsigned char *p, long n)
{
	unsigned long c = 0xFFFFFFFFUL;
	if (!crc_tab[1]) {
		unsigned long i, k, v;
		for (i = 0; i < 256; i++) {
			for (v = i, k = 0; k < 8; k++)
				v = v & 1 ? 0xEDB88320UL ^ (v >> 1) : v >> 1;
			crc_tab[i] = v;
		}
	}
	while (n-- > 0)
		c = crc_tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFUL;
}

static unsigned le16(const unsigned char *p)
{
	return p[0] | (p[1] << 8);
}

static unsigned long le32(const unsigned char *p)
{
	return p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static int bad(char *err, int errlen, const char *why)
{
	str_copy(err, why, errlen);
	return -1;
}

int unzip(const unsigned char *z, long n, UNZIP_FILE cb, void *ud, char *err, int errlen)
{
	long e, cd, cdend, i, count, files = 0;
	SBUF out;
	/* the end of the central directory: the last 22 bytes, or before a
	   comment of up to 64 KB */
	if (n < 22)
		return bad(err, errlen, "not a zip archive");
	for (e = n - 22; e >= 0 && e >= n - 22 - 65535; e--)
		if (le32(z + e) == 0x06054b50UL && e + 22 + (long)le16(z + e + 20) == n)
			break;
	if (e < 0 || e < n - 22 - 65535)
		return bad(err, errlen, "not a zip archive");
	count = le16(z + e + 10);
	cd = (long)le32(z + e + 16);
	cdend = cd + (long)le32(z + e + 12);
	if (le32(z + e + 16) == 0xFFFFFFFFUL || cd < 0 || cd > e || cdend > e || cdend < cd)
		return bad(err, errlen, "the zip archive is damaged");
	sb_init(&out);
	for (i = 0; i < count; i++) {
		const unsigned char *h = z + cd, *l, *data;
		unsigned long csize, usize, crc, off;
		unsigned method, flags, nlen, xlen, clen;
		char name[256];
		if (cdend - cd < 46 || le32(h) != 0x02014b50UL)
			goto damaged;
		flags = le16(h + 8);
		method = le16(h + 10);
		crc = le32(h + 16);
		csize = le32(h + 20);
		usize = le32(h + 24);
		nlen = le16(h + 28);
		xlen = le16(h + 30);
		clen = le16(h + 32);
		off = le32(h + 42);
		if ((long)(46 + nlen + xlen + clen) > cdend - cd)
			goto damaged;
		cd += 46 + nlen + xlen + clen;
		if (!nlen || nlen >= sizeof(name))
			goto damaged;
		memcpy(name, h + 46, nlen);
		name[nlen] = 0;
		if (name[nlen - 1] == '/')
			continue;		/* a folder: made with its files */
		if (flags & 1) {
			sb_free(&out);
			return bad(err, errlen, "the zip archive is encrypted");
		}
		if (csize == 0xFFFFFFFFUL || usize == 0xFFFFFFFFUL || off == 0xFFFFFFFFUL) {
			sb_free(&out);
			return bad(err, errlen, "the zip archive is too big (ZIP64)");
		}
		/* the local header, then the data */
		if (off > (unsigned long)e || (unsigned long)e - off < 30)
			goto damaged;
		l = z + off;
		if (le32(l) != 0x04034b50UL)
			goto damaged;
		if ((unsigned long)(30 + le16(l + 26) + le16(l + 28)) > (unsigned long)e - off)
			goto damaged;
		data = l + 30 + le16(l + 26) + le16(l + 28);
		if (csize > (unsigned long)(z + e - data))
			goto damaged;
		sb_reset(&out);
		if (method == 0) {
			if (csize != usize)
				goto damaged;
		} else if (method == 8) {
			if (usize > 0x3FFFFFFFUL || !sb_reserve(&out, (long)usize)) {
				sb_free(&out);
				return bad(err, errlen, "not enough memory to unpack the zip archive");
			}
			if (inflate_raw(data, (long)csize, &out, (long)usize) != 1 || out.len != (long)usize)
				goto damaged;
			data = (const unsigned char *)out.s;
		} else {
			sb_free(&out);
			return bad(err, errlen, "the zip archive is packed in a way EUpdate doesn't know");
		}
		if (crc32(data, (long)usize) != crc)
			goto damaged;
		if (cb(ud, name, data, (long)usize)) {
			sb_free(&out);
			return -1;
		}
		files++;
	}
	sb_free(&out);
	return (int)files;
damaged:
	sb_free(&out);
	return bad(err, errlen, "the zip archive is damaged");
}
