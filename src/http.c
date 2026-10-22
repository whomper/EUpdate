/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * http.c - HTTP/1.1 GET (see http.h). Everything a server sends is
 * checked before it is used (the guidelines, 7.1): the status line, a
 * header line's length and their number, Content-Length, every chunk's
 * size, the body against the caller's limit, a redirect's address.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include "plat.h"
#include "conn.h"
#include "inflate.h"
#include "http.h"

char http_user_agent[48] = "EUpdate";
void (*http_progress)(long got, long total);

#define MAXHOST   64
#define MAXPATH   4096		/* GitHub's signed download addresses are 900 and more */
#define MAXLINE   8192		/* a header line, a chunk's size line */
#define MAXHDRS   100
#define REDIRECTS 5
/* a redirect's own body, read so the connection can be used again */
#define SKIPBODY  65536L

typedef struct {
	int tls;
	char host[MAXHOST];
	unsigned short port;
	char path[MAXPATH];
} URLP;

/* ---------------- addresses ---------------- */

static int host_char(int c)
{
	return isalnum(c) || c == '.' || c == '-';
}

/* "https://host[:port]/path?query"; the fragment goes. 0 when it isn't
   one EUpdate can fetch */
static int url_parse(const char *u, URLP *p)
{
	const char *h, *e;
	long n;
	if (!strncasecmp(u, "https://", 8)) {
		p->tls = 1;
		p->port = 443;
		h = u + 8;
	} else if (!strncasecmp(u, "http://", 7)) {
		p->tls = 0;
		p->port = 80;
		h = u + 7;
	} else {
		return 0;
	}
	for (e = h; host_char((unsigned char)*e); e++)
		;
	n = e - h;
	if (n < 1 || n >= MAXHOST)
		return 0;
	memcpy(p->host, h, n);
	p->host[n] = 0;
	for (n = 0; p->host[n]; n++)
		p->host[n] = (char)tolower((unsigned char)p->host[n]);
	if (*e == ':') {
		long port = 0;
		for (e++; isdigit((unsigned char)*e) && port <= 65535; e++)
			port = port * 10 + (*e - '0');
		if (port < 1 || port > 65535)
			return 0;
		p->port = (unsigned short)port;
	}
	if (*e && *e != '/' && *e != '?' && *e != '#')
		return 0;		/* user@host, or something else after the name */
	n = 0;
	if (*e != '/')
		p->path[n++] = '/';
	for (; *e && *e != '#'; e++) {
		if ((unsigned char)*e <= ' ' || (unsigned char)*e >= 0x7f || n >= MAXPATH - 1)
			return 0;
		p->path[n++] = *e;
	}
	p->path[n] = 0;
	return 1;
}

/* where a redirect leads, from the address it came from */
static int url_follow(URLP *p, const char *loc)
{
	char abs[MAXPATH + MAXHOST + 16];
	if (loc[0] == '/' && loc[1] == '/')
		snprintf(abs, sizeof(abs), "%s:%s", p->tls ? "https" : "http", loc);
	else if (loc[0] == '/')
		snprintf(abs, sizeof(abs), "%s://%s:%u%s", p->tls ? "https" : "http", p->host, p->port, loc);
	else
		str_copy(abs, loc, sizeof(abs));
	return url_parse(abs, p);
}

/* ---------------- kept connections ---------------- */

#define KEEP 2
static struct {
	CONN *c;
	char host[MAXHOST];
	unsigned short port;
	int tls;
	unsigned long used;
} kept[KEEP];

static void drop(int i)
{
	conn_close(kept[i].c);
	kept[i].c = 0;
}

void http_close_all(void)
{
	int i;
	for (i = 0; i < KEEP; i++)
		if (kept[i].c)
			drop(i);
}

/* a connection to p's server: one kept open, if the server hasn't
   closed it meanwhile, else a new one. *reused says which */
