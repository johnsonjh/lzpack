:: LZPACK - msvcbuild.bat
:: Copyright (c) 2026 Jeffrey H. Johnson - johnsonjh.dev@gmail.com
:: SPDX-License-Identifier: MIT-0
:: vim: set ft=dosbatch cc=80 :
:: scspell-id: 3e847470-632a-11f1-b7db-80ee73e9b8e7

REM === Compile STUBASM ===
cl /Ob3 /GS- /Oi /O2 /W4 /wd4996 /Festubasm.exe stubasm.c

REM === Assemble 8080 Stubs ===
.\stubasm.exe s8080s.asm s8080d.asm > cs8080.h
.\stubasm.exe -r s8080r.asm > csr8080.h

REM === Assemble Z80 Stubs ===
.\stubasm.exe -z80 sz80s.asm sz80d.asm > csz80.h
.\stubasm.exe -rz80 sz80r.asm > csrz80.h

REM === Assemble Check Stub ===
.\stubasm.exe -chk chk.asm > cschk.h

REM === Compile STRPACK ===
cl /Ob3 /GS- /Oi /O2 /W4 /wd4996 /Festrpack.exe strpack.c
.\strpack.exe messages.def > csmsg.h

REM === Compile LZPACK ===
cl /Ob3 /GS- /Oi /O2 /W4 /wd4996 /Felzpack.exe lzpack.c
