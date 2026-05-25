# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0

CC = cc
#CFLAGS = -O3 -Wall -Wextra -Wpedantic -ansi -pedantic
CFLAGS = -O

all: lzpack

lzpack: lzpack.c cs8080.h
	$(CC) $(CFLAGS) -I. -o $@ lzpack.c

cs8080.h: s8080s.asm s8080d.asm mkstub
	./mkstub s8080s.asm s8080d.asm > $@

mkstub: mkstub.c
	$(CC) $(CFLAGS) -o $@ mkstub.c

strip:
	test -x mkstub && strip mkstub || :
	test -x mkstub && sstrip mkstub || :
	test -x lzpack && strip lzpack || :
	test -x lzpack && sstrip lzpack || :

clean:
	rm -f a.out lzpack mkstub
	rm -f lzpack.o mkstub.o
	rm -f lzpack.obj mkstub.obj
	rm -f lzpack.cmd mkstub.cmd
	rm -f lzpack.com mkstub.com
	rm -f lzpack.exe mkstub.exe

distclean: clean
	rm -f cs8080.h rm -f ./*.o ./*.obj ./*.cmd ./*.com ./*.exe
	rm -f compile_commands.json core core-*

stub: cs8080.h

.PHONY: all clean distclean stub
