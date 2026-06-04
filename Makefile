# LZPACK - Makefile
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: afec1e70-585c-11f1-aaeb-80ee73e9b8e7

################################################################################

# Compatible defaults.

CC="$$(command -v cc 2> /dev/null || command -v gcc 2> /dev/null || \
	command -v clang 2> /dev/null || echo cc)"
CFLAGS?=-O

################################################################################

# Builds the native LZPACK binary.

all: lzpack

################################################################################

# Builds the native LZPACK binary.

build: all

################################################################################

# Builds the native LZPACK binary.

lzpack: lzpack.c cs8080.h csz80.h csr8080.h csrz80.h cschk.h lzpack.c
	@eval echo \
		"$${CC-$(CC)}" $(CFLAGS) -I. -o $@ lzpack.c 2> /dev/null || :
	@eval \
		"$${CC-$(CC)}" $(CFLAGS) -I. -o $@ lzpack.c

################################################################################

# Builds the 8080 decompression stub from source code using the stub assembler.

cs8080.h: s8080s.asm s8080d.asm stubasm
	./stubasm s8080s.asm s8080d.asm > $@
	@command -v "$${AWK:-awk}" > /dev/null 2>&1 && { "$${AWK:-awk}" \
	'/S8_.*LEN/ { a[++i]=$$4; t+=$$4 } END { printf \
	"[8080] %d bytes (setup) + %d bytes (stub) == %d total bytes.\n", \
	a[1], a[2], t }' $@ 2> /dev/null || :; } || :

################################################################################

# Builds the CALL-able 8080 decompressor reused by lzpack's in-RAM -R restore.

csr8080.h: s8080r.asm stubasm
	./stubasm -r s8080r.asm > $@
	@command -v "$${AWK:-awk}" > /dev/null 2>&1 && { "$${AWK:-awk}" \
	'/S8R_DLEN/ { printf "[8080] "$$4" total bytes (reloc).\n" }' \
	$@ 2> /dev/null || :; } || :

################################################################################

# Builds the Z80 decompression stub from source code using the stub assembler.

csz80.h: sz80s.asm sz80d.asm stubasm
	./stubasm -z80 sz80s.asm sz80d.asm > $@
	@command -v "$${AWK:-awk}" > /dev/null 2>&1 && { "$${AWK:-awk}" \
	'/Z80.*_LEN/ { a[++i]=$$4; t+=$$4 } END { printf \
	"[Z80] %d bytes (setup) + %d bytes (stub) == %d total bytes.\n", \
	a[1], a[2], t }' $@ 2> /dev/null || :; } || :

################################################################################

# Builds the CALL-able Z80 decompressor reused by lzpack's in-RAM -R restore.

csrz80.h: sz80r.asm stubasm
	./stubasm -rz80 sz80r.asm > $@
	@command -v "$${AWK:-awk}" > /dev/null 2>&1 && { "$${AWK:-awk}" \
	'/SRZ_DLEN/ { printf "[Z80] "$$4" total bytes (reloc).\n" }' \
	$@ 2> /dev/null || :; } || :

################################################################################

# Builds the optional (-C) runtime memory-check block from source code using
# the stub assembler.

cschk.h: chk.asm stubasm
	./stubasm -chk chk.asm > $@
	@command -v "$${AWK:-awk}" > /dev/null 2>&1 && { "$${AWK:-awk}" \
	'/CHK_LEN/ { printf "[8080] "$$4" total bytes (check).\n" }' \
	$@ 2> /dev/null || :; } || :

################################################################################

# Builds the stub assembler.

stubasm: stubasm.c
	@eval echo \
		"$${CC-$(CC)}" $(CFLAGS) -o $@ stubasm.c 2> /dev/null || :
	@eval \
		"$${CC-$(CC)}" $(CFLAGS) -o $@ stubasm.c

################################################################################

# Strips native binaries.

strip:
	test -x stubasm 2> /dev/null && strip stubasm || :
	test -x stubasm 2> /dev/null && sstrip stubasm 2> /dev/null || :
	test -x lzpack 2> /dev/null && strip lzpack || :
	test -x lzpack 2> /dev/null && sstrip lzpack 2> /dev/null || :

################################################################################

# Clean up artifacts from the local native build.

clean:
	rm -f lzpack stubasm lzpack.o stubasm.o lzpack.exe stubasm.exe

################################################################################

