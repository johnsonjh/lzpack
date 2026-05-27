# LZPACK - Makefile
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: afec1e70-585c-11f1-aaeb-80ee73e9b8e7

################################################################################

CC = cc
CFLAGS = -O

################################################################################

all: lzpack

################################################################################

build: all

################################################################################

lzpack: lzpack.c cs8080.h cz80.h
	$(CC) $(CFLAGS) -I. -o $@ lzpack.c

################################################################################

cs8080.h: s8080s.asm s8080d.asm stubasm
	./stubasm s8080s.asm s8080d.asm > $@

################################################################################

cz80.h: sz80s.asm sz80d.asm stubasm
	./stubasm -z80 sz80s.asm sz80d.asm > $@

################################################################################

stubasm: stubasm.c
	$(CC) $(CFLAGS) -o $@ stubasm.c

################################################################################

strip:
	test -x stubasm 2> /dev/null && strip stubasm || :
	test -x stubasm 2> /dev/null && sstrip stubasm 2> /dev/null || :
	test -x lzpack 2> /dev/null && strip lzpack || :
	test -x lzpack 2> /dev/null && sstrip lzpack 2> /dev/null || :

################################################################################

clean:
	rm -f lzpack stubasm lzpack.o stubasm.o lzpack.exe stubasm.exe

################################################################################

distclean: clean
	rm -f cs8080.h cz80.h ./*.o ./*.obj ./*.cmd ./*.com ./*.exe ./*.map
	rm -f compile_commands.json compile_commands.events.json log.pvs
	rm -f ./*.pop ./*.unp ./*.t a.out a.exe core core-*
	rm -f -r ./pvsreport 2> /dev/null
	rm -f -r ./cpm-8080 2> /dev/null
	rm -f -r ./cpm-z80 2> /dev/null
	rm -f -r ./cpm-86 2> /dev/null
	test -d ./.git 2> /dev/null && git clean -ndx 2> /dev/null || :

################################################################################

stub: cs8080.h cz80.h

################################################################################

cpm cpm80 cpm-auto cpm80-auto: cs8080.h cz80.h
	@env CPM_BACKEND="auto" ./.build-cpm.sh

################################################################################

cpm-local cpm80-local: cs8080.h cz80.h
	@env CPM_BACKEND="local" ./.build-cpm.sh

################################################################################

cpm-docker cpm80-docker: cs8080.h cz80.h
	@env CPM_BACKEND="docker" ./.build-cpm.sh

################################################################################

cpm86 cpm-86: cs8080.h cz80.h
	@mkdir -p ./cpm-86/
	@(cd cpm-86 && rm -f ./lzpack.o ./lzpack.cmd ./stubasm.o ./stubasm.cmd)
	aztec42_cc -B "+CA" -L19 -Z450 -D__AZTEC_C_42T__=1 \
		-DMAXSYM=96 -DMAXREF=96 -DMAXCODE=768 stubasm.c \
		-o cpm-86/stubasm.o
	aztec42_sqz cpm-86/stubasm.o
	aztec42_link -o cpm-86/stubasm.cmd cpm-86/stubasm.o -lc86
	@pcdev_cmdinfo cpm-86/stubasm.cmd
	aztec42_cc -I. -B "+CA" -L19 -Z814 -D__AZTEC_C_42T__=1 \
		-DPOPCOM_STREAM=1 -DHSZ=1024 -DMZXFILE=65535L lzpack.c \
		-o cpm-86/lzpack.o
	aztec42_sqz cpm-86/lzpack.o
	aztec42_link -o cpm-86/lzpack.cmd cpm-86/lzpack.o -lc86
	@pcdev_cmdinfo cpm-86/lzpack.cmd

################################################################################

lint:
	@./.lint.sh

################################################################################

test: lzpack
	@./tests/run.sh

################################################################################

.PHONY: all build clean distclean stub strip cpm cpm80 cpm80-auto cpm-auto \
	cpm-local cpm80-local cpm-docker cpm80-docler lint test cpm86 cpm-86

################################################################################

.NOTPARALLEL:

################################################################################

# Local Variables:
# mode: makefile
# indent-tabs-mode: t
# tab-width: 8
# whitespace-style: (tabs tab-mark)
# whitespace-display-mappings: ((tab-mark 9 [45] [45]))
# fill-column: 80
# eval: (setq-local whitespace-display-mappings
#                   '((tab-mark 9
#                               [45 45 45 45 45 45 62]
#                               [45 45 45 45 45 45 62])))
# eval: (whitespace-mode 1)
# eval: (setq-local display-fill-column-indicator-column 80)
# eval: (display-fill-column-indicator-mode 1)
# End:

################################################################################
# vim: set ft=make ts=8 ai noexpandtab list listchars=tab\:\>\- cc=80 :
################################################################################
