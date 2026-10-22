/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fuzz_zip.c - a release's zip as the fuzz input says it: unzip.c's
 * reading of the directory, the headers, inflate and the CRC, and the
 * names put_file would write (made into paths, never written).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "unzip.h"
#include "update.h"

static int file(void *ud, const char *name, const unsigned char *data, long len)
{
	char out[64];
	unsigned long *sum = ud;
	disk_name(name, out, sizeof(out));
	*sum += len ? data[0] + data[len - 1] : 0;
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
	char err[120];
	unsigned long sum = 0;
	unzip(d, (long)n, file, &sum, err, sizeof(err));
	return 0;
}
