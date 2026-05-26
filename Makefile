# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: afec1e70-585c-11f1-aaeb-80ee73e9b8e7

CC = cc
#CFLAGS = -O3 -Wall -Wextra -Wpedantic -ansi -pedantic
CFLAGS = -O

all: lzpack

lzpack: lzpack.c cs8080.h
	$(CC) $(CFLAGS) -I. -o $@ lzpack.c

cs8080.h: s8080s.asm s8080d.asm stubasm
	./stubasm s8080s.asm s8080d.asm > $@

stubasm: stubasm.c
	$(CC) $(CFLAGS) -o $@ stubasm.c

strip:
	test -x stubasm && strip stubasm || :
	test -x stubasm && sstrip stubasm || :
	test -x lzpack && strip lzpack || :
	test -x lzpack && sstrip lzpack || :

clean:
	rm -f a.out lzpack stubasm
	rm -f lzpack.o stubasm.o
	rm -f lzpack.obj stubasm.obj
	rm -f lzpack.cmd stubasm.cmd
	rm -f lzpack.com stubasm.com
	rm -f lzpack.exe stubasm.exe

distclean: clean
	rm -f cs8080.h ./*.o ./*.obj ./*.cmd ./*.com ./*.exe
	rm -f compile_commands.json log.pvs core core-*
	rm -f -r ./pvsreport > /dev/null 2>&1

stub: cs8080.h

.PHONY: all clean distclean stub
