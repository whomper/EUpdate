# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# EUpdate - the latest releases of GitHub repositories, straight to the
# Atari Falcon and TT: find repositories and owners, follow them, see
# which are current, and update them by hand or on a schedule.
#
#   make            binary/eupdate.prg (68030 code), TLS.OVL beside it (its
#                   TLS: BearSSL, RSA and the curves on the Falcon's DSP;
#                   the family's, the guidelines 7.4), TLSFB.OVL (its
#                   ColdFire build, for a FireBee) and the root
#                   certificates, with a stock m68k ELF cross compiler
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
#   make help       binary/eupdate.hyp, the guide (HCP=/path/to/hcp)
#   make dist       a release's files in dist/, and eupdate-x.y.zip
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
           -DEU_TOS -nostdinc -isystem $(GCCINC) -Icommon/atari/include $(INC)
# EUpdate's own files: no warning passes
OWNWARN := -Wextra -Wno-unused-parameter -Werror
LDFLAGS := -m68030 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments

# BearSSL 0.6, for the tests on this computer: on the Atari the TLS is
# TLS.OVL's (common/tlsovl, built in AERA-Atari, never here)
BSSL    := common/third_party/bearssl
BSSLSRC := $(wildcard $(BSSL)/src/*/*.c)
HBSSLOBJ = $(patsubst $(BSSL)/src/%.c,$(OBJDIR)/hbssl/%.o,$(BSSLSRC))

# EUpdate's core (src/), and what it takes from common/src
OWN     := http json gh store unzip update
CORE    := conn util futil inflate secret charset ctime bidi ovl
ATARI   := libc tos plat_tos net_tos sting gem tls_ovl crash bigbuf
UI      := main win draw lists dialogs font about theme \
           dsp_ovl fontsel help av alert dialog field mbar aboutbox iconify clip clipedit dd popup \
           panebar notify aboutline cpucheck hebkey text arrowed tooltip

OBJDIR  := build
# the objects where GCC's -m68030 divides by a constant with a 64-bit
# mulu.l, which a 68060 and the FireBee's ColdFire lack (17 Nov 2026: 2.08
# stopped at its first log line on the FireBee stand-in, exception 61,
# before it could load TLSFB.OVL): tuned for the 68020-60, still 68030
# code without it. Only these, so the rest keeps -m68030's code (the
# whole program tuned cost the window's redraw a tick); tools/no64.py
# fails the link where one is left, naming its function
TUNE64  := text unzip conn ctime libc win fontsel panebar aboutline
$(addprefix $(OBJDIR)/,$(addsuffix .o,$(TUNE64))): CFLAGS += -mtune=68020-60
# the link order (the guidelines' Speed, 13 Nov 2026): the main flows'
# hot loops first, after the start's assembly, so they sit together in
# memory (link.ld keeps input order): unzip's CRC and inflate's piece loop
HOT     := unzip inflate
OBJS    := $(OBJDIR)/crt0.o $(OBJDIR)/sting_s.o $(OBJDIR)/nf.o $(OBJDIR)/crash_s.o $(OBJDIR)/inflate_fast.o \
           $(addprefix $(OBJDIR)/,$(addsuffix .o,$(HOT) $(filter-out $(HOT),$(OWN) $(CORE)) $(ATARI) $(UI)))
# the pads (tools/objpad.py): each section at 2.07's place mod the 68030's
# 256-byte caches, a pad object before each object it moves (17 Nov 2026,
# 2.08); atari/pads.txt made by "objpad.py step" against v2.07's map, the
# map beside the program to recheck them with, PADS=no to link without
# them
PADFILE ?= atari/pads.txt
PADAT   := $(if $(PADS),,$(shell grep -v '^\#' $(PADFILE) 2>/dev/null | awk 'NF == 3 { print $$1 }' | sort -u))
PADLD   := $(if $(PADS),,$(shell grep -v '^\#' $(PADFILE) 2>/dev/null | awk 'NF == 3 { print "-Wl,-u,eu_pad_" $$1 "_" $$2 }'))
OBJS0   := $(OBJS)
OBJS    := $(foreach o,$(OBJS0),$(if $(filter $(basename $(notdir $(o))),$(PADAT)),$(OBJDIR)/pad_$(notdir $(o))) $(o))
HDRS    := $(wildcard src/*.h atari/*.h ui/*.h common/src/*.h common/atari/*.h \
           common/atari/include/*.h common/ui/*.h common/dsp/*.h)

all: binary/eupdate.prg binary/eupdfb.prg binary/tls.ovl binary/tlsfb.ovl binary/roots.dat

binary/eupdate.prg: $(OBJDIR)/eupdate.elf common/tools/elf2tos.py | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
# EUPDFB.PRG, the FireBee's (ColdFire, -mcpu=5475; 18 Nov 2026): the
# same rules again as ColdFire code (common's tools/mkfirebee.py, once
# EUpdate's own), into build/cf/; EUPDATE.PRG on a FireBee names it
# (cpu_cf_alert), it on any other machine names EUPDATE.PRG (cpu_alert,
# common's cpucheck_cf.S)
binary/eupdfb.prg: binary/eupdate.prg common/tools/mkfirebee.py common/atari/cpucheck_cf.S common/atari/ll64.c | binary
	python3 common/tools/mkfirebee.py --elf build/eupdate.elf --make PADS=no $@
# the family's TLS.OVL, as it comes with common/: copied, never built here
binary/tls.ovl: common/tlsovl/tls.ovl | binary
	cp $< $@
# and its ColdFire build, which tls_ovl.c loads on a FireBee (the _CF_
# cookie; the guidelines' "TLSFB.OVL for the FireBee", 15 and 17 Nov
# 2026): copied too, from the same common
binary/tlsfb.ovl: common/tlsovl/tlsfb.ovl | binary
	cp $< $@

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
	python3 common/tools/appicon.py EUPDATE --rsc icons/eupdate.rsc --rscnv icons/eupdnv.rsc \
		--icn icons/eupdate.icn --mask icons/eupdmk.icn --headers ui --docs docs

ifdef PROF
# the timed build, at the shipped build's places mod 256 (common's
# profpad.py, the hub 17 Nov 2026): gem.o and tos.o hold the timing code
$(OBJDIR)/eupdate.elf: $(OBJS) $(OBJDIR)/version.o common/atari/link.ld build/eupdate.elf \
		common/tools/profpad.py common/atari/codepad.S
	python3 common/tools/profpad.py --ship build/eupdate.elf --out $@ \
	    --cpu=-m68030 --dir $(OBJDIR)/pads \
	    $(foreach o,gem tos,--exclude build/$(o).o --exclude $(OBJDIR)/$(o).o) \
	    -- $(CC) $(LDFLAGS) $(PADLD) $(OBJS) $(OBJDIR)/version.o -lgcc
else
$(OBJDIR)/eupdate.elf: $(OBJS) $(OBJDIR)/version.o common/atari/link.ld $(if $(PADS),,$(wildcard $(PADFILE)))
	$(CC) $(LDFLAGS) $(PADLD) -Wl,-Map,$@.map -o $@ $(OBJS) $(OBJDIR)/version.o -lgcc
	python3 tools/no64.py $@ || { rm -f $@; exit 1; }
endif
# without the pads, what objpad.py measures them on
$(OBJDIR)/nopad.elf: $(OBJS0) $(OBJDIR)/version.o common/atari/link.ld
	$(CC) $(LDFLAGS) -Wl,-Map,$@.map -o $@ $(OBJS0) $(OBJDIR)/version.o -lgcc
$(OBJDIR)/pad_%.o: $(wildcard $(PADFILE)) tools/objpad.py | $(OBJDIR)
	python3 tools/objpad.py asm $(PADFILE) $* > $(OBJDIR)/pad_$*.S
	$(CC) $(CFLAGS) -c -o $@ $(OBJDIR)/pad_$*.S

$(OBJDIR)/%.o: src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
$(OBJDIR)/%.o: atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
$(OBJDIR)/%.o: ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(OWNWARN) -c -o $@ $<
# the first code, built for the 68000: on an ST it says EUpdate needs a
# Falcon or TT, before anything of the 68030's runs (common's cpucheck.c)
$(OBJDIR)/cpucheck.o: common/atari/cpucheck.c | $(OBJDIR)
	$(CC) $(subst -m68030,-m68000,$(CFLAGS)) -c -o $@ $<
$(OBJDIR)/%.o: common/src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/atari/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJDIR)/%.o: common/ui/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<
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
# the unzip's hot loop in assembly (common's inflate_piece, INFLATE_ASM):
# programs unpack some 1.8 times as fast on a stock Falcon (1 Nov
# 2026: saving the family took too long); the host's tests keep the C
$(OBJDIR)/inflate_fast.o: common/atari/inflate_fast.S | $(OBJDIR)
	$(CC) -m68030 -c -o $@ $<
$(OBJDIR)/inflate.o: common/src/inflate.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -DINFLATE_ASM -c -o $@ $<

$(OBJDIR) binary:
	mkdir -p $@

# the speed rig's build (tests/hatari/speed.py): each stretch between
# event calls of 20 ms or more on the emulator's console (-DAES_PROFILE,
# common/atari/gem.c), in build/prof/eupdate.prg
ifdef PROF
CFLAGS += -DAES_PROFILE=$(PROF)
endif
# the FireBee stand-in's build (tests/hatari/family_test.py WORK FireBee:16):
# EUPDATE.PRG without its FireBee alert (cpu_cf_alert), so that its
# installer runs where the _CF_ cookie is; the stand-in, a 68060, runs no
# ColdFire code, so EUPDFB.PRG itself is the tester's
ifdef CFTEST
CFLAGS += -DEU_CF_TEST
endif
cftest: build/eupdate.elf
	$(MAKE) OBJDIR=build/cft CFTEST=1 build/cft/eupdate.prg
build/cft/eupdate.prg: build/cft/eupdate.elf common/tools/elf2tos.py
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
# the speed rig's build (its link pads it to build/'s places, above)
prof: build/eupdate.elf
	$(MAKE) OBJDIR=build/prof PROF=20 build/prof/eupdate.prg
build/prof/eupdate.prg: build/prof/eupdate.elf common/tools/elf2tos.py
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
binary/cacert.pem: common/certs/CACERT.PEM | binary
	cp $< $@
binary/roots.dat: binary/cacert.pem
	$(MAKE) $(OBJDIR)/mkroots
	$(OBJDIR)/mkroots binary/cacert.pem $@

# the help file for ST-Guide and EHypView, from docs/GUIDE.md; needs hcp
# (common/tools/build-hcp.sh builds it): make help HCP=/path/to/hcp. The
# front page (the guidelines, 4.3) is common/tools/helpfront.py's.
HCP     ?= hcp
help: binary/eupdate.hyp
binary/eupdate.hyp: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
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

# a release (the guidelines 7.4, 22 Nov 2026: "Remove all files from
# root and just have two ZIP files, one for each platform"): two zips in
# dist/, every file and folder in them in lowercase (the guidelines 6.6:
# MiNT's ext2 tells the cases apart), the icons in icons/:
# - eupdate-x.y.zip, the Falcon's and the TT's: EUPDATE.PRG and TLS.OVL
#   (2.15's carried EUPDFB.PRG and TLSFB.OVL too, the bridge, decided 22 Nov
#   2026; 2.15 on a FireBee takes the FireBee's zip alone);
# - eupdate-x.y-firebee.zip, the FireBee's: EUPDATE.PRG the ColdFire build,
#   and TLS.OVL, TLS.OVL's ColdFire build (common's ColdFire build loads
#   tls.ovl, AERA-Atari c184d49)
DIST    := binary/cacert.pem binary/roots.dat binary/eupdate.hyp binary/defaults.inf
ICONS   := icons/eupdate.rsc icons/eupdnv.rsc icons/eupdate.icn icons/eupdmk.icn
dist: all help binary/defaults.inf
	rm -rf dist
	mkdir -p dist/zip/icons
	cp $(DIST) binary/eupdate.prg binary/tls.ovl dist/zip/
	cp $(ICONS) dist/zip/icons/
	cd dist/zip && zip -q -9 -r ../eupdate-$(VERSION_STR).zip .
	rm -rf dist/zip
	mkdir -p dist/zip/icons
	cp $(DIST) dist/zip/
	cp binary/eupdfb.prg dist/zip/eupdate.prg
	cp binary/tlsfb.ovl dist/zip/tls.ovl
	cp $(ICONS) dist/zip/icons/
	cd dist/zip && zip -q -9 -r ../eupdate-$(VERSION_STR)-firebee.zip .
	rm -rf dist/zip

# what a first start follows (the AERA programs), EUpdate itself at this version
binary/defaults.inf: defaults.inf VERSION | binary
	sed 's/@VERSION@/$(VERSION_STR)/' defaults.inf > $@

clean:
	rm -rf $(OBJDIR) binary dist

.PHONY: all help icons dist test itest clean fuzz prof cftest FORCE