static int take(const URLP *p, int *reused, char *err, int errlen)
{
	int i, k = 0;
	*reused = 0;
	for (i = 0; i < KEEP; i++)
		if (kept[i].c && kept[i].port == p->port && kept[i].tls == p->tls && !strcmp(kept[i].host, p->host)) {
			/* anything waiting on a quiet line is its end */
			if (conn_pending(kept[i].c)) {
				drop(i);
				k = i;
				break;
			}
			kept[i].used = pf_ms();
			*reused = 1;
			return i;
		}
	if (i == KEEP)
		for (i = 0; i < KEEP; i++) {
			if (!kept[i].c) {
				k = i;
				break;
			}
			if (kept[i].used < kept[k].used)
				k = i;
		}
	if (kept[k].c)
		drop(k);
	kept[k].c = conn_open("HTTP", p->host, p->port, 0, p->tls ? SEC_TLS : SEC_PLAIN, err, errlen);
	if (!kept[k].c)
		return -1;
	str_copy(kept[k].host, p->host, sizeof(kept[k].host));
	kept[k].port = p->port;
	kept[k].tls = p->tls;
	kept[k].used = pf_ms();
	return k;
}

/* ---------------- reading the answer ---------------- */

/* one line, without CR LF, at most MAXLINE bytes; -1 on a closed line,
   a timeout, a stop or a line too long */
static long get_line(CONN *c, SBUF *b)
{
	char ch;
	sb_reset(b);
	for (;;) {
		long r = conn_read(c, &ch, 1);
		if (r <= 0) {
			if (!r)
				str_copy(c->err, "the server closed the connection", sizeof(c->err));
			return -1;
		}
		if (ch == '\n')
			break;
		if (b->len >= MAXLINE) {
			str_copy(c->err, "a line of the answer is too long", sizeof(c->err));
			return -1;
		}
		if (!sb_addc(b, ch)) {
			str_copy(c->err, "out of memory", sizeof(c->err));
			return -1;
		}
	}
	if (b->len && b->s[b->len - 1] == '\r')
		b->s[--b->len] = 0;
	if (!b->s)
		sb_add(b, "", 0);
	return b->len;
}

/* a whole decimal number; -1 if it isn't one or is too big */
static long number(const char *s)
{
	long v = 0;
	while (*s == ' ' || *s == '\t')
		s++;
	if (!isdigit((unsigned char)*s))
		return -1;
	for (; isdigit((unsigned char)*s); s++) {
		if (v > (0x7fffffffL - 9) / 10)
			return -1;
		v = v * 10 + (*s - '0');
	}
	while (*s == ' ' || *s == '\t')
		s++;
	return *s ? -1 : v;
}

static void progress(long got, long total)
{
	if (http_progress)
		http_progress(got, total);
}

/* n bytes of the body (n < 0: until the server closes the line) into
   out, which may hold max; *closed when it did */
static int body_bytes(CONN *c, SBUF *out, long n, long max, long total, int *closed)
{
	while (n) {
		long room = n > 0 ? n : 16384, r;
		if (room > 16384)
			room = 16384;
		if (out->len + room > max)
			room = max - out->len;
		if (room <= 0) {
			str_copy(c->err, "the answer is bigger than EUpdate takes", sizeof(c->err));
			return 0;
		}
		if (!sb_room(out, room)) {
			str_copy(c->err, "out of memory", sizeof(c->err));
			return 0;
		}
		r = conn_read(c, out->s + out->len, room);
		if (r < 0)
			return 0;
		if (!r) {
			if (n < 0) {
				*closed = 1;
				return 1;
			}
			str_copy(c->err, "the server closed the connection", sizeof(c->err));
			return 0;
		}
		out->len += r;
		out->s[out->len] = 0;
		if (n > 0)
			n -= r;
		progress(out->len, total);
	}
	return 1;
}

