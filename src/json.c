/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * json.c - the JSON pull reader (see json.h).
 */
#include <string.h>
#include "json.h"
#include "charset.h"

void json_init(JSON *js, const char *text, long n)
{
	if (!text)			/* an empty answer has no buffer at all */
		text = "", n = 0;
	js->p = text;
	js->end = text + (n > 0 ? n : 0);
	js->depth = 0;
	js->err = 0;
	js->first = 0;
}

static void fail(JSON *js)
{
	js->err = 1;
	js->p = js->end;
}

static void ws(JSON *js)
{
	while (js->p < js->end && (*js->p == ' ' || *js->p == '\t' || *js->p == '\n' || *js->p == '\r'))
		js->p++;
}

/* the next character, after white space; 0 at the end */
static int next_char(JSON *js)
{
	ws(js);
	return js->p < js->end ? (unsigned char)*js->p : 0;
}

int json_peek(JSON *js)
{
	switch (next_char(js)) {
	case '{': return JS_OBJECT;
	case '[': return JS_ARRAY;
	case '"': return JS_STRING;
	case 't': return JS_TRUE;
	case 'f': return JS_FALSE;
	case 'n': return JS_NULL;
	case '-': case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return JS_NUMBER;
	}
	return JS_NONE;
}

static int enter(JSON *js, char open)
{
	if (next_char(js) != open) {
		json_skip(js);
		return 0;
	}
	if (js->depth >= JSON_DEPTH) {
		fail(js);
		return 0;
	}
	js->p++;
	js->depth++;
	js->first = 1;
	return 1;
}

int json_object(JSON *js)
{
	return enter(js, '{');
}

int json_array(JSON *js)
{
	return enter(js, '[');
}

/* the end of an object or array, or the comma before the next of its
   members or elements: 1 when there is one */
static int more(JSON *js, char close)
{
	int c = next_char(js);
	if (js->err || !js->depth)
		return 0;
	if (c == close) {
		js->p++;
		js->depth--;
		js->first = 0;
		return 0;
	}
	if (!js->first) {
		if (c != ',') {
			fail(js);
			return 0;
		}
		js->p++;
	}
	js->first = 0;
	return 1;
}

/* four hex digits of a \u escape; -1 if they aren't */
static long hex4(JSON *js)
{
	long v = 0;
	int i;
	if (js->end - js->p < 4)
		return -1;
	for (i = 0; i < 4; i++) {
		int c = (unsigned char)*js->p++;
		v <<= 4;
		if (c >= '0' && c <= '9')
			v |= c - '0';
		else if (c >= 'a' && c <= 'f')
			v |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			v |= c - 'A' + 10;
		else
			return -1;
	}
	return v;
}

/* one UTF-8 character from the text; 0xFFFD for a broken one */
static unsigned long utf8(JSON *js)
{
	unsigned char c = (unsigned char)*js->p++;
	unsigned long u;
	int more_bytes, i;
	if (c < 0x80)
		return c;
	if (c >= 0xC2 && c <= 0xDF) {
		u = c & 0x1F;
		more_bytes = 1;
	} else if (c >= 0xE0 && c <= 0xEF) {
		u = c & 0x0F;
		more_bytes = 2;
	} else if (c >= 0xF0 && c <= 0xF4) {
		u = c & 0x07;
		more_bytes = 3;
	} else {
		return 0xFFFD;
	}
	for (i = 0; i < more_bytes; i++) {
		if (js->p >= js->end || ((unsigned char)*js->p & 0xC0) != 0x80)
			return 0xFFFD;
		u = (u << 6) | ((unsigned char)*js->p++ & 0x3F);
	}
	return u;
}

/* a string at p (its opening quote), as Atari text into out (NULL:
   only skipped) */
