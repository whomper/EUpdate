# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EUpdate - the latest releases of GitHub repositories, straight to the
# Atari Falcon and TT: find repositories and owners, follow them, see
# which are current, and update them by hand or on a schedule.
#
#   make            binary/EUPDATE.PRG (68030 code, TLS on the Atari with
#                   BearSSL, RSA and the curves on the Falcon's DSP) and the
#                   root certificates, with a stock m68k ELF cross compiler
#                   (Debian/Ubuntu: apt install gcc-m68k-linux-gnu). No
#                   MiNTLib: the program is freestanding, on common/'s
#                   runtime, and common/tools/elf2tos.py writes the header.
#   make test       the core on this computer, with AddressSanitizer and
#                   UBSan: JSON, HTTP (common/tests/fuzz/fuzz_net.c plays
#                   the server), zip, names, EUPDATE.INF; and the fuzz
#                   targets' seeds and finds
#   make itest      eupd-cli against tests/server.py, a GitHub of our own,
#                   over HTTP and TLS
#   make fuzz       the parsers under libFuzzer (clang), FUZZTIME s each
#   make help       binary/EUPDATE.HYP, the guide (HCP=/path/to/hcp)
#   make dist       a release's files in dist/, and EUPDATE-x.y.zip
#
# binary/ is what make builds, never committed: releases carry the
# program (.github/workflows/release.yml).
#
# The code EUpdate shares with the other AERA programs is in common/, a
# git subtree of github.com/whomper/AERA-Atari: change it there, then
# tools/sync-common.sh. Every file is built with -ffunction-sections and
# the program linked with --gc-sections, so of common/ only what EUpdate
# calls goes into EUPDATE.PRG.

CROSS   ?= m68k-linux-gnu-
CC      := $(CROSS)gcc
GCCINC  := $(shell $(CC) -print-file-name=include 2>/dev/null)
INC     := -Isrc -Icommon/src -Iatari -Icommon/atari -Iui -Icommon/ui -Icommon/third_party/bearssl/inc
CFLAGS  := -m68030 -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables \
           -ffunction-sections -fdata-sections -Wall -Wno-pointer-sign \
           -DEU_TOS -DTLS_030 -nostdinc -isystem $(GCCINC) -Icommon/atari/include $(INC)
# EUpdate's own files: no warning passes
OWNWARN := -Wextra -Wno-unused-parameter -Werror
LDFLAGS := -m68030 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments

# BearSSL 0.6, for TLS on the Atari (as EFTP, EMail and ECal)
BSSL    := common/third_party/bearssl
BSSLSRC := $(wildcard $(BSSL)/src/*/*.c)
BSSLDEF := -DBR_USE_URANDOM=0 -DBR_USE_GETENTROPY=0 -DBR_USE_UNIX_TIME=0 \
           -DBR_USE_WIN32_TIME=0 -DBR_USE_WIN32_RAND=0 -DBR_RDRAND=0
BSSLCF  := -m68030 -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables -ffunction-sections \
           -fdata-sections -w -nostdinc -isystem $(GCCINC) -Icommon/atari/include -I$(BSSL)/inc \
           -I$(BSSL)/src $(BSSLDEF)
# on the Atari, BearSSL's br_i31_montymul is common/tlsovl's, its loop in
# 68030 assembly (as TLS.OVL's: RSA, the prime curves, ECDSA, X25519),
# and RSA on the 68030 is rsa030.c with mont030.S (TLS_030; AERA-Atari
# d6a764d: a TT's chain check from 16.6 s to under 2 s)
ABSSLSRC := $(filter-out $(BSSL)/src/int/i31_montmul.c,$(BSSLSRC))
BSSLOBJ  = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/bssl/%.o,$(ABSSLSRC)) $(OBJDIR)/bssl/i31_montmul_030.o \
           $(OBJDIR)/bssl/mont31_030.o
HBSSLOBJ = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/hbssl/%.o,$(BSSLSRC))

# EUpdate's core (src/), and what it takes from common/src
OWN     := http json gh store unzip update
CORE    := conn tls util futil inflate secret charset ctime bidi ecdsp rsa030
ATARI   := libc tos plat_tos net_tos sting gem dsprsa dspec crash bigbuf
UI      := main win draw lists dialogs font about theme \
           dsp dsp_ec fontsel help av alert dialog field mbar aboutbox iconify clip clipedit dd popup \
           panebar notify aboutline

OBJDIR  := build
OBJS    := $(OBJDIR)/crt0.o $(OBJDIR)/sting_s.o $(OBJDIR)/nf.o $(OBJDIR)/crash_s.o $(OBJDIR)/mont030.o \
           $(addprefix $(OBJDIR)/,$(addsuffix .o,$(OWN) $(CORE) $(ATARI) $(UI)))
HDRS    := $(wildcard src/*.h atari/*.h ui/*.h common/src/*.h common/atari/*.h \
           common/atari/include/*.h common/ui/*.h common/dsp/*.h)

all: binary/EUPDATE.PRG binary/ROOTS.DAT

binary/EUPDATE.PRG: $(OBJDIR)/eupdate.elf common/tools/elf2tos.py | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@

# the version: VERSION holds the last release (major.minor), and the
# build never writes it; tools/bump.py starts a release
VERSION_STR := $(shell cat VERSION)

# written again whenever the string changes, so a new VERSION rebuilds it
$(OBJDIR)/version.c: FORCE | $(OBJDIR)
	@printf '/* written by the Makefile: do not edit */\nconst char app_version[] = "%s";\n' "$(VERSION_STR)" > $@.new
	@if cmp -s $@.new $@; then rm $@.new; else mv $@.new $@; fi
FORCE:
$(OBJDIR)/version.o: $(OBJDIR)/version.c
	$(CC) $(CFLAGS) -c -o $@ $<

# the desktop icon, the About box's and the README's, from the family's
# drawing (common/icons/EUPDATE.txt, the guidelines 5.5): committed
icons:
	python3 common/tools/appicon.py EUPDATE --rsc icons/EUPDATE.RSC --rscnv icons/EUPDNV.RSC \
		--icn icons/EUPDATE.ICN --mask icons/EUPDMK.ICN --headers ui --docs docs

$(OBJDIR)/eupdate.elf: $(OBJS) $(OBJDIR)/version.o $(OBJDIR)/libbearssl.a common/atari/link.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(OBJDIR)/version.o $(OBJDIR)/libbearssl.a -lgcc

$(OBJDIR)/bssl/%.o: $(BSSL)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(BSSLCF) -c -o $@ $<

$(OBJDIR)/bssl/i31_montmul_030.o: common/tlsovl/i31_montmul_030.c
	@mkdir -p $(dir $@)
	$(CC) $(BSSLCF) -c -o $@ $<
$(OBJDIR)/bssl/mont31_030.o: common/tlsovl/mont31_030.S
	@mkdir -p $(dir $@)
	$(CC) -m68030 -c -o $@ $<

$(OBJDIR)/libbearssl.a: $(BSSLOBJ)
	rm -f $@
	$(CROSS)ar rcs $@ $^

$(OBJDIR)/%.o: src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
$(OBJDIR)/%.o: atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
$(OBJDIR)/%.o: ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
$(OBJDIR)/%.o: common/src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/dspec.o: common/dsp/ec_prog.h
# the stack, 20 KB: a family update on Falcon-4:16 reaches 11 KB (TLS's
# handshake), the window and dialogs 3 KB. It is the program's own memory,
# taken before the VDI opens its workstation: NVDI with four accessories
# in 4 MB needs what the old 48 KB took, and the program stays smaller
# than 1.13, which passed there.
$(OBJDIR)/crt0.o: common/atari/crt0.S Makefile | $(OBJDIR)
	$(CC) -m68030 -DSTACK_SIZE=20480 -c -o $@ $<
$(OBJDIR)/sting_s.o: common/atari/sting.S | $(OBJDIR)
	$(CC) -m68030 -c -o $@ $<
$(OBJDIR)/crash_s.o: common/atari/crash.S | $(OBJDIR)
	$(CC) -m68030 -c -o $@ $<
$(OBJDIR)/nf.o: common/atari/nf.S | $(OBJDIR)
	$(CC) -m68030 -c -o $@ $<
$(OBJDIR)/mont030.o: common/atari/mont030.S | $(OBJDIR)
	$(CC) -m68030 -c -o $@ $<

$(OBJDIR) binary:
	mkdir -p $@

# the speed rig's build (tests/hatari/speed.py): each stretch between
# event calls of 20 ms or more on the emulator's console (-DAES_PROFILE,
# common/atari/gem.c), in build/prof/EUPDATE.PRG
ifdef PROF
CFLAGS += -DAES_PROFILE=$(PROF)
endif
prof:
	$(MAKE) OBJDIR=build/prof PROF=20 build/prof/EUPDATE.PRG
build/prof/EUPDATE.PRG: build/prof/eupdate.elf common/tools/elf2tos.py
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@

# ---- this computer: the same core, for the tests and eupd-cli ----
HOSTCC     ?= cc
HOSTCFLAGS := -O1 -g -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
              -Wno-format-truncation -Isrc -Icommon/src -I$(BSSL)/inc -fsanitize=address,undefined \
              -fno-sanitize-recover=undefined
HOSTSRC    := $(addprefix src/,$(addsuffix .c,$(OWN))) \
              $(addprefix common/src/,$(addsuffix .c,conn tls util futil inflate secret charset ctime)) \
              tests/host/plat_posix.c
HOSTLIB    := $(OBJDIR)/libbearssl-host.a

$(OBJDIR)/hbssl/%.o: $(BSSL)/src/%.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -O2 -w -I$(BSSL)/inc -I$(BSSL)/src -c -o $@ $<

$(HOSTLIB): $(HBSSLOBJ)
	rm -f $@
	ar rcs $@ $^

# the unit tests play the server with fuzz_net.c
$(OBJDIR)/unit: tests/unit.c $(HOSTSRC) common/tests/fuzz/fuzz_net.c $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -DFUZZ_NET -Icommon/tests/fuzz -o $@ tests/unit.c $(HOSTSRC) \
		common/tests/fuzz/fuzz_net.c $(HOSTLIB)

$(OBJDIR)/eupd-cli: tests/host/eupd_cli.c $(HOSTSRC) $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ tests/host/eupd_cli.c $(HOSTSRC) $(HOSTLIB)

$(OBJDIR)/mkroots: common/tools/mkroots.c $(HOSTSRC) $(HDRS) $(HOSTLIB) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ common/tools/mkroots.c $(HOSTSRC) $(HOSTLIB)

# the root certificates (Mozilla's list, kept in common/) next to the
# program, and decoded on this computer so the Atari needn't
binary/CACERT.PEM: common/certs/CACERT.PEM | binary
	cp $< $@
binary/ROOTS.DAT: binary/CACERT.PEM
	$(MAKE) $(OBJDIR)/mkroots
	$(OBJDIR)/mkroots binary/CACERT.PEM $@

# the help file for ST-Guide and EHypView, from docs/GUIDE.md; needs hcp
# (common/tools/build-hcp.sh builds it): make help HCP=/path/to/hcp. The
# front page (the guidelines, 4.3) is common/tools/helpfront.py's.
HCP     ?= hcp
help: binary/EUPDATE.HYP
binary/EUPDATE.HYP: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
		common/help/EUPDATE.IMG common/tools/helpfront.py | $(OBJDIR) binary
	python3 tools/mkhelp.py docs/GUIDE.md VERSION $(OBJDIR)/EUPDATE.STG
	python3 common/tools/helpfront.py page docs/front.json $(OBJDIR)/EUPDATE.STG $(OBJDIR)/EUPDATE.STG
	$(HCP) -o $@ $(OBJDIR)/EUPDATE.STG

# ---- fuzzing (the guidelines, 7.1): what GitHub sends, from libFuzzer,
# with AddressSanitizer and UBSan; the network is fuzz_net.c. Needs clang
# with libFuzzer. make fuzz runs each target FUZZTIME seconds from
# tests/fuzz/seeds (tests/fuzz/mkseeds.py)
FUZZCC    ?= clang
FUZZTIME  ?= 60
FUZZERS   := http json zip
FUZZFLAGS := -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -DFUZZ_NET \
             -Isrc -Icommon/src -Icommon/tests/fuzz -I$(BSSL)/inc

$(OBJDIR)/fuzz_%: tests/fuzz/fuzz_%.c $(HOSTSRC) $(HDRS) $(HOSTLIB) common/tests/fuzz/fuzz_net.c | $(OBJDIR)
	$(FUZZCC) $(FUZZFLAGS) -o $@ $< $(HOSTSRC) common/tests/fuzz/fuzz_net.c $(HOSTLIB)

fuzz: $(addprefix $(OBJDIR)/fuzz_,$(FUZZERS))
	@for f in $(FUZZERS); do \
	  mkdir -p $(OBJDIR)/corpus/$$f; \
	  echo "fuzz_$$f: $(FUZZTIME) s"; \
	  $(OBJDIR)/fuzz_$$f -max_total_time=$(FUZZTIME) -timeout=10 -rss_limit_mb=2048 \
	    -artifact_prefix=$(OBJDIR)/crash-$$f- $(OBJDIR)/corpus/$$f tests/fuzz/seeds/$$f \
	    $$(ls -d tests/fuzz/found/$$f 2>/dev/null) \
	    > $(OBJDIR)/fuzz_$$f.log 2>&1 || { tail -40 $(OBJDIR)/fuzz_$$f.log; exit 1; }; \
	  grep -E "^#[0-9]+.*DONE" $(OBJDIR)/fuzz_$$f.log | tail -1; \
	done

# the same targets without libFuzzer (common/tests/fuzz/replay.c): make
# test runs the seeds again, and what once broke a parser
REPLAYSRC := common/tests/fuzz/fuzz_net.c common/tests/fuzz/replay.c

$(OBJDIR)/replay_%: tests/fuzz/fuzz_%.c $(HOSTSRC) $(HDRS) $(HOSTLIB) $(REPLAYSRC) | $(OBJDIR)
	$(HOSTCC) $(HOSTCFLAGS) -DFUZZ_NET -Icommon/tests/fuzz -o $@ $< $(HOSTSRC) $(REPLAYSRC) $(HOSTLIB)

test: $(OBJDIR)/unit $(addprefix $(OBJDIR)/replay_,$(FUZZERS))
	$(OBJDIR)/unit
	@for f in $(FUZZERS); do \
	  $(OBJDIR)/replay_$$f tests/fuzz/seeds/$$f $$(ls -d tests/fuzz/found/$$f 2>/dev/null) || exit 1; \
	done

itest: $(OBJDIR)/eupd-cli
	python3 tests/integration.py $(OBJDIR)/eupd-cli

# a release's files: each one in dist/, and all of them in
# dist/EUPDATE-x.y.zip (the icons in icons/), for the release workflow
DIST    := binary/EUPDATE.PRG binary/CACERT.PEM binary/ROOTS.DAT binary/EUPDATE.HYP binary/DEFAULTS.INF
ICONS   := icons/EUPDATE.RSC icons/EUPDNV.RSC icons/EUPDATE.ICN icons/EUPDMK.ICN
dist: all help binary/DEFAULTS.INF
	rm -rf dist
	mkdir -p dist/zip/icons
	cp $(DIST) $(ICONS) dist/
	cp $(DIST) dist/zip/
	cp $(ICONS) dist/zip/icons/
	cd dist/zip && zip -q -9 -r ../EUPDATE-$(VERSION_STR).zip .
	rm -rf dist/zip

# what a first start follows (the AERA programs), EUpdate itself at this version
binary/DEFAULTS.INF: DEFAULTS.INF VERSION | binary
	sed 's/@VERSION@/$(VERSION_STR)/' DEFAULTS.INF > $@

clean:
	rm -rf $(OBJDIR) binary dist

.PHONY: all help icons dist test itest clean fuzz prof FORCE
