/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * json.h - a small pull reader for JSON (RFC 8259), for GitHub's
 * answers: the caller walks the text and takes what it wants, nothing
 * is built in memory. Every step checks where it is against the end of
 * the text and the nesting against JSON_DEPTH; after any error each call
 * gives nothing more, and js.err says so (the guidelines, 7.1).
 *
 *   json_init(&js, text, n);
 *   if (json_object(&js))
 *       while (json_key(&js, key, sizeof(key)))
 *           if (!strcmp(key, "tag_name")) json_string(&js, tag, sizeof(tag));
 *           else json_skip(&js);
 */
#ifndef JSON_H
#define JSON_H

#define JSON_DEPTH 32

enum { JS_NONE, JS_OBJECT, JS_ARRAY, JS_STRING, JS_NUMBER, JS_TRUE, JS_FALSE, JS_NULL };

typedef struct {
	const char *p, *end;
	int depth;
	int err;		/* 1 once the text was found wrong (or nested too deep) */
	int first;		/* the next member or element is the first of its object or array */
} JSON;

void json_init(JSON *js, const char *text, long n);
int  json_peek(JSON *js);		/* JS_*: what the next value is */
/* into an object or an array: 1 when the next value is one */
int  json_object(JSON *js);
int  json_array(JSON *js);
/* the next member's name (as Atari text, cut to size), 1; 0 at the
   object's end (then it is left) */
int  json_key(JSON *js, char *key, int size);
/* 1 while the array has another element; 0 at its end (then it is left) */
int  json_next(JSON *js);
/* a string as Atari text (common's charset: Hebrew at 0xC2-0xDC, '?'
   for what the Atari can't show), cut to size; 0 (and "") for null and
   for anything not a string, which is skipped */
int  json_string(JSON *js, char *out, int size);
/* a whole number; def for anything else, or one too big for a long */
long json_long(JSON *js, long def);
int  json_bool(JSON *js);		/* true: 1; anything else 0 */
void json_skip(JSON *js);		/* the next value, however deep */

#endif
