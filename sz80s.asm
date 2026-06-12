; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 6813f15a-59dc-11f1-bf24-80ee73e9b8e7

; Z80 setup block - runs in place at the stub load address (stub_v).
; It restores the 16 literal header bytes the .COM header overwrote at 0100h,
; relocates the decompressor block to high memory (backward LDDR copy), presets
; the payload-relocation arguments in HL/DE/BC and jumps into the relocated
; decompressor (which finishes the payload copy itself, from safe high memory).

; Two words are parked on the stack for the decompressor: 0100h (its RET Z
; exit target - launching the decompressed program) under 0110h (the initial
; output pointer, popped at START).  The literal-restore LDIR runs in the bank
; the decompressor uses as bank M and leaves B = 0 there (an invariant the
; decompressor relies on); the EXX before the payload arguments parks them in
; the other bank (bank P), where START's LDDR expects them.

; The first seven bytes (LD HL,LIT_SRC / LD DE,0100h / the 01h of LD BC,16)
; are the stub-architecture tag prologue that lzpack -L matches byte-for-byte;
; insertions must stay behind them.

; This block has no internal branch targets, so it is position independent and
; ORG only documents the example load address.  The six 16-bit LD operands and
; the final JP operand are per-file patch slots: lzpack overwrites them via the
; P_* offsets emitted alongside this listing.  The EQU values below are simply
; the placeholders baked into the verbatim blob so the assembler reproduces it
; byte-for-byte; they are meaningless at run time.

; DCMP_LEN is not an EQU: stubasm assembles the decompressor block first and
; injects its byte length as a predefined symbol here, so the LDDR count below
; tracks sz80d.asm automatically and can never drift out of sync.

LIT_SRC     EQU 00F12h   ; patch: vaddr of 16 saved literal bytes
DCMP_SRCTOP EQU 01007h   ; patch: top vaddr of file-resident decompressor block
DCMP_DSTTOP EQU 01775h   ; patch: top vaddr of relocated decompressor block
PL_SRCTOP   EQU 00F11h   ; patch: top vaddr of file-resident payload
PL_DSTTOP   EQU 0167Fh   ; patch: top vaddr of relocated payload
PL_LEN      EQU 00E02h   ; patch: payload length
DCMP_RUN    EQU 016B2h   ; patch: relocated decompressor entry (= START)

        ORG 0F22h
ENTRY:
        ; 1. restore the 16 header bytes -> 0100h (CP/M TPA), parking the
        ;    decompressor's exit address and initial output pointer
        LD   HL,LIT_SRC
        LD   DE,0100h
        LD   BC,16
        PUSH DE              ; park 0100h: the decompressor's RET Z target
        LDIR                 ; B = 0 here is the decompressor's bank M invariant
        PUSH DE              ; park 0110h: initial DST, popped at START
        ; 2. relocate the decompressor block to high memory (dest > src, so
        ;    copy top-down with LDDR).  DCMP_LEN is injected by stubasm as the
        ;    assembled length of sz80d.asm, so the count is always exact.
        LD   HL,DCMP_SRCTOP
        LD   DE,DCMP_DSTTOP
        LD   C,DCMP_LEN      ; B is already 0 from the LDIR above
        LDDR
        ; 3. preset payload relocation args (consumed by the LDDR at START) in
        ;    the other bank (bank P), then enter the relocated decompressor
        EXX
        LD   HL,PL_SRCTOP
        LD   DE,PL_DSTTOP
        LD   BC,PL_LEN
        JP   DCMP_RUN
