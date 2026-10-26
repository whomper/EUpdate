/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * unit.c - EUpdate's core on this computer (make test): the JSON reader,
 * HTTP against answers fuzz_net.c plays (chunked, gzip, redirects, a
 * kept line, broken ones), GitHub's answers, zips, names on the disk,
 * EUPDATE.INF. Run from the repository's top.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "plat.h"
#include "util.h"
#include "json.h"
#include "http.h"
#include "gh.h"
#include "store.h"
#include "unzip.h"
#include "update.h"
#include "conn.h"
#include "fuzz_net.h"

static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static char *load(const char *path, long *n)
{
	char *b = pf_load(path, n);
	if (!b) {
		printf("can't read %s (run from the repository's top)\n", path);
		exit(1);
	}
	return b;
}

/* ---------------- JSON ---------------- */

static void test_json(void)
{
	static const char doc[] =
		"{ \"tag_name\": \"v1.45\", \"n\": -42, \"big\": 99999999999, \"f\": 1.5e3,\n"
		"  \"esc\": \"a\\\"b\\\\c\\u00e9\\ud83d\\ude00\\n\", \"heb\": \"\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d\",\n"
		"  \"nested\": { \"a\": [1, [2, {\"x\": null}], \"s\"], \"b\": true }, \"empty\": {}, \"e2\": [],\n"
		"  \"last\": false }";
	JSON js;
	char key[32], s[64];
	int seen = 0;
	json_init(&js, doc, (long)strlen(doc));
	CHECK(json_object(&js));
	while (json_key(&js, key, sizeof(key))) {
		seen++;
		if (!strcmp(key, "tag_name")) {
			CHECK(json_string(&js, s, sizeof(s)) && !strcmp(s, "v1.45"));
		} else if (!strcmp(key, "n")) {
			CHECK(json_long(&js, 0) == -42);
		} else if (!strcmp(key, "big")) {
			CHECK(json_long(&js, 7) == 7);
		} else if (!strcmp(key, "f")) {
			CHECK(json_long(&js, 8) == 8);
		} else if (!strcmp(key, "esc")) {
			/* é is 0x82 in the Atari's character set; the emoji has none, and goes */
			CHECK(json_string(&js, s, sizeof(s)) && !strcmp(s, "a\"b\\c\x82\n"));
		} else if (!strcmp(key, "heb")) {
			/* shin, lamed, vav, final mem: Atari 0xD6 0xCD 0xC7 0xDA */
			CHECK(json_string(&js, s, sizeof(s)) && !strcmp(s, "\xD6\xCD\xC7\xDA"));
		} else if (!strcmp(key, "nested")) {
			CHECK(json_object(&js));
			CHECK(json_key(&js, key, sizeof(key)) && !strcmp(key, "a"));
			json_skip(&js);
			CHECK(json_key(&js, key, sizeof(key)) && !strcmp(key, "b"));
			CHECK(json_bool(&js) == 1);
			CHECK(!json_key(&js, key, sizeof(key)));
		} else if (!strcmp(key, "last")) {
			CHECK(json_bool(&js) == 0);
		} else {
			json_skip(&js);
		}
	}
	CHECK(seen == 10);
	CHECK(!js.err && js.depth == 0);

	/* cut short, wrong, too deep: an error, never a read past the end */
	{
		static const char *bad[] = { "{\"a\": \"abc", "{\"a\" 1}", "[1 2]", "{\"a\": tru}", "[\"\x01\"]",
					     "{\"a\":\"\\uZZZZ\"}", "[" };
		int i;
		for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++) {
			json_init(&js, bad[i], (long)strlen(bad[i]));
			json_skip(&js);
			CHECK(js.err);
		}
	}
	{
		char deep[200];
		memset(deep, '[', sizeof(deep));
		json_init(&js, deep, sizeof(deep));
		json_skip(&js);
		CHECK(js.err);
	}
	/* a string cut to its room */
	json_init(&js, "\"abcdefgh\"", 10);
	CHECK(json_string(&js, s, 4) && !strcmp(s, "abc"));
}

/* ---------------- HTTP ---------------- */

static int get(const char *answer, const char *url, int flags, SBUF *b, HTTPRESP *rs, char *err)
{
	fuzz_net_set((const unsigned char *)answer, (long)strlen(answer));
	sb_reset(b);
	return http_get(url, flags, 0, 0, b, 1 << 20, rs, err, 200);
}

