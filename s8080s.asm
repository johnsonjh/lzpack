; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: ded12e38-585c-11f1-8a15-80ee73e9b8e7

; 8080 setup block - runs in place at the stub load address (stub_v).
; restores the 16 literal header bytes, relocates the decompressor to high
; memory (STUB_RUN), then jumps into it, with payload relocation done by
; the decompressor block itself (so it runs from safe high memory); SLn
; loop targets relocate by +stub_v; LXI/JMP operands below are patch slots.

; Two words are parked on the stack for the decompressor: 0100h (its RZ exit
; target - launching the decompressed program) under 0110h (the initial
; output pointer, popped at the decompressor's START).

; The first seven bytes (LXI H,LIT_SRC / LXI D,0100h / the 06h of MVI B,16)
; are the stub-architecture tag prologue that lzpack -L matches byte-for-byte;
; insertions must stay behind them.

LIT_SRC     EQU 0    ; patch: vaddr of 16 literal bytes
DCMP_SRCTOP EQU 0    ; patch: top vaddr of file-resident decompressor block
DCMP_DSTTOP EQU 0    ; patch: top vaddr of relocated decompressor block
DCMP_LEN    EQU 0    ; patch: decompressor block length
DCMP_RUN    EQU 0    ; patch: run address of decompressor (STUB_RUN)

        ORG 0
ENTRY:
        ; 1. restore 16 literal bytes -> 0100h, parking the decompressor's
        ;    exit address and initial output pointer
        LXI  H, LIT_SRC
        LXI  D, 0100h
        MVI  B, 16
        PUSH D               ; park 0100h: the decompressor's RZ target
SL1:    MOV  A,M
        STAX D
        INX  H
        INX  D
        DCR  B
        JNZ  SL1
        PUSH D               ; park 0110h: initial DSTV, popped at START
        ; 2. relocate decompressor block high (backward copy, dest > src).
        ;    8-bit count: the decompressor is <= 256 bytes, so B alone counts it
        ;    (lzpack patches MVI B with DCMP_LEN & 0xff; 256 wraps to 0 = full page).
        LXI  H, DCMP_SRCTOP
        LXI  D, DCMP_DSTTOP
        MVI  B, DCMP_LEN
SL3:    MOV  A,M
        STAX D
        DCX  H
        DCX  D
        DCR  B
        JNZ  SL3
        ; 3. enter the relocated decompressor
        JMP  DCMP_RUN
