; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 5aa663ae-59dc-11f1-a4fb-80ee73e9b8e7

; Z80 decompressor block - relocated by the setup block to high memory and run
; there (just above out_end).  It first finishes relocating the compressed
; payload (the LDDR uses HL/DE/BC preset by the setup block), then decodes the
; lzpack stream into the CP/M TPA starting at 0110h.
;
; The Z80 version keeps all the state in registers using the alternate bank
; (EXX) instead of fixed scratch RAM:
;
;   bank M (working set)   HL = DST  (output pointer)
;                          DE = match offset (D = high, E = low) during a match
;                          A  = scratch / current length value 'a'
;   bank P (after EXX)     HL'= SRC  (compressed stream pointer)
;                          E' = bit reservoir  (see GETBIT)
;                          D' = current stream byte being shifted out
;                          C' = 'c' length base / FORM2 high accumulator
;
; GETBIT (runs in bank P) is a subroutine at the end of this block (see there).
;   E' is seeded with 80h so the very first GETBIT triggers a refill; the marker
;   bit then walks 01h,02h,..,80h and wraps every 8 bits, refilling D' again.
;
; Control flow inside this block is PC-relative (JR/DJNZ) except the absolute
; "JP LOOP" back-edge and the five "CALL GETBIT" sites.  Those operands, plus the
; two CP out_end immediates, are the per-file patch slots: P_JP_LOOP, P_CP_HI,
; P_CP_LO, and z80_getbit_fix[] (all CALL GETBIT operands, retargeted to the
; relocated GETBIT at run_base + Z80_GETBIT_OFF).  ORG 016B2h is the placeholder
; address baked into the verbatim blob; lzpack relocates these per file.

OUT_END_HI EQU 016h      ; patch: (out_end>>8)
OUT_END_LO EQU 080h      ; patch: (out_end&0ffh)

        ORG 016B2h
START:
        LDDR                 ; finish payload relocation (HL/DE/BC from setup)
        EX   DE,HL           ; DE held dst-1; put it in HL
        INC  HL              ; HL = payload bottom = compressed SRC pointer
        LD   E,080h          ; reservoir seeded empty (forces refill on 1st bit)
        EXX                  ; park SRC/reservoir in bank P
        LD   HL,0110h        ; bank M: DST = 0110h (TPA + restored 16 bytes)

LOOP:                        ; main token loop, bank M (HL = DST)
        LD   A,H
        CP   OUT_END_HI
        JR   NZ,TOK
        LD   A,L
        CP   OUT_END_LO
        JP   Z,0100h         ; DST == out_end -> run decompressed program; else fall

TOK:    EXX                  ; -> bank P (SRC/reservoir)
        CALL GETBIT          ; CY = control bit
        LD   A,(HL)          ; GETRAW: A = next stream byte
        INC  HL
        JR   C,ISMTCH        ; control bit 1 -> match
        EXX                  ; literal: -> bank M
        LD   (HL),A
        INC  HL
        JR   LOOP

ISMTCH:                      ; A = first match byte (bank P)
        BIT  7,A
        JR   NZ,NOTF1
        ; FORM1: offset = first byte (0..127); length base a = 0
        EXX                  ; -> bank M
        LD   D,0             ;   offset high = 0
        LD   E,A             ;   offset low  = first byte
        EXX                  ; -> bank P
        XOR  A               ; a = 0
        JR   LF

NOTF1:  BIT  6,A
        JR   NZ,FORM3
        ; FORM2: 7-bit field + 4 streamed bits -> 12-bit offset; a = 1
        RES  7,A             ; A = first & 7fh
        LD   BC,0400h        ; B = 4 (bits to read), C = 0 (high accumulator)
F2L:    CALL GETBIT
        RLA                  ; A = (A<<1)|bit; CY = overflow
        RL   C               ; C = (C<<1)|CY
        DJNZ F2L
        ADD  A,080h          ; offset low = field + 80h; CY = overflow
        EXX
        LD   E,A             ;   bank M: offset low
        EXX
        LD   A,C
        ADC  A,0             ; offset high = C + carry
        EXX
        LD   D,A             ;   bank M: offset high
        EXX
        LD   A,1             ; a = 1
        JR   LF

FORM3:                       ; 13-bit offset from 6 low bits + 1 streamed byte
        AND  03Fh            ; A = first & 3fh
        SRL  A               ; A >>= 1; CY = bit0
        EXX
        LD   D,A             ;   bank M: offset high = (first&3f)>>1
        EXX
        LD   A,(HL)          ; GETRAW second byte
        INC  HL
        RRA                  ; A = (savedbit<<7)|(byte>>1); CY = byte&1 = b0
        EXX
        LD   E,A             ;   bank M: offset low
        EXX
        LD   A,2             ; a = 2
        JR   NC,COPY         ; b0 = 0 -> length 3
        LD   C,1             ; c = 1
        LD   B,2             ; FORM3: up to 2 unary length slots (a already = 2)
        JR   ULOOP           ; b0 = 1 -> extended length

; ---- length grammar: a in A, c in C', bank P ----
LF:     LD   C,A             ; c = a
        LD   B,3             ; FORM1/FORM2: up to 3 unary length slots
ULOOP:  INC  A
        CALL GETBIT
        JR   NC,COPY
        DJNZ ULOOP
        LD   A,2             ; a = 2; decode extended bit length b in A
LEXT:   CALL GETBIT
        JR   NC,LEXTD
        INC  A
        CP   7
        JR   NZ,LEXT
LEXTD:  LD   B,A             ; B = b (number of value bits)
        LD   A,1             ; a = 1 (implicit leading 1)
LRD:    CALL GETBIT
        RLA                  ; a = (a<<1)|bit
        DJNZ LRD
        ADD  A,C             ; a = (a + c) & ffh

; ---- copy a+1 bytes from DST-offset-1 to DST ----
COPY:   EXX                  ; -> bank M (HL = DST, DE = offset)
        PUSH HL              ; save DST
        SCF
        SBC  HL,DE           ; HL = DST - offset - 1 = match source
        POP  DE              ; DE = DST (destination)
        LD   B,0
        LD   C,A             ; BC = a
        INC  BC              ; BC = a + 1 = byte count
        LDIR
        EX   DE,HL           ; HL = DST advanced past the copy
        JP   LOOP

; ---- GETBIT: next stream bit -> CY (bank P).  Clobbers D' (and HL' on refill).
; The reservoir marker E' rotates; when it wraps (CY set) D' is refilled from
; *SRC++.  Rotating D' then yields the next data bit (MSB first) in CY.  CALL is
; absolute, so lzpack relocates each call operand per file (z80_getbit_fix[]).
GETBIT: RLC  E               ; rotate marker; CY set on wrap (reservoir empty)
        JR   NC,GBROT        ; still bits buffered -> just rotate D'
        LD   D,(HL)          ; refill: D' = *SRC++
        INC  HL
GBROT:  RLC  D               ; CY = next data bit
        RET
