CC = cc
CFLAGS = -O3 -Wall -Wextra -Wpedantic -ansi -pedantic

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
	rm -f lzpack mkstub
	rm -f lzpack.o mkstub.o
	rm -f lzpack.obj mkstub.obj
	rm -f lzpack.cmd mkstub.cmd
	rm -f lzpack.com mkstub.com
	rm -f lzpack.exe mkstub.exe

distclean: clean
	rm -f cs8080.h rm -f ./*.o ./*.obj ./*.cmd ./*.com ./*.exe ./*.bak

stub: cs8080.h

.PHONY: all clean distclean stub
