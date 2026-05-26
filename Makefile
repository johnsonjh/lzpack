# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: afec1e70-585c-11f1-aaeb-80ee73e9b8e7

CC = cc
CFLAGS = -O

all: lzpack

build: all

lzpack: lzpack.c cs8080.h
	$(CC) $(CFLAGS) -I. -o $@ lzpack.c

cs8080.h: s8080s.asm s8080d.asm stubasm
	./stubasm s8080s.asm s8080d.asm > $@

stubasm: stubasm.c
	$(CC) $(CFLAGS) -o $@ stubasm.c

strip:
	test -x stubasm 2> /dev/null && strip stubasm || :
	test -x stubasm 2> /dev/null && sstrip stubasm 2> /dev/null || :
	test -x lzpack 2> /dev/null && strip lzpack || :
	test -x lzpack 2> /dev/null && sstrip lzpack 2> /dev/null || :

clean:
	rm -f a.out lzpack stubasm
	rm -f lzpack.o stubasm.o
	rm -f lzpack.obj stubasm.obj
	rm -f lzpack.cmd stubasm.cmd
	rm -f lzpack.com stubasm.com
	rm -f lzpack.exe stubasm.exe

distclean: clean
	rm -f cs8080.h ./*.o ./*.obj ./*.cmd ./*.com ./*.exe ./*.map ./*.t
	rm -f compile_commands.json compile_commands.events.json log.pvs
	rm -f ./*.pop core core-*
	rm -f -r ./pvsreport 2> /dev/null
	rm -f -r ./cpm-8080 2> /dev/null
	rm -f -r ./cpm-z80 2> /dev/null
	test -d ./.git 2> /dev/null && git clean -ndx 2> /dev/null || :

stub: cs8080.h

cpm: cs8080.h
	@./.build-cpm.sh

cpm86 cpm-86: cs8080.h
	@rm -f ./lzpack.o ./lzpack.cmd ./stubasm.o ./stubasm.cmd
	aztec42_cc -B "+CA" -L19 -Z450 stubasm.c
	aztec42_sqz stubasm.o
	aztec42_link -o stubasm.cmd stubasm.o -lc86
	test -f stubasm.cmd
	pcdev_cmdinfo stubasm.cmd
	aztec42_cc -B "+CA" -L19 -Z814 -D__AZTEC_C_42T__=1 -DPOPCOM_STREAM=1 -DHSZ=1024 -DMZXFILE=65535L lzpack.c
	aztec42_sqz lzpack.o
	aztec42_link -o lzpack.cmd lzpack.o -lc86
	test -f lzpack.cmd
	pcdev_cmdinfo lzpack.cmd

cpm-test test-cpm:
	@printf '\n%s\n' "==========================================="
	@$$(command -v python3) ./tests/harness.py cpm

lint:
	@./.lint.sh

test: lzpack
	@./tests/run.sh

.PHONY: all clean distclean stub cpm cpm-test lint test cpm86 cpm-86
