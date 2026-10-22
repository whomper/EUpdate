/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fuzz_json.c - the JSON reader on any text: walked every way the
 * callers walk it, and skipped whole.
 */
#include <stdint.h>
#include <stddef.h>
#include "json.h"

static void walk(JSON *js, int depth)
{
	char key[16], s[24];
	switch (json_peek(js)) {
	case JS_OBJECT:
		if (json_object(js))
			while (json_key(js, key, sizeof(key)))
				walk(js, depth + 1);
		break;
	case JS_ARRAY:
		if (json_array(js))
			while (json_next(js))
				walk(js, depth + 1);
		break;
	case JS_STRING: json_string(js, s, depth & 1 ? 3 : sizeof(s)); break;
	case JS_NUMBER: json_long(js, 0); break;
	case JS_TRUE: json_bool(js); break;
	default: json_skip(js);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	JSON js;
	json_init(&js, (const char *)d, (long)n);
	walk(&js, 0);
	json_init(&js, (const char *)d, (long)n);
	json_skip(&js);
	return 0;
}