static int string_at(JSON *js, char *out, int size)
{
	int n = 0;
	if (next_char(js) != '"') {
		fail(js);
		return 0;
	}
	js->p++;
	for (;;) {
		unsigned long u;
		char a[4];
		int k, i;
		if (js->p >= js->end) {
			fail(js);
			return 0;
		}
		if (*js->p == '"') {
			js->p++;
			break;
		}
		if ((unsigned char)*js->p < 0x20) {
			fail(js);
			return 0;
		}
		if (*js->p == '\\') {
			if (++js->p >= js->end) {
				fail(js);
				return 0;
			}
			switch (*js->p++) {
			case '"': u = '"'; break;
			case '\\': u = '\\'; break;
			case '/': u = '/'; break;
			case 'b': u = 8; break;
			case 'f': u = 12; break;
			case 'n': u = '\n'; break;
			case 'r': u = '\r'; break;
			case 't': u = '\t'; break;
			case 'u': {
				long h = hex4(js);
				if (h < 0) {
					fail(js);
					return 0;
				}
				u = (unsigned long)h;
				/* a surrogate pair: one character */
				if (u >= 0xD800 && u <= 0xDBFF && js->end - js->p >= 6 && js->p[0] == '\\' &&
				    js->p[1] == 'u') {
					const char *keep = js->p;
					long lo;
					js->p += 2;
					lo = hex4(js);
					if (lo >= 0xDC00 && lo <= 0xDFFF)
						u = 0x10000 + ((u - 0xD800) << 10) + (unsigned long)(lo - 0xDC00);
					else
						js->p = keep;
				}
				break;
			}
			default:
				fail(js);
				return 0;
			}
		} else {
			u = utf8(js);
		}
		if (!out)
			continue;
		k = u < 0x80 ? (a[0] = (char)u, 1) : uni_to_atari(u, a);
		for (i = 0; i < k && n < size - 1; i++)
			out[n++] = a[i];
	}
	if (out && size > 0)
		out[n] = 0;
	return 1;
}

int json_key(JSON *js, char *key, int size)
{
	if (size > 0)
		key[0] = 0;
	if (!more(js, '}'))
		return 0;
	if (!string_at(js, key, size))
		return 0;
	if (next_char(js) != ':') {
		fail(js);
		return 0;
	}
	js->p++;
	return 1;
}

int json_next(JSON *js)
{
	return more(js, ']');
}

int json_string(JSON *js, char *out, int size)
{
	if (size > 0)
		out[0] = 0;
	if (json_peek(js) != JS_STRING) {
		json_skip(js);
		return 0;
	}
	return string_at(js, out, size);
}

static int literal(JSON *js, const char *word)
{
	long n = (long)strlen(word);
	if (js->end - js->p < n || memcmp(js->p, word, n)) {
		fail(js);
		return 0;
	}
	js->p += n;
	return 1;
}

/* a number's characters (RFC 8259, 6, a little more lenient) */
static void number_skip(JSON *js)
{
	const char *s = js->p;
	while (js->p < js->end && (strchr("+-.eE", *js->p) || (*js->p >= '0' && *js->p <= '9')))
		js->p++;
	if (js->p == s)
		fail(js);
}

long json_long(JSON *js, long def)
{
	long v = 0;
	int neg = 0, digits = 0;
	if (json_peek(js) != JS_NUMBER) {
		json_skip(js);
		return def;
	}
	if (*js->p == '-') {
		neg = 1;
		js->p++;
	}
	while (js->p < js->end && *js->p >= '0' && *js->p <= '9') {
		if (v > (0x7fffffffL - 9) / 10) {
			number_skip(js);
			return def;
		}
		v = v * 10 + (*js->p++ - '0');
		digits++;
	}
	if (!digits) {
		fail(js);
		return def;
	}
	if (js->p < js->end && strchr(".eE", *js->p)) {
		number_skip(js);
		return def;
	}
	return neg ? -v : v;
}

int json_bool(JSON *js)
{
	if (json_peek(js) == JS_TRUE)
		return literal(js, "true");
	json_skip(js);
	return 0;
}

void json_skip(JSON *js)
{
	switch (json_peek(js)) {
	case JS_OBJECT:
		if (json_object(js))
			while (json_key(js, 0, 0))
				json_skip(js);
		break;
	case JS_ARRAY:
		if (json_array(js))
			while (json_next(js))
				json_skip(js);
		break;
	case JS_STRING: string_at(js, 0, 0); break;
	case JS_NUMBER: number_skip(js); break;
	case JS_TRUE: literal(js, "true"); break;
	case JS_FALSE: literal(js, "false"); break;
	case JS_NULL: literal(js, "null"); break;
	default: fail(js);
	}
}
