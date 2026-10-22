/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * http.h - HTTP/1.1 GET for EUpdate, on common/'s connections (conn.c)
 * and TLS (tls.c: BearSSL, RSA and the curves on the Falcon's DSP). Just
 * what GitHub's API and its downloads need: redirects, chunked answers,
 * gzip, and two connections kept open between requests (the API's, and
 * the one the downloads are sent on to), as a handshake costs a Falcon a
 * second or more.
 */
#ifndef HTTP_H
#define HTTP_H

#include "util.h"

/* flags */
#define HTTP_GZIP  1	/* ask for the answer packed (the API's JSON) */
#define HTTP_OCTET 2	/* Accept: application/octet-stream (a release's file) */

typedef struct {
	int status;		/* 200, 404 ... */
	long rate_left;		/* X-RateLimit-Remaining, -1 when not said */
} HTTPRESP;

/* GET url (http: or https:), its answer's body appended to body,
   unpacked, at most max bytes. auth: the value of an Authorization
   header, sent to auth_host only, and not on to where a redirect leads,
   or NULL. 1: an answer came (rs->status says which); 0: none, err says
   why ("stopped" after conn_stop) */
int  http_get(const char *url, int flags, const char *auth, const char *auth_host,
	      SBUF *body, long max, HTTPRESP *rs, char *err, int errlen);
void http_close_all(void);	/* close the kept connections */

/* how EUpdate names itself to servers (GitHub wants a User-Agent) */
extern char http_user_agent[48];
/* told as the body comes: bytes so far, and how many there will be (-1
   when the server didn't say); or NULL */
extern void (*http_progress)(long got, long total);

#endif