static void test_http(void)
{
	SBUF b;
	HTTPRESP rs;
	char err[200], *gz, ans[2000];
	long gzn;
	sb_init(&b);

	CHECK(get("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-RateLimit-Remaining: 59\r\n\r\nhello",
		  "http://api.test/x", 0, &b, &rs, err));
	CHECK(rs.status == 200 && b.len == 5 && !strcmp(b.s, "hello") && rs.rate_left == 59);
	http_close_all();

	CHECK(get("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		  "4\r\nWiki\r\n5;ext=1\r\npedia\r\n0\r\nTrailer: x\r\n\r\n", "http://api.test/", 0, &b, &rs, err));
	CHECK(b.len == 9 && !strcmp(b.s, "Wikipedia"));
	http_close_all();

	/* a redirect, its own body dropped; then the answer, until the line closes */
	CHECK(get("HTTP/1.1 302 Found\r\nLocation: https://cdn.test/file\r\nContent-Length: 3\r\n\r\nxyz"
		  "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.0 200 OK\r\nConnection: close\r\n\r\nthe file",
		  "http://api.test/asset", HTTP_OCTET, &b, &rs, err) == 0);
	/* (https needs TLS: the fuzz network can't do it; a plain one can) */
	/* (each connection its part of the input: a line kept open with an
	   answer already waiting on it is taken as closed, and dropped) */
	CHECK(get("HTTP/1.1 302 Found\r\nLocation: /file\r\nContent-Length: 3\r\n\r\nxyz" FUZZ_NET_NEXT
		  "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.0 200 OK\r\nConnection: close\r\n\r\nthe file",
		  "http://api.test/asset", HTTP_OCTET, &b, &rs, err));
	CHECK(rs.status == 200 && !strcmp(b.s, "the file"));
	http_close_all();

	/* a redirect to a signed address as long as GitHub's (900 characters
	   and more) is followed whole; one longer than EUpdate takes is
	   refused, never cut short (1.0 cut the request line at 511 bytes) */
	{
		static char ans[12000], loc[5200];
		memset(loc, 'a', sizeof(loc));
		memcpy(loc, "/file?sig=", 10);
		loc[3000] = 0;
		snprintf(ans, sizeof(ans), "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n" FUZZ_NET_NEXT
			 "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok", loc);
		CHECK(get(ans, "http://api.test/asset", HTTP_OCTET, &b, &rs, err) && !strcmp(b.s, "ok"));
		http_close_all();
		loc[3000] = 'a';
		loc[5100] = 0;
		snprintf(ans, sizeof(ans), "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", loc);
		CHECK(!get(ans, "http://api.test/asset", HTTP_OCTET, &b, &rs, err) && strstr(err, "too long"));
		http_close_all();
	}

	/* gzip */
	gz = load("tests/data/hello.gz", &gzn);
	snprintf(ans, sizeof(ans), "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: %ld\r\n\r\n", gzn);
	{
		long h = (long)strlen(ans);
		memcpy(ans + h, gz, gzn);
		fuzz_net_set((const unsigned char *)ans, h + gzn);
		sb_reset(&b);
		CHECK(http_get("http://api.test/gz", HTTP_GZIP, 0, 0, &b, 1 << 20, &rs, err, sizeof(err)));
		CHECK(b.len == 13 * 50 && !strncmp(b.s, "hello, Atari\n", 13));
		http_close_all();
		/* more than the caller takes */
		fuzz_net_set((const unsigned char *)ans, h + gzn);
		sb_reset(&b);
		CHECK(!http_get("http://api.test/gz", HTTP_GZIP, 0, 0, &b, 100, &rs, err, sizeof(err)));
		http_close_all();
	}
	free(gz);

	/* broken answers */
	CHECK(!get("HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\n", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nfffffffff\r\n", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabcd\r\n0\r\n\r\n", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("SSH-2.0-OpenSSH\r\n", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("HTTP/1.1 301 Moved\r\nLocation: ftp://x/\r\nContent-Length: 0\r\n\r\n", "http://a.test/", 0, &b, &rs, err));
	http_close_all();
	CHECK(!get("", "http://user@a.test/", 0, &b, &rs, err));
	CHECK(!get("", "gopher://a.test/", 0, &b, &rs, err));
	CHECK(!get("", "http://a.test:99999/", 0, &b, &rs, err));
	/* a line that never ends */
	{
		char *big = malloc(20000);
		memcpy(big, "HTTP/1.1 200 OK\r\nX: ", 20);
		memset(big + 20, 'a', 20000 - 21);
		big[19999] = 0;
		CHECK(!get(big, "http://a.test/", 0, &b, &rs, err));
		http_close_all();
		free(big);
	}
	sb_free(&b);
}

/* ---------------- GitHub's answers ---------------- */

/* what the server says next */
static void say(const char *answer)
{
	fuzz_net_set((const unsigned char *)answer, (long)strlen(answer));
}

static void test_gh(void)
{
	static const char rel[] =
		"HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s";
	static const char body[] =
		"{\"url\":\"u\",\"tag_name\":\"v1.45\",\"name\":\"EFTP 1.45\",\"author\":{\"login\":\"whomper\"},"
		"\"published_at\":\"2026-10-20T09:15:00Z\",\"assets\":["
		"{\"id\":101,\"name\":\"EFTP.PRG\",\"size\":180000,\"uploader\":{\"id\":1}},"
		"{\"id\":102,\"name\":\"EFTP-1.45.zip\",\"size\":400000},"
		"{\"id\":103,\"name\":\"EFTP.HYP\",\"size\":30000}],\"body\":\"notes\"}";
	static const char search[] =
		"{\"total_count\":2,\"incomplete_results\":false,\"items\":["
		"{\"full_name\":\"whomper/EFTP\",\"description\":\"FTP for the Atari\",\"stargazers_count\":7,"
		"\"pushed_at\":\"2026-10-21T10:00:00Z\",\"fork\":false,\"archived\":false,\"owner\":{\"login\":\"whomper\"}},"
		"{\"full_name\":\"bad/../name\",\"description\":null}]}";
	char ans[3000], err[200];
	GHREL r;
	GHREPO repos[4];
	GHUSER users[4];
	REPO sub;
	short which[GH_MAXASSET];
	long total;
	int n;

	str_copy(gh_base, "http://api.test", sizeof(gh_base));
	snprintf(ans, sizeof(ans), rel, (int)strlen(body), body);
	fuzz_net_set((const unsigned char *)ans, (long)strlen(ans));
	CHECK(gh_latest("whomper/EFTP", &r, err, sizeof(err)) == 1);
	CHECK(!strcmp(r.tag, "v1.45") && !strcmp(r.date, "2026-10-20") && r.nasset == 3);
	CHECK(r.asset[1].id == 102 && r.asset[1].size == 400000 && !strcmp(r.asset[2].name, "EFTP.HYP"));
	http_close_all();

	/* which files: the zip by itself, or what the patterns say */
	memset(&sub, 0, sizeof(sub));
	CHECK(up_chosen(&sub, &r, which) == 1 && which[0] == 1);
	str_copy(sub.files, "*.prg, eftp.hy?", sizeof(sub.files));
	CHECK(up_chosen(&sub, &r, which) == 2 && which[0] == 0 && which[1] == 2);
	r.asset[1].name[0] = 'x';
	r.asset[1].name[strlen(r.asset[1].name) - 1] = 'x';	/* no zip: every file */
	sub.files[0] = 0;
	CHECK(up_chosen(&sub, &r, which) == 3);

	/* an edition and its ST twin, and accessories: the suggested rules */
	{
		static const char *const names[] = { "ECAL.ACC", "ECALST.ACC", "ECAL.PRG", "ECAL.HYP", "ECAL-2.21.zip" };
		GHREL e;
		int i;
		memset(&e, 0, sizeof(e));
		for (i = 0; i < 5; i++) {
			e.asset[i].id = i + 1;
			str_copy(e.asset[i].name, names[i], sizeof(e.asset[i].name));
		}
		e.nasset = 5;
		memset(&sub, 0, sizeof(sub));
		up_suggest(&sub, &e, 0);
		CHECK(!strcmp(sub.rule[0].names, "ECALST.ACC") && !strcmp(sub.rule[0].folder, "-"));
		CHECK(!strcmp(sub.rule[1].names, "*.ACC") && !strcmp(sub.rule[1].folder, "C:/"));
		CHECK(up_rule(&sub, "ecalst.acc") == &sub.rule[0] && up_rule(&sub, "x/ECAL.ACC") == &sub.rule[1]);
		CHECK(!up_rule(&sub, "ECAL.PRG"));
		str_copy(sub.files, "*.ACC", sizeof(sub.files));
		CHECK(up_chosen(&sub, &e, which) == 1 && which[0] == 0);	/* the twin isn't even fetched */
		up_suggest(&sub, &e, 1);
		CHECK(!strcmp(sub.rule[0].names, "ECAL.ACC"));
	}

	/* no release; then no repository */
	say("HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\n\r\n{}" FUZZ_NET_NEXT
	    "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}");
	n = gh_latest("whomper/EFTP", &r, err, sizeof(err));
	CHECK(n == 0 && strstr(err, "no release"));
	http_close_all();
	say("HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\n\r\n{}" FUZZ_NET_NEXT
	    "HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\n\r\n{}");
	n = gh_latest("whomper/NONE", &r, err, sizeof(err));
	CHECK(n == 0 && strstr(err, "no such"));
	http_close_all();
	/* the hour's limit */
	{
		const char *lim = "HTTP/1.1 403 Forbidden\r\nX-RateLimit-Remaining: 0\r\nContent-Length: 2\r\n\r\n{}";
		fuzz_net_set((const unsigned char *)lim, (long)strlen(lim));
		CHECK(gh_latest("whomper/EFTP", &r, err, sizeof(err)) == -1 && strstr(err, "limit"));
		http_close_all();
	}
	/* names that never go into an address */
	CHECK(gh_valid_full("whomper/EFTP_XL") && gh_valid_full("a-b/c.d"));
	CHECK(!gh_valid_full("whomper") && !gh_valid_full("a/b/c") && !gh_valid_full("a/..") &&
	      !gh_valid_full("/x") && !gh_valid_full("a b/c") && !gh_valid_full("a/b?c"));

	snprintf(ans, sizeof(ans), rel, (int)strlen(search), search);
	fuzz_net_set((const unsigned char *)ans, (long)strlen(ans));
	n = gh_find_repos("eftp", repos, 4, &total, err, sizeof(err));
	CHECK(n == 1 && total == 2 && !strcmp(repos[0].full, "whomper/EFTP") && repos[0].stars == 7);
	CHECK(!strcmp(repos[0].pushed, "2026-10-21") && !strcmp(repos[0].desc, "FTP for the Atari"));
	http_close_all();
	{
		static const char users_body[] = "{\"total_count\":1,\"items\":[{\"login\":\"whomper\",\"type\":\"User\"},"
						 "{\"login\":\"atari\",\"type\":\"Organization\"}]}";
		snprintf(ans, sizeof(ans), rel, (int)strlen(users_body), users_body);
		fuzz_net_set((const unsigned char *)ans, (long)strlen(ans));
		n = gh_find_owners("atari", users, 4, &total, err, sizeof(err));
		CHECK(n == 2 && !users[0].org && users[1].org && !strcmp(users[1].login, "atari"));
		http_close_all();
	}
	CHECK(gh_find_repos("", repos, 4, &total, err, sizeof(err)) == -1);
	str_copy(gh_base, "https://api.github.com", sizeof(gh_base));
}

/* ---------------- zips and names ---------------- */

typedef struct {
	int n;
	char names[8][64];
	long sizes[8];
} SEEN;

static int seen_file(void *ud, const char *name, const unsigned char *data, long len)
{
	SEEN *s = ud;
	if (s->n < 8) {
		str_copy(s->names[s->n], name, 64);
		s->sizes[s->n] = len;
	}
	s->n++;
	return 0;
}

static void rm_rf(const char *dir)
{
	char cmd[300];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
	if (system(cmd))
		printf("can't remove %s\n", dir);
}

static void test_zip(void)
{
	long n;
	char *z = load("tests/data/good.zip", &n), err[200], name[64], dir[] = "build/unit-zip";
	SEEN s;
	REPO r;
	memset(&s, 0, sizeof(s));
	CHECK(unzip((unsigned char *)z, n, seen_file, &s, err, sizeof(err)) == 2);
	CHECK(s.n == 2 && !strcmp(s.names[0], "EUPDATE.PRG") && s.sizes[0] == 10240 &&
	      !strcmp(s.names[1], "icons/README.TXT") && s.sizes[1] == 22 * 200);
	CHECK(unzip((unsigned char *)z, n - 1, seen_file, &s, err, sizeof(err)) == -1);
	CHECK(unzip((unsigned char *)z, 10, seen_file, &s, err, sizeof(err)) == -1);
	free(z);
	z = load("tests/data/bad.zip", &n);
	memset(&s, 0, sizeof(s));
	CHECK(unzip((unsigned char *)z, n, seen_file, &s, err, sizeof(err)) == -1 && strstr(err, "damaged"));
	free(z);

	CHECK(crc32((const unsigned char *)"123456789", 9) == 0xCBF43926UL);

	opt.shorten = 1;
	disk_name("EFTP-1.45.zip", name, sizeof(name));
	CHECK(!strcmp(name, "EFTP-1_4.ZIP"));
	disk_name("readme", name, sizeof(name));
	CHECK(!strcmp(name, "README"));
	disk_name(".hidden", name, sizeof(name));
	CHECK(!strcmp(name, "_HIDDEN"));
	disk_name("a b+c.tar.gz", name, sizeof(name));
	CHECK(!strcmp(name, "A_B_C_TA.GZ"));
	opt.shorten = 0;
	disk_name("EFTP-1.45.zip", name, sizeof(name));
	CHECK(!strcmp(name, "EFTP-1.45.zip"));
	opt.shorten = 1;
	CHECK(wild_match("*.PRG", "eftp.prg") && wild_match("E?TP*", "EFTP.HYP") && wild_match("*", "x"));
	CHECK(!wild_match("*.PRG", "EFTP.PRG.zip") && !wild_match("A?", "A"));

	/* a release written into a folder: unpacked, its folder made; and
	   one whose names climb out of it, refused */
	rm_rf(dir);
	memset(&r, 0, sizeof(r));
	str_copy(r.folder, dir, sizeof(r.folder));
	r.unzip = 1;
	pf_mkdir(dir);
	z = load("tests/data/good.zip", &n);
	CHECK(up_put(&r, "EUpdate-1.0.zip", (unsigned char *)z, n, err, sizeof(err)));
	CHECK(pf_size("build/unit-zip/EUPDATE.PRG") == 10240 && pf_size("build/unit-zip/ICONS/README.TXT") == 4400);
	r.unzip = 0;
	CHECK(up_put(&r, "EUpdate-1.0.zip", (unsigned char *)z, n, err, sizeof(err)));
	CHECK(pf_size("build/unit-zip/EUPDATE-.ZIP") == n);
	free(z);
	r.unzip = 1;
	z = load("tests/data/evil.zip", &n);
	CHECK(!up_put(&r, "evil.zip", (unsigned char *)z, n, err, sizeof(err)) && strstr(err, "won't write"));
	CHECK(pf_size("build/EVIL.PRG") < 0);
	free(z);
	CHECK(!up_put(&r, "..", (unsigned char *)"x", 1, err, sizeof(err)));

	/* from a file, as a download is unpacked: the same files, no NAME.EX$
	   left; a damaged archive leaves the file there as it was */
	rm_rf(dir);
	pf_mkdir(dir);
	CHECK(up_put_file(&r, "EUpdate-1.0.zip", "tests/data/good.zip", err, sizeof(err)));
	CHECK(pf_size("build/unit-zip/EUPDATE.PRG") == 10240 && pf_size("build/unit-zip/ICONS/README.TXT") == 4400);
	CHECK(pf_size("build/unit-zip/EUPDATE.PR$") < 0 && pf_size("build/unit-zip/ICONS/README.TX$") < 0);
	z = load("tests/data/good.zip", &n);
	memset(&s, 0, sizeof(s));
	{
		FILE *f = fopen("build/unit-zip/BAD.ZIP", "wb");
		long k;
		/* a byte of EUPDATE.PRG's data turned over: its CRC is wrong */
		for (k = 0; k < n; k++)
			fputc(k == 100 ? z[k] ^ 0x55 : z[k], f);
		fclose(f);
	}
	free(z);
	z = load("build/unit-zip/EUPDATE.PRG", &n);
	CHECK(!up_put_file(&r, "EUpdate-1.0.zip", "build/unit-zip/BAD.ZIP", err, sizeof(err)) && strstr(err, "damaged"));
	{
		long m;
		char *now = load("build/unit-zip/EUPDATE.PRG", &m);
		CHECK(m == n && !memcmp(now, z, n));
		free(now);
	}
	free(z);
	CHECK(pf_size("build/unit-zip/EUPDATE.PR$") < 0);
	r.unzip = 0;
	CHECK(up_put_file(&r, "CACERT.PEM", "tests/data/good.zip", err, sizeof(err)));
	CHECK(pf_size("build/unit-zip/CACERT.PEM") == pf_size("tests/data/good.zip"));
	r.unzip = 1;

	/* rules: a file elsewhere by its name alone, one left out */
	rm_rf(dir);
	pf_mkdir(dir);
	str_copy(r.rule[0].names, "readme.txt", sizeof(r.rule[0].names));
	str_copy(r.rule[0].folder, "-", sizeof(r.rule[0].folder));
	str_copy(r.rule[1].names, "*.PRG", sizeof(r.rule[1].names));
	str_copy(r.rule[1].folder, "build/unit-zip/A/ROOT", sizeof(r.rule[1].folder));	/* made, both */
	z = load("tests/data/good.zip", &n);
	CHECK(up_put(&r, "EUpdate-1.0.zip", (unsigned char *)z, n, err, sizeof(err)));
	CHECK(pf_size("build/unit-zip/A/ROOT/EUPDATE.PRG") == 10240 && pf_size("build/unit-zip/EUPDATE.PRG") < 0);
	CHECK(pf_size("build/unit-zip/ICONS/README.TXT") < 0);
	free(z);
	rm_rf(dir);
}

/* ---------------- TLS.OVL and the ST editions (the guidelines 7.4) ---------------- */

static char replaced_old[16], replaced_now[16];
static void note_replaced(const REPO *r, const char *old, const char *now)
{
	str_copy(replaced_old, old, sizeof(replaced_old));
	str_copy(replaced_now, now, sizeof(replaced_now));
}

static GHREL release_of(const char *const *names, int n)
{
	GHREL e;
	int i;
	memset(&e, 0, sizeof(e));
	for (i = 0; i < n; i++) {
		e.asset[i].id = i + 1;
		e.asset[i].size = 4;
		str_copy(e.asset[i].name, names[i], sizeof(e.asset[i].name));
	}
	e.nasset = n;
	str_copy(e.tag, "v3.0", sizeof(e.tag));
	return e;
}

static void test_tlsovl(void)
{
	static const char *const now[] = { "ECAL.ACC", "ECAL.HYP", "TLS.OVL", "ECAL-3.0.zip" };
	static const char *const then[] = { "ECAL.ACC", "ECALST.ACC", "TLS.OVL" };
	static const char *const two = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nACC!"
				       FUZZ_NET_NEXT "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nOVL!";
	char dir[] = "build/unit-st";
	short which[GH_MAXASSET];
	GHREL e = release_of(now, 4);
	REPO r;

	/* TLS.OVL goes with a program whatever the patterns say; not with
	   files that aren't one, nor against a rule's "-" */
	memset(&r, 0, sizeof(r));
	CHECK(up_chosen(&r, &e, which) == 1 && which[0] == 3);		/* the zip: it's in there */
	str_copy(r.files, "*.ACC", sizeof(r.files));
	CHECK(up_chosen(&r, &e, which) == 2 && which[0] == 0 && which[1] == 2);
	str_copy(r.files, "*.HYP", sizeof(r.files));
	CHECK(up_chosen(&r, &e, which) == 1 && which[0] == 1);
	str_copy(r.files, "*.ACC", sizeof(r.files));
	str_copy(r.rule[0].names, "TLS.OVL", sizeof(r.rule[0].names));
	str_copy(r.rule[0].folder, "-", sizeof(r.rule[0].folder));
	CHECK(up_chosen(&r, &e, which) == 1 && which[0] == 0);

	/* an ST edition by hand beside the accessory: replaced once the
	   release has no ST edition; switched off (.ACX), left; the program
	   and TLS.OVL in their places */
	rm_rf(dir);
	pf_mkdir(dir);
	pf_mkdir("build/unit-st/ROOT");
	CHECK(pf_save("build/unit-st/ROOT/ECALST.ACC", "old", 3) == 0);
	CHECK(pf_save("build/unit-st/ROOT/ECALST.ACX", "off", 3) == 0);
	memset(&r, 0, sizeof(r));
	str_copy(r.full, "whomper/ECal", sizeof(r.full));
	str_copy(r.folder, "build/unit-st/ECAL", sizeof(r.folder));
	str_copy(r.files, "*.ACC", sizeof(r.files));
	str_copy(r.rule[0].names, "*.ACC", sizeof(r.rule[0].names));
	str_copy(r.rule[0].folder, "build/unit-st/ROOT", sizeof(r.rule[0].folder));
	str_copy(gh_base, "http://api.test", sizeof(gh_base));
	up_replaced = note_replaced;
	opt.shorten = 1;
	say(two);
	CHECK(up_install(&r, &e) && !strcmp(r.have, "v3.0"));
	http_close_all();
	CHECK(pf_size("build/unit-st/ROOT/ECAL.ACC") == 4 && pf_size("build/unit-st/ECAL/TLS.OVL") == 4);
	CHECK(pf_size("build/unit-st/ROOT/ECALST.ACC") < 0 && pf_size("build/unit-st/ROOT/ECALST.ACX") == 3);
	CHECK(!strcmp(replaced_old, "ECALST.ACC") && !strcmp(replaced_now, "ECAL.ACC"));

	/* a release that still has its ST edition (left out here): kept */
	CHECK(pf_save("build/unit-st/ROOT/ECALST.ACC", "old", 3) == 0);
	replaced_old[0] = 0;
	e = release_of(then, 3);
	r.rule[1] = r.rule[0];		/* the suggested rules' order: "-" first */
	str_copy(r.rule[0].names, "ECALST.ACC", sizeof(r.rule[0].names));
	str_copy(r.rule[0].folder, "-", sizeof(r.rule[0].folder));
	say(two);
	CHECK(up_install(&r, &e));
	http_close_all();
	CHECK(pf_size("build/unit-st/ROOT/ECALST.ACC") == 3 && !replaced_old[0]);

	/* as a release comes, in its zip (tests/data/onefor.zip: ECAL.ACC,
	   TLS.OVL, ECAL.HYP, icons/ECAL.RSC), by the suggested rules */
	{
		static const char *const zipped[] = { "ECAL-3.0.zip" };
		long n;
		char *z = load("tests/data/onefor.zip", &n), *ans = malloc(n + 80);
		int k = snprintf(ans, 80, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", n);
		memcpy(ans + k, z, n);
		e = release_of(zipped, 1);
		e.asset[0].size = n;
		r.files[0] = 0;
		r.unzip = 1;
		fuzz_net_set((unsigned char *)ans, k + n);
		CHECK(up_install(&r, &e));
		http_close_all();
		CHECK(pf_size("build/unit-st/ROOT/ECAL.ACC") == 256 && pf_size("build/unit-st/ECAL/TLS.OVL") == 256);
		CHECK(pf_size("build/unit-st/ROOT/ECALST.ACC") < 0 && !strcmp(replaced_old, "ECALST.ACC"));
		free(ans);
		free(z);
	}
	up_replaced = 0;
	str_copy(gh_base, "https://api.github.com", sizeof(gh_base));
	rm_rf(dir);
}

/* ---------------- EUPDATE.INF ---------------- */

static void test_store(void)
{
	char dir[] = "build/unit-store";
	REPO *r;
	rm_rf(dir);
	pf_mkdir(dir);
	store_init(dir);
	CHECK(nrepos == 0 && opt.check == CHECK_START && opt.shorten == 1 && opt.dialogs3d == 1);
	r = repo_add("whomper/EFTP");
	CHECK(r && !repo_add("WHOMPER/eftp"));
	str_copy(r->folder, "C:\\NET\\EFTP", sizeof(r->folder));
	str_copy(r->have, "v1.44", sizeof(r->have));
	str_copy(r->latest, "v1.45", sizeof(r->latest));
	str_copy(r->files, "*.PRG,*.HYP", sizeof(r->files));
	str_copy(r->rule[0].names, "EFTPST.PRG", sizeof(r->rule[0].names));
	str_copy(r->rule[0].folder, "-", sizeof(r->rule[0].folder));
	str_copy(r->rule[1].names, "*.ACC", sizeof(r->rule[1].names));
	str_copy(r->rule[1].folder, "C:\\", sizeof(r->rule[1].folder));
	CHECK(!repo_current(r));
	r = repo_add("whomper/ECal");
	str_copy(r->have, "v2.21", sizeof(r->have));
	str_copy(r->latest, "v2.21", sizeof(r->latest));
	CHECK(repo_current(r));
	opt.check = CHECK_EVERY;
	opt.hours = 6;
	opt.dialogs3d = 0;
	str_copy(opt.token, "ghp_secret;with stuff", sizeof(opt.token));
	CHECK(store_save() == 0);
	{
		long n;
		char path[100], *text;
		snprintf(path, sizeof(path), "%s/EUPDATE.INF", dir);
		text = load(path, &n);
		CHECK(!strstr(text, "ghp_secret"));	/* encrypted */
		CHECK(strstr(text, "REPO=whomper/ECal") != 0);
		/* guidelines 6.1: an empty line after every setting, one before each [REPO] */
		CHECK(strstr(text, "HAVEDATE=\r\n\r\n; the latest") != 0);
		CHECK(strstr(text, "LATESTDATE=\r\n\r\n[REPO]\r\nREPO=whomper/ECal\r\n\r\n") != 0);
		CHECK(strstr(text, "\r\n\r\n[OPTIONS]\r\n; check") != 0);
		CHECK(!strstr(text, "\r\n\r\n\r\n") && !strstr(text, "\r\n[REPO]\r\n\r\n"));
		/* guidelines 5.6: the 3D look's setting, its note above */
		CHECK(strstr(text, "0 = flat\r\nDIALOGS3D=0\r\n\r\n") != 0);
		free(text);
	}
	while (nrepos)
		repo_delete(repos[0]);
	opt.check = 0;
	opt.token[0] = 0;
	store_init(dir);
	CHECK(nrepos == 2 && opt.check == CHECK_EVERY && opt.hours == 6 && opt.dialogs3d == 0);
	CHECK(!strcmp(opt.token, "ghp_secret;with stuff"));
	CHECK(!strcmp(repos[0]->folder, "C:\\NET\\EFTP") && !strcmp(repos[0]->files, "*.PRG,*.HYP"));
	CHECK(repos[0]->unzip == 1 && !strcmp(repos[1]->have, "v2.21"));
	CHECK(!strcmp(repos[0]->rule[0].names, "EFTPST.PRG") && !strcmp(repos[0]->rule[0].folder, "-"));
	CHECK(!strcmp(repos[0]->rule[1].folder, "C:\\") && !repos[0]->rule[2].names[0] && !repos[1]->rule[0].names[0]);
	while (nrepos)
		repo_delete(repos[0]);
	{
		/* an INF as 1.1 wrote it, with no empty lines, still reads */
		static const char old11[] =
			"; EUpdate settings\r\n\r\n[OPTIONS]\r\n; the hours between checks (1-168)\r\nHOURS=12\r\n"
			"FETCH=0\r\n\r\n; one [REPO] part per repository followed, up to 64\r\n[REPO]\r\n"
			"; owner/name on GitHub\r\nREPO=whomper/EMail\r\nFOLDER=C:\\NET\r\n"
			"RULE=*.ACC;C:\\\r\nHAVE=v3.10\r\nHAVEDATE=2026-10-01\r\n"
			"\r\n; one [REPO] part per repository followed, up to 64\r\n[REPO]\r\n"
			"REPO=whomper/EWeb\r\nHAVE=v0.27\r\n";
		char path[100];
		snprintf(path, sizeof(path), "%s/EUPDATE.INF", dir);
		CHECK(pf_save(path, old11, sizeof(old11) - 1) == 0);
		store_init(dir);
		CHECK(opt.hours == 12 && opt.fetch == 0 && nrepos == 2);
		CHECK(!strcmp(repos[0]->full, "whomper/EMail") && !strcmp(repos[0]->folder, "C:\\NET"));
		CHECK(!strcmp(repos[0]->rule[0].folder, "C:\\") && !strcmp(repos[0]->have_date, "2026-10-01"));
		CHECK(!strcmp(repos[1]->full, "whomper/EWeb") && !strcmp(repos[1]->have, "v0.27"));
		while (nrepos)
			repo_delete(repos[0]);
	}
	{
		/* DEFAULTS.INF beside EUPDATE.PRG: read only while there is no
		   EUPDATE.INF; "." is EUpdate's own folder */
		long n;
		char path[100], *text = load("DEFAULTS.INF", &n);
		snprintf(path, sizeof(path), "%s/DEFAULTS.INF", dir);
		CHECK(text && pf_save(path, text, n) == 0);
		free(text);
		store_init(dir);
		CHECK(nrepos == 2);		/* EUPDATE.INF is there: no defaults */
		while (nrepos)
			repo_delete(repos[0]);
		snprintf(path, sizeof(path), "%s/EUPDATE.INF", dir);
		pf_remove(path);
		store_init(dir);
		CHECK(nrepos == 8 && !strcmp(repos[0]->full, "whomper/ECal"));
		CHECK(!strcmp(repos[0]->folder, "C:\\AERA\\ECAL") && repos[0]->unzip == 1);
		CHECK(!strcmp(repos[0]->rule[0].names, "*.ACC") && !strcmp(repos[0]->rule[0].folder, "C:\\"));
		CHECK(!strcmp(repos[6]->full, "whomper/EWeb") && !strcmp(repos[6]->rule[0].folder, "-"));
		CHECK(!strcmp(repos[7]->full, "whomper/EUpdate") && !strcmp(repos[7]->folder, dir));
		CHECK(!strncmp(repos[7]->have, "v", 1) && !repos[2]->have[0] && !repos[2]->rule[0].names[0]);
		CHECK(store_save() == 0);	/* from now on EUPDATE.INF */
		while (nrepos)
			repo_delete(repos[0]);
		store_init(dir);
		CHECK(nrepos == 8 && !strcmp(repos[7]->folder, dir));
		while (nrepos)
			repo_delete(repos[0]);
	}
	rm_rf(dir);
}

int main(void)
{
	pf_init();
	test_json();
	test_http();
	test_gh();
	test_zip();
	test_tlsovl();
	test_store();
	printf("%d checks, %d failed\n", checks, fails);
	return fails != 0;
}