/* a chunked body (RFC 9112, 7.1) */
static int body_chunked(CONN *c, SBUF *out, long max, SBUF *line)
{
	for (;;) {
		long size = 0;
		const char *s;
		int digits = 0, closed = 0;
		if (get_line(c, line) < 0)
			return 0;
		for (s = line->s; isxdigit((unsigned char)*s); s++, digits++) {
			if (size > 0x7ffffffL) {
				digits = 0;
				break;
			}
			size = size * 16 + (isdigit((unsigned char)*s) ? *s - '0' : (tolower((unsigned char)*s) - 'a' + 10));
		}
		if (!digits || (*s && *s != ';' && *s != ' ' && *s != '\t')) {
			str_copy(c->err, "a chunk's size is wrong", sizeof(c->err));
			return 0;
		}
		if (!size)
			break;
		if (size > max - out->len) {
			str_copy(c->err, "the answer is bigger than EUpdate takes", sizeof(c->err));
			return 0;
		}
		if (!body_bytes(c, out, size, max, -1, &closed))
			return 0;
		if (get_line(c, line) != 0) {
			str_copy(c->err, "a chunk doesn't end where it said", sizeof(c->err));
			return 0;
		}
	}
	/* the trailer, up to its empty line */
	do {
		if (get_line(c, line) < 0)
			return 0;
	} while (line->len);
	return 1;
}

typedef struct {
	long length;		/* Content-Length, -1 none */
	int chunked, gzip, close;
	char location[MAXPATH + MAXHOST + 16];
} HEAD;

static int headers(CONN *c, SBUF *line, HEAD *h, HTTPRESP *rs)
{
	int n;
	memset(h, 0, sizeof(*h));
	h->length = -1;
	for (n = 0;; n++) {
		char *v;
		if (get_line(c, line) < 0)
			return 0;
		if (!line->len)
			return 1;
		if (n >= MAXHDRS) {
			str_copy(c->err, "too many header lines", sizeof(c->err));
			return 0;
		}
		v = strchr(line->s, ':');
		if (!v)
			continue;
		*v++ = 0;
		while (*v == ' ' || *v == '\t')
			v++;
		if (!strcasecmp(line->s, "Content-Length")) {
			h->length = number(v);
			if (h->length < 0) {
				str_copy(c->err, "Content-Length is wrong", sizeof(c->err));
				return 0;
			}
		} else if (!strcasecmp(line->s, "Transfer-Encoding")) {
			h->chunked = str_istr(v, "chunked") != 0;
		} else if (!strcasecmp(line->s, "Content-Encoding")) {
			h->gzip = str_istr(v, "gzip") != 0;
		} else if (!strcasecmp(line->s, "Connection")) {
			h->close = str_istr(v, "close") != 0;
		} else if (!strcasecmp(line->s, "Location")) {
			if (strlen(v) >= sizeof(h->location)) {	/* never cut short */
				str_copy(c->err, "a redirect to an address too long", sizeof(c->err));
				return 0;
			}
			str_copy(h->location, v, sizeof(h->location));
		} else if (!strcasecmp(line->s, "X-RateLimit-Remaining")) {
			rs->rate_left = number(v);
		}
	}
}

static int is_redirect(int s)
{
	return s == 301 || s == 302 || s == 303 || s == 307 || s == 308;
}

/* one request and its answer on kept[k]; 1 when an answer came. *again
   when the kept line turned out to be closed before anything came */
