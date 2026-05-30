; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 6813f15a-59dc-11f1-bf24-80ee73e9b8e7

; Z80 setup block - runs in place at the stub load address (stub_v).
; It restores the 16 literal header bytes the .COM header overwrote at 0100h,
; relocates the decompressor block to high memory (backward LDDR copy), presets
; the payload-relocation arguments in HL/DE/BC and jumps into the relocated
; decompressor (which finishes the payload copy itself, from safe high memory).
;
; This block has no internal branch targets, so it is position independent and
; ORG only documents the example load address.  The five 16-bit LD operands and
; the final JP operand are per-file patch slots: lzpack overwrites them via the
; P_* offsets emitted alongside this listing.  The EQU values below are simply
; the placeholders baked into the verbatim blob so the assembler reproduces it
; byte-for-byte; they are meaningless at run time.

LIT_SRC     EQU 00F12h   ; patch: vaddr of 16 saved literal bytes
DCMP_SRCTOP EQU 01007h   ; patch: top vaddr of file-resident decompressor block
DCMP_DSTTOP EQU 01775h   ; patch: top vaddr of relocated decompressor block
PL_SRCTOP   EQU 00F11h   ; patch: top vaddr of file-resident payload
PL_DSTTOP   EQU 0167Fh   ; patch: top vaddr of relocated payload
PL_LEN      EQU 00E02h   ; patch: payload length
DCMP_RUN    EQU 016B2h   ; patch: relocated decompressor entry (= START)

        ORG 0F22h
ENTRY:
        ; 1. restore the 16 header bytes -> 0100h (CP/M TPA)
        LD   HL,LIT_SRC
        LD   DE,0100h
        LD   BC,16
        LDIR
        ; 2. relocate the decompressor block to high memory (dest > src, so
        ;    copy top-down with LDDR; 09Bh = 155 = STUBLEN - this block's length)
        LD   HL,DCMP_SRCTOP
        LD   DE,DCMP_DSTTOP
        LD   BC,09Bh         ; 09Bh = 155 = STUBLEN - this block's length
        LDDR
        ; 3. preset payload relocation args (consumed by the LDDR at START), then
        ;    enter the relocated decompressor in high memory
        LD   HL,PL_SRCTOP
        LD   DE,PL_DSTTOP
        LD   BC,PL_LEN
        JP   DCMP_RUN
