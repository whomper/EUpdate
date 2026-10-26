/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * unzip.c - the files of a .zip archive (see unzip.h), after PKWARE's
 * APPNOTE: the central directory at the end says where each file is.
 * One reader for an archive in memory and one in a file: it asks for the
 * bytes at an offset, and hands each file on in pieces, unpacked through
 * common's streaming inflate, so neither an archive nor a file in it is
 * ever held whole (a stock Falcon with 4 MB had 510 KB free
 * for EUpdate's work, and EMail's zip alone is 460 KB).
 */
#include <string.h>
#include "util.h"
#include "inflate.h"
#include "unzip.h"
#include "plat.h"

int sb_big(SBUF *b, long n)
{
	sb_init(b);
	b->s = pf_big_alloc(n + 1);
	if (!b->s)
		return 0;
	b->s[0] = 0;
	b->cap = n + 1;
	return 1;
}

void sb_big_free(SBUF *b)
{
	pf_big_free(b->s);
	sb_init(b);
}

static unsigned long crc_tab[256];

unsigned long crc32_more(unsigned long crc, const unsigned char *p, long n)
{
	unsigned long c = crc ^ 0xFFFFFFFFUL;
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

unsigned long crc32(const unsigned char *p, long n)
{
	return crc32_more(0, p, n);
}

static unsigned le16(const unsigned char *p)
{
	return p[0] | (p[1] << 8);
}

static unsigned long le32(const unsigned char *p)
{
	return p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

/* where the archive's bytes come from: n bytes at off into buf, 0 when
   they can't all be had */
typedef struct {
	const unsigned char *mem;	/* in memory, or */
	int h;				/* a file's handle */
	long size;
} SRC;

static int get(SRC *s, long off, void *buf, long n)
{
	if (off < 0 || n < 0 || off > s->size || n > s->size - off)
		return 0;
	if (s->mem) {
		memcpy(buf, s->mem + off, n);
		return 1;
	}
	return pf_seek(s->h, off) == off && pf_read(s->h, buf, n) == n;
}

#define PIECE 32768L		/* what a file is handed on in */
#define STEP  2048L		/* what is unpacked between two turns */

/* a deflated file's packed bytes for the inflate, no more than it has */
typedef struct {
	SRC *s;
	long at, left;
} FEED;

static int feed(void *u, unsigned char *buf, long n)
{
	FEED *f = u;
	if (n > f->left)
		n = f->left;
	if (n <= 0)
		return 0;
	if (!get(f->s, f->at, buf, n))
		return -1;
	f->at += n;
	f->left -= n;
	return (int)n;
}

/* one file's bytes: stored ones as they are, deflated ones unpacked;
   each piece to the sink, the CRC on the way. 1 when whole and right,
   0 damaged, -1 out of memory, -2 the sink stopped */
static int member(SRC *s, long data, unsigned long csize, unsigned long usize, unsigned method,
		  unsigned long crc, const UNZIP_TO *to, void *ud, unsigned char *piece)
{
	unsigned long c = 0;
	if (method == 0) {
		unsigned long done = 0;
		if (csize != usize)
			return 0;
		while (done < usize) {
			long k = usize - done > (unsigned long)PIECE ? PIECE : (long)(usize - done);
			const unsigned char *p = s->mem ? s->mem + data + done : piece;
			if (!s->mem && !get(s, data + (long)done, piece, k))
				return 0;
			c = crc32_more(c, p, k);
			if (to->put(ud, p, k))
				return -2;
			done += k;
		}
	} else {
		/* deflated: through common's streaming inflate (a 32 KB window),
		   a piece at a time, never the whole file */
		FEED f;
		INFLATE *z;
		unsigned long total = 0;
		long k;
		int r = 1;
		f.s = s;
		f.at = data;
		f.left = (long)csize;
		z = inflate_open(feed, &f);
		if (!z)
			return -1;
		do {
			/* a piece filled STEP at a time, with a turn for GEM between:
			   a stock Falcon unpacks 32 KB in about a second */
			long have = 0;
			while (have < PIECE && (k = inflate_read(z, piece + have, STEP)) > 0) {
				have += k;
				if (pf_idle)
					pf_idle();
			}
			if (k < 0)
				r = 0;
			total += have;
			if (r == 1 && total > usize)
				r = 0;
			if (r == 1 && have) {
				c = crc32_more(c, piece, have);
				if (to->put(ud, piece, have))
					r = -2;
			}
		} while (r == 1 && k > 0);
		inflate_close(z);
		if (r == 1 && total != usize)
			r = 0;
		if (r != 1)
			return r;
	}
	return c == crc;
}

static int bad(char *err, int errlen, const char *why)
{
	str_copy(err, why, errlen);
	return -1;
}

static int unzip_src(SRC *s, const UNZIP_TO *to, void *ud, char *err, int errlen)
{
	unsigned char *tail, *cdir = 0, *piece = 0;
	long n = s->size, t, base, e, cd, cdlen, i, count, files = 0, at;
	int r = -1;
	/* the end of the central directory: the last 22 bytes, or before a
	   comment of up to 64 KB */
	if (n < 22)
		return bad(err, errlen, "not a zip archive");
	t = n < 22 + 65535 ? n : 22 + 65535;
	base = n - t;
	tail = s->mem ? (unsigned char *)s->mem + base : pf_big_alloc(t);
	if (!tail)
		return bad(err, errlen, "not enough memory to unpack the zip archive");
	if (!s->mem && !get(s, base, tail, t)) {
		pf_big_free(tail);
		return bad(err, errlen, "the zip archive can't be read");
	}
	for (e = t - 22; e >= 0; e--)
		if (le32(tail + e) == 0x06054b50UL && base + e + 22 + (long)le16(tail + e + 20) == n)
			break;
	if (e < 0) {
		if (!s->mem)
			pf_big_free(tail);
		return bad(err, errlen, "not a zip archive");
	}
	count = le16(tail + e + 10);
	cd = (long)le32(tail + e + 16);
	cdlen = (long)le32(tail + e + 12);
	if (le32(tail + e + 16) == 0xFFFFFFFFUL || le32(tail + e + 12) == 0xFFFFFFFFUL || cd < 0 || cdlen < 0)
		cd = -1;
	e += base;			/* the end record, in the archive */
	if (!s->mem)
		pf_big_free(tail);
	if (cd < 0 || cd > e || cdlen > e - cd)
		return bad(err, errlen, "the zip archive is damaged");
	cdir = s->mem ? (unsigned char *)s->mem + cd : pf_big_alloc(cdlen + 1);
	piece = pf_big_alloc(PIECE);
	if (!cdir || !piece) {
		bad(err, errlen, "not enough memory to unpack the zip archive");
		goto done;
	}
	if (!s->mem && !get(s, cd, cdir, cdlen))
		goto damaged;
	for (i = 0, at = 0; i < count; i++) {
		const unsigned char *h = cdir + at;
		unsigned char l[30];
		unsigned long csize, usize, crc, off;
		unsigned method, flags, nlen, xlen, clen;
		long data;
		char name[256];
		int m;
		if (cdlen - at < 46 || le32(h) != 0x02014b50UL)
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
		if ((long)(46 + nlen + xlen + clen) > cdlen - at)
			goto damaged;
		at += 46 + nlen + xlen + clen;
		if (!nlen || nlen >= sizeof(name))
			goto damaged;
		memcpy(name, h + 46, nlen);
		name[nlen] = 0;
		if (name[nlen - 1] == '/')
			continue;		/* a folder: made with its files */
		if (flags & 1) {
			bad(err, errlen, "the zip archive is encrypted");
			goto done;
		}
		if (csize == 0xFFFFFFFFUL || usize == 0xFFFFFFFFUL || off == 0xFFFFFFFFUL) {
			bad(err, errlen, "the zip archive is too big (ZIP64)");
			goto done;
		}
		if (method != 0 && method != 8) {
			bad(err, errlen, "the zip archive is packed in a way EUpdate doesn't know");
			goto done;
		}
		/* the local header, then the data */
		if (off > (unsigned long)e || (unsigned long)e - off < 30 || !get(s, (long)off, l, 30) ||
		    le32(l) != 0x04034b50UL)
			goto damaged;
		if ((unsigned long)(30 + le16(l + 26) + le16(l + 28)) > (unsigned long)e - off)
			goto damaged;
		data = (long)off + 30 + le16(l + 26) + le16(l + 28);
		if (csize > (unsigned long)(e - data))
			goto damaged;
		/* the sizes believable before anything is taken for them: a
		   stored file's the same, a deflated one at most 1032 times its
		   packed size (deflate's limit) */
		if (method == 0 ? usize != csize : usize > 0x3FFFFFFFUL || usize / 1032 > csize)
			goto damaged;
		switch (to->begin(ud, name, (long)usize)) {
		case 0: break;
		case 2: continue;		/* left out */
		default: goto done;		/* the sink's error */
		}
		m = member(s, data, csize, usize, method, crc, to, ud, piece);
		if (to->end(ud, m == 1) && m == 1)
			goto done;
		if (m == -2)
			goto done;
		if (m == -1) {
			bad(err, errlen, "not enough memory to unpack the zip archive");
			goto done;
		}
		if (m != 1)
			goto damaged;
		files++;
	}
	r = (int)files;
	goto done;
damaged:
	bad(err, errlen, "the zip archive is damaged");
done:
	if (!s->mem)
		pf_big_free(cdir);
	pf_big_free(piece);
	return r;
}

int unzip_file(const char *path, const UNZIP_TO *to, void *ud, char *err, int errlen)
{
	SRC s;
	int r;
	s.mem = 0;
	s.size = pf_size(path);
	s.h = s.size < 0 ? -1 : pf_open(path, PF_READ);
	if (s.h < 0)
		return bad(err, errlen, "the zip archive can't be read");
	r = unzip_src(&s, to, ud, err, errlen);
	pf_close(s.h);
	return r;
}

int unzip_to(const unsigned char *z, long n, const UNZIP_TO *to, void *ud, char *err, int errlen)
{
	SRC s;
	s.mem = z;
	s.h = -1;
	s.size = n;
	return unzip_src(&s, to, ud, err, errlen);
}

/* ---- each file whole, for the callers that want it so ---- */
typedef struct {
	UNZIP_FILE cb;
	void *ud;
	SBUF b;
	int nomem;
} WHOLE;

static int whole_begin(void *u, const char *name, long size)
{
	WHOLE *w = u;
	(void)name;
	sb_big_free(&w->b);
	if (sb_big(&w->b, size))
		return 0;
	w->nomem = 1;
	return 1;
}

static int whole_put(void *u, const unsigned char *p, long n)
{
	WHOLE *w = u;
	if (n > w->b.cap - 1 - w->b.len)
		return 1;
	memcpy(w->b.s + w->b.len, p, n);
	w->b.len += n;
	return 0;
}

static const char *whole_name;

static int whole_end(void *u, int ok)
{
	WHOLE *w = u;
	return ok && w->cb(w->ud, whole_name, (const unsigned char *)w->b.s, w->b.len);
}

static int whole_begin_named(void *u, const char *name, long size)
{
	whole_name = name;
	return whole_begin(u, name, size);
}

int unzip(const unsigned char *z, long n, UNZIP_FILE cb, void *ud, char *err, int errlen)
{
	static const UNZIP_TO to = { whole_begin_named, whole_put, whole_end };
	WHOLE w;
	int r;
	w.cb = cb;
	w.ud = ud;
	w.nomem = 0;
	sb_init(&w.b);
	r = unzip_to(z, n, &to, &w, err, errlen);
	if (r < 0 && w.nomem)
		str_copy(err, "not enough memory to unpack the zip archive", errlen);
	sb_big_free(&w.b);
	return r;
}