static int exchange(int k, const URLP *p, int flags, const char *auth, SBUF *body, long max,
		    HTTPRESP *rs, HEAD *h, int reused, int *again, char *err, int errlen)
{
	CONN *c = kept[k].c;
	SBUF req, line, packed;
	int ok = 0, closed = 0, tries;
	long status = -1;
	*again = 0;
	sb_init(&req);
	sb_init(&line);
	sb_init(&packed);
	/* the path by itself, not through sb_printf, whose 512 bytes would cut
	   it short: GitHub sends a file's download to a signed address of 900
	   characters and more, and its server answered half of one with 400 */
	sb_adds(&req, "GET ");
	sb_adds(&req, p->path);
	sb_adds(&req, " HTTP/1.1");
	conn_log("HTTP", " >> ", req.s, req.len > 200 ? 200 : req.len);
	if ((p->tls && p->port != 443) || (!p->tls && p->port != 80))
		sb_printf(&req, "\r\nHost: %s:%u", p->host, p->port);
	else
		sb_printf(&req, "\r\nHost: %s", p->host);
	sb_printf(&req, "\r\nUser-Agent: %s\r\nAccept: %s\r\n%s", http_user_agent,
		  flags & HTTP_OCTET ? "application/octet-stream" : "application/json",
		  flags & HTTP_GZIP ? "Accept-Encoding: gzip\r\n" : "");
	if (auth)
		sb_printf(&req, "Authorization: %s\r\n", auth);
	sb_adds(&req, "\r\n");
	if (!req.s || !conn_write(c, req.s, req.len)) {
		*again = req.s && reused && !conn_stop;	/* the kept line had gone */
		goto out;
	}
	/* the status line; 1xx answers are passed over */
	for (tries = 0; tries < 4; tries++) {
		if (get_line(c, &line) < 0) {
			*again = reused && !tries && !conn_stop;
			goto out;
		}
		if (strncmp(line.s, "HTTP/1.", 7) || line.len < 12 || line.s[8] != ' ' ||
		    !isdigit((unsigned char)line.s[9]) || !isdigit((unsigned char)line.s[10]) ||
		    !isdigit((unsigned char)line.s[11]) || (line.s[12] && line.s[12] != ' ')) {
			str_copy(c->err, "not an HTTP answer", sizeof(c->err));
			goto out;
		}
		status = (line.s[9] - '0') * 100 + (line.s[10] - '0') * 10 + (line.s[11] - '0');
		if (!headers(c, &line, h, rs))
			goto out;
		if (status >= 200)
			break;
	}
	if (status < 200) {
		str_copy(c->err, "no answer after its 1xx lines", sizeof(c->err));
		goto out;
	}
	rs->status = (int)status;
	if (status == 204 || status == 304) {
		ok = 1;
	} else {
		/* a redirect's own body is read and dropped; a packed one is
		   unpacked at the end */
		int skip = is_redirect((int)status) && h->location[0];
		SBUF *to = skip || h->gzip ? &packed : body;
		long lim = skip ? SKIPBODY : max;
		long before = to->len;
		if (h->chunked)
			ok = body_chunked(c, to, before + lim, &line);
		else if (h->length >= 0)
			ok = h->length <= lim ? body_bytes(c, to, h->length, before + lim, h->length, &closed) :
			     (str_copy(c->err, "the answer is bigger than EUpdate takes", sizeof(c->err)), 0);
		else
			ok = body_bytes(c, to, -1, before + lim, -1, &closed);
		if (ok && h->gzip && !skip && packed.len) {	/* an empty body is empty, packed or not */
			int r = inflate_gzip((const unsigned char *)packed.s, packed.len, body, body->len + max);
			if (r <= 0) {
				str_copy(c->err, r < 0 ? "the answer is bigger than EUpdate takes" :
					 "the packed answer is damaged", sizeof(c->err));
				ok = 0;
			}
		}
	}
out:
	if (!ok && !*again)
		str_copy(err, c->err[0] ? c->err : "out of memory", errlen);
	if (!ok || closed || h->close)
		drop(k);
	sb_free(&req);
	sb_free(&line);
	sb_free(&packed);
	return ok;
}

int http_get(const char *url, int flags, const char *auth, const char *auth_host,
	     SBUF *body, long max, HTTPRESP *rs, char *err, int errlen)
{
	URLP p;
	HEAD h;
	int hops;
	memset(rs, 0, sizeof(*rs));
	rs->rate_left = -1;
	if (!url_parse(url, &p)) {
		str_copy(err, "not an address EUpdate can fetch", errlen);
		return 0;
	}
	for (hops = 0;; hops++) {
		int k, reused, again, tries;
		/* the token: to its host, and never on where a redirect leads */
		const char *a = !hops && auth && auth_host && !strcmp(p.host, auth_host) ? auth : 0;
		for (tries = 0; tries < 2; tries++) {
			k = take(&p, &reused, err, errlen);
			if (k < 0)
				return 0;
			if (exchange(k, &p, flags, a, body, max, rs, &h, reused, &again, err, errlen))
				break;
			if (!again)
				return 0;
		}
		if (tries == 2) {
			str_copy(err, "the server closed the connection", errlen);
			return 0;
		}
		if (!is_redirect(rs->status) || !h.location[0])
			return 1;
		if (hops >= REDIRECTS) {
			str_copy(err, "too many redirects", errlen);
			return 0;
		}
		if (!url_follow(&p, h.location)) {
			str_copy(err, "a redirect to an address EUpdate can't fetch", errlen);
			return 0;
		}
	}
}