# Clean up all the builds and assorted build, testing, and temporary detritus.

distclean reallyclean: clean
	rm -f cs8080.h csz80.h csr8080.h csrz80.h cschk.h
	rm -f ./*.o ./*.obj ./*.cmd ./*.com ./*.exe ./*.map
	rm -f compile_commands.json compile_commands.events.json log.pvs
	rm -f ./*.pop ./*.unp ./*.t a.out a.exe core core-*
	rm -f tags cscope.out GPATH GRTAGS GTAGS TAGS
	rm -f -r ./pvsreport 2> /dev/null
	rm -f -r ./cpm-8080 2> /dev/null
	rm -f -r ./cpm-z80 2> /dev/null
	rm -f -r ./cpm-86 2> /dev/null
	rm -f -r ./windows 2> /dev/null
	rm -f -r ./msdos 2> /dev/null
	rm -f -r ./djgpp 2> /dev/null
	rm -f -r ./elks 2> /dev/null
	test -d ./.git 2> /dev/null && git clean -ndx 2> /dev/null || :

################################################################################

# Builds both decompression stubs from the assembly source code.

stub stubs: cs8080.h csz80.h csr8080.h csrz80.h cschk.h stubasm.c

################################################################################

# CP/M-80 builds (Z80 and 8080) using the z88dk development kit.
# https://z88dk.org/

cpm cpm80 cpm-auto cpm80-auto: cs8080.h csz80.h csr8080.h csrz80.h cschk.h \
		stubasm.c lzpack.c .build-cpm.sh .common.sh
	@env CPM_BACKEND="auto" ./.build-cpm.sh

################################################################################

# Local CP/M-80 builds (Z80 and 8080) using the z88dk development kit.
# https://github.com/z88dk/z88dk

cpm-local cpm80-local: cs8080.h csz80.h csr8080.h csrz80.h cschk.h \
		stubasm.c lzpack.c .build-cpm.sh .common.sh
	@env CPM_BACKEND="local" ./.build-cpm.sh

################################################################################

# Dockerized CP/M-80 builds (Z80 and 8080) using the z88dk development kit.
# https://hub.docker.com/r/z88dk/z88dk

cpm-docker cpm80-docker: cs8080.h csz80.h csr8080.h csrz80.h cschk.h \
		stubasm.c lzpack.c .build-cpm.sh .common.sh
	@env CPM_BACKEND="docker" ./.build-cpm.sh

################################################################################

# CP/M-86 build using the tsupplis Aztec C v4.2 CP/M-86 cross-toolchain.
# https://github.com/tsupplis/cpm86-crossdev

cpm86 cpm-86: cs8080.h csz80.h cschk.h stubasm.c lzpack.c lz86body.asm \
	.lz86gen.sh
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command upx aztec42_cc aztec42_sqz aztec42_link \
		pcdev_cmdinfo)
	@mkdir -p ./cpm-86/
	@(cd cpm-86 && rm -f ./lzpack.o ./lzpack.cmd ./stubasm.o ./stubasm.cmd \
		./lz86.c ./lz86.o)
	aztec42_cc -B "+CA" -D__AZTEC_C_42T__=1 \
		-DMAXSYM=96 -DMAXREF=96 -DMAXCODE=768 ./stubasm.c \
		-o ./cpm-86/stubasm.o
	aztec42_sqz ./cpm-86/stubasm.o
	aztec42_link -t -o ./cpm-86/stubasm.cmd \
		./cpm-86/stubasm.o -lc86
	@pcdev_cmdinfo ./cpm-86/stubasm.cmd
	(upx -q -9 --8086 ./cpm-86/stubasm.cmd 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :
	sh ./.lz86gen.sh aztec ./lz86body.asm > ./cpm-86/lz86.c
	aztec42_cc -B "+CA" -D__AZTEC_C_42T__=1 ./cpm-86/lz86.c \
		-o ./cpm-86/lz86.o
	aztec42_cc -I. -B "+CA" -D__AZTEC_C_42T__=1 \
		-DLZPACK_STREAM=1 -DLZPACK_OPT=1 -DHSZ=1024 -DMZXFILE=65535L \
		./lzpack.c -o ./cpm-86/lzpack.o
	aztec42_sqz ./cpm-86/lzpack.o
	# +D reserves data-segment headroom for the run-time stack: the optimal
	# parser recurses deeper than the greedy path and overflows the default
	aztec42_link -V +D 12288 -t -o ./cpm-86/lzpack.cmd \
		./cpm-86/lzpack.o ./cpm-86/lz86.o -lc86
	@pcdev_cmdinfo ./cpm-86/lzpack.cmd
	(upx -q -9 --8086 ./cpm-86/lzpack.cmd 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :

################################################################################

# Real-mode MS-DOS build using Open Watcom V2.0's "owcc" compiler driver.
# https://github.com/open-watcom/open-watcom-v2

msdos dos pcdos: cs8080.h csz80.h cschk.h stubasm.c lzpack.c lz86body.asm \
	.lz86gen.sh
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command upx owcc wasm)
	@mkdir -p ./msdos/
	(cd msdos && owcc -v -bcom -march=i86 -mcmodel=t -frerun-optimizer \
		-Os -fno-stack-check -DMAXSYM=96 -DMAXREF=96 \
		-DMAXCODE=768 -s -I.. -fm=stubasm.map -o ./stubasm.com \
		-DNDEBUG ../stubasm.c)
	(upx -q -9 --8086 ./msdos/stubasm.com 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :
	sh ./.lz86gen.sh watcom ./lz86body.asm > ./msdos/lz86.asm
	(cd msdos && wasm -q -0 -mt -fo=lz86.obj lz86.asm)
	(cd msdos && owcc -v -bcom -march=i86 -mcmodel=t -frerun-optimizer \
		-Os -fno-stack-check -DLZPACK_STREAM=1 -DHSZ=1024 \
		-DMZXFILE=65535L -s -I.. -fm=lzpack.map -o ./lzpack.com \
		-DNDEBUG ../lzpack.c ./lz86.obj)
	(upx -q -9 --8086 ./msdos/lzpack.com 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :

################################################################################

# Protected-mode (386+) MS-DOS build using DJGPP and embedded CWSDPMI.
# https://www.delorie.com/djgpp/

djgpp: cs8080.h csz80.h cschk.h stubasm.c lzpack.c
	@mkdir -p ./djgpp/
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		export DJGPP_TRIP="i586-pc-msdosdjgpp" && \
		find_command upx \
		"$${DJGPP_GCC:-/opt/djgpp/bin/$${DJGPP_TRIP}-gcc}" \
		"$${DJGPP_STRIP:-/opt/djgpp/$${DJGPP_TRIP}/bin/strip}" \
		"$${DJGPP_EXE2COFF:-/opt/djgpp/$${DJGPP_TRIP}/bin/exe2coff}")
	test -f "$${CWSDSTUB:-/opt/cwspdmi/cwsdstub.exe}"
	"$${DJGPP_GCC:-/opt/djgpp/bin/i586-pc-msdosdjgpp-gcc}" -s \
		-march=i386 -O3 -o ./djgpp/stubasm.exe ./stubasm.c
	"$${DJGPP_STRIP:-/opt/djgpp/i586-pc-msdosdjgpp/bin/strip}" \
		./djgpp/stubasm.exe
	"$${DJGPP_EXE2COFF:-/opt/djgpp/i586-pc-msdosdjgpp/bin/exe2coff}" \
		./djgpp/stubasm.exe
	cat "$${CWSDSTUB:-/opt/cwspdmi/cwsdstub.exe}" ./djgpp/stubasm \
		> ./djgpp/stubasm.exe
	(upx -q -9 ./djgpp/stubasm.exe 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :
	"$${DJGPP_GCC:-/opt/djgpp/bin/i586-pc-msdosdjgpp-gcc}" -s \
		-march=i386 -O3 -o ./djgpp/lzpack.exe ./lzpack.c
	"$${DJGPP_STRIP:-/opt/djgpp/i586-pc-msdosdjgpp/bin/strip}" \
		./djgpp/lzpack.exe
	"$${DJGPP_EXE2COFF:-/opt/djgpp/i586-pc-msdosdjgpp/bin/exe2coff}" \
		./djgpp/lzpack.exe
	cat "$${CWSDSTUB:-/opt/cwspdmi/cwsdstub.exe}" ./djgpp/lzpack \
		> ./djgpp/lzpack.exe
	(upx -q -9 ./djgpp/lzpack.exe 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :

################################################################################

# ELKS 8086 (https://github.com/ghaerr/elks) build using IA16-GCC.
# https://gitlab.com/tkchia/build-ia16

elks: cs8080.h csz80.h cschk.h stubasm.c lzpack.c lz86body.asm .lz86gen.sh
	@mkdir -p ./elks/
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command "$${IA16_ELF_GCC:-ia16-elf-gcc}")
	sh ./.lz86gen.sh ia16 ./lz86body.asm > ./elks/lz86.s
	"$${IA16_ELF_GCC:-ia16-elf-gcc}" -march=i8086 -mtune=i8086 -melks \
		-mregparmcall -Os -s -DLZPACK_STREAM=1 -DLZPACK_OPT=1 \
		-DHSZ=1024 -DMZXFILE=65535L -maout-heap=32767 \
		-o ./elks/lzpack ./lzpack.c ./elks/lz86.s

################################################################################

# Windows 32-bit MSVCRT and 64-bit UCRT builds using Fedora MinGW-w64 GCC

windows: cs8080.h csz80.h cschk.h stubasm.c lzpack.c
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command upx \
		"$${MINGW64_GCC:-x86_64-w64-mingw32ucrt-gcc}" \
		"$${MINGW32_GCC:-i686-w64-mingw32-gcc}")
	@mkdir -p ./windows/
	"$${MINGW64_GCC:-x86_64-w64-mingw32ucrt-gcc}" -O3 -s \
		-o ./windows/lzpack64.exe ./lzpack.c
	(upx -q -9 windows/lzpack64.exe 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :
	"$${MINGW32_GCC:-i686-w64-mingw32-gcc}" -O3 -s \
		-o ./windows/lzpack32.exe ./lzpack.c
	(upx -q -9 windows/lzpack32.exe 2> /dev/null | \
		grep ' \-> ' 2> /dev/null) || :

################################################################################

# Runs extensive end-to-end tests on the CP/M-80, CP/M-86, and native binaries.

test: lzpack tests/run.sh .common.sh
	@./tests/run.sh

################################################################################

# Runs extensive source code checks to hopefully ensure high code quality.

lint: .lint.sh .common.sh
	@./.lint.sh

################################################################################

megalint everything-lint: .lint.sh .common.sh tests/run.sh
	"$${MAKE:-make}" distclean
	"$${MAKE:-make}" lint
	"$${MAKE:-make}" all cpm cpm86 msdos djgpp elks windows
	"$${MAKE:-make}" test

################################################################################

# Runs the binary release process for CP/M-80, CP/M-86, and MS-DOS binaries.
# Not for end users.

bindist: .lint.sh .common.sh .updatedocs.sh tests/run.sh
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command arc compress "$${GIT_CMD:-git}" \
		"$${MAKE:-make}" zip)
	"$${MAKE:-make}" distclean
	"$${MAKE:-make}" all cpm cpm86 msdos djgpp elks windows
	mkdir -p ./bindist/
	# CP/M-80 8080
	test -f ./cpm-8080/lzpack.com
	(cd cpm-8080 && mv -f lzpack.com LZPACK.COM && \
		arc as LZPCKI80.ARC LZPACK.COM)
	mv -f ./cpm-8080/LZPCKI80.ARC ./bindist/LZPCKI80.ARC
	# CP/M-80 Z80
	test -f ./cpm-z80/lzpack.com
	(cd cpm-z80 && mv -f lzpack.com LZPACK.COM && \
		arc as LZPCKZ80.ARC LZPACK.COM)
	mv -f ./cpm-z80/LZPCKZ80.ARC ./bindist/LZPCKZ80.ARC
	# CP/M-86
	test -f ./cpm-86/lzpack.cmd
	(cd cpm-86 && mv -f lzpack.cmd LZPACK.CMD && \
		arc as LZPCK86C.ARC LZPACK.CMD)
	mv -f ./cpm-86/LZPCK86C.ARC ./bindist/LZPCK86C.ARC
	# MS-DOS 8088
	test -f ./msdos/lzpack.com
	zip -0 -X -D -j ./msdos/lzpack.com.zip ./msdos/lzpack.com
	chmod a-x ./msdos/lzpack.com.zip
	mv -f ./msdos/lzpack.com.zip ./bindist/LZPCK86R.ZIP
	# MS-DOS 386+
	test -f ./djgpp/lzpack.exe
	zip -0 -X -D -j ./djgpp/lzpack.exe.zip ./djgpp/lzpack.exe
	chmod a-x ./djgpp/lzpack.exe.zip
	mv -f ./djgpp/lzpack.exe.zip ./bindist/LZPCK86P.ZIP
	# ELKS 8086
	test -f ./elks/lzpack
	compress -v -f -k -b 13 ./elks/lzpack
	chmod a-x ./elks/lzpack.Z
	mv -f ./elks/lzpack.Z ./bindist/LZPCKELK.Z
	# Windows (32-bit)
	test -f ./windows/lzpack32.exe
	zip -0 -X -D -j ./windows/lzpack32.exe.zip ./windows/lzpack32.exe
	chmod a-x ./windows/lzpack32.exe.zip
	mv -f ./windows/lzpack32.exe.zip ./bindist/LZPCKW32.ZIP
	# Windows (64-bit)
	test -f ./windows/lzpack64.exe
	zip -0 -X -D -j ./windows/lzpack64.exe.zip ./windows/lzpack64.exe
	chmod a-x ./windows/lzpack64.exe.zip
	mv -f ./windows/lzpack64.exe.zip ./bindist/LZPCKW64.ZIP
	"$${MAKE:-make}" distclean
	"$${MAKE:-make}" all
	./.updatedocs.sh
	"$${MAKE:-make}" distclean
	"$${GIT_CMD-git}" status || :

################################################################################

tags etags ctags gtags TAGS GPATH GRTAGS GTAGS cscope cscope.out tag: \
	cs8080.h csz80.h stubasm.c lzpack.c tests/t_autoarch.c
	@command -v etags > /dev/null 2>&1 && \
		{ { echo etags...; etags cs8080.h csz80.h stubasm.c lzpack.c \
			tests/t_autoarch.c && exit 0; }; exit 1; } || :
	@command -v ctags > /dev/null 2>&1 && \
		{ { echo ctags...; ctags cs8080.h csz80.h stubasm.c lzpack.c \
			tests/t_autoarch.c 2> /dev/null && exit 0; }; \
			exit 1; } || :
	@command -v gtags > /dev/null 2>&1 && \
		{ { echo gtags...; gtags . && exit 0; }; \
			exit 1; } || :
	@command -v cscope > /dev/null 2>&1 && \
		{ { echo cscope...; cscope -b cs8080.h csz80.h stubasm.c \
			lzpack.c tests/t_autoarch.c && exit 0; }; \
			exit 1; } || :

################################################################################

# Not used yet!

scspell: ./.scspell/basedict.txt ./.scspell/dictionary.txt
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command grep scspell find rm)
	@printf '%s\n' \
		"* Running scspell, use scspell-fix to run interactively" \
			2> /dev/null || :; \
	set -x; rm -f ./tags ./GPATH ./GRTAGS ./GTAGS > /dev/null 2>&1; \
	scspell \
		--report-only \
		--override-dictionary ./.scspell/dictionary.txt \
		--base-dict ./.scspell/basedict.txt \
		$$( find . \( -path ./.git -o -path ./.venv -o -path ./vendor \
			-o -name '.doc.tmpl' -o -name 'README.md' \) \
			-prune -o -type f -exec grep -l 'scspell-id:' {} \; )

################################################################################

# Not used yet!

scspell-fix: ./.scspell/basedict.txt ./.scspell/dictionary.txt
	@(export CPE1704TKS=1 && . ./.common.sh && \
		export FIND_COMMAND_FATAL=1 && \
		find_command scspell find grep)
	@printf '%s\n' \
		"* Running scspell-fix, use scspell to run non-interactively" \
			2> /dev/null || :
	scspell \
		--override-dictionary ./.scspell/dictionary.txt \
		--base-dict ./.scspell/basedict.txt \
		$$( find . \( -path ./.git -o -path ./.venv -o -path ./vendor \
			-o -name '.doc.tmpl' -o -name 'README.md' \) \
			-prune -o -type f -exec grep -l 'scspell-id:' {} \; )

################################################################################

.PHONY: all build clean distclean reallyclean stub stubs strip cpm cpm80 \
	cpm80-auto cpm-auto cpm-local cpm80-local cpm-docker cpm80-docker \
	lint test cpm86 cpm-86 msdos djgpp elks windows bindist tags etags \
	ctags gtags TAGS GPATH GRTAGS GTAGS cscope cscope.out tag scspell \
	scspell-fix dos pcdos everything-lint megalint

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
