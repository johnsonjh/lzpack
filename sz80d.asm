; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 5aa663ae-59dc-11f1-a4fb-80ee73e9b8e7

; Z80 decompressor block - relocated by the setup block to high memory and run
; there (just above out_end).  It first finishes relocating the compressed
; payload (the LDDR uses HL/DE/BC preset by the setup block), then decodes the
; lzpack stream into the CP/M TPA starting at 0110h.

; The Z80 version keeps all the state in registers using the alternate bank
; (EXX) instead of fixed scratch RAM:

;   bank M (working set)   HL = DST  (output pointer)
;                          DE = match offset (D = high, E = low) during a match
;                          A  = scratch / current length value 'a'
;   bank P (after EXX)     HL'= SRC  (compressed stream pointer, pre-increment:
;                                     always points at the last byte consumed)
;                          D' = bit reservoir (see GETBIT)
;                          C' = 'c' length base / FORM2 high accumulator

; GETBIT (runs in bank P) is a subroutine at the end of this block (see there).
;   D' is seeded with 80h (a bare marker) so the very first GETBIT empties the
;   reservoir and refills it from the stream.

; The setup block parks two words on the stack: 0110h (the initial DST, popped
; at START) under 0100h (the exit address, consumed by the RET Z in LOOP).
; Bank M's B is zero on entry (the setup block's literal-restore LDIR ran in
; that bank), and stays zero (DJNZ loops and LDIR end at zero).

; Control flow inside this block is PC-relative (JR/DJNZ) except the five
; "CALL GETBIT" sites.  Those operands, plus the two CP out_end immediates,
; are the per-file patch slots: P_CP_HI, P_CP_LO, and z80_getbit_fix[] (all
; CALL GETBIT operands, retargeted to the relocated GETBIT at run_base +
; Z80_GETBIT_OFF).  ORG 016B2h is the placeholder address baked into the
; verbatim blob; lzpack relocates these per file.

OUT_END_HI EQU 016h      ; patch: (out_end>>8)
OUT_END_LO EQU 080h      ; patch: (out_end&0ffh)

        ORG 016B2h
START:
        LDDR                 ; finish payload relocation (HL/DE/BC from setup)
                             ; BC is now 0 (Bank P)
        EX   DE,HL           ; DE held dst-1 = payload bottom - 1: that is
                             ; exactly the pre-increment SRC pointer
        LD   D,080h          ; reservoir = bare marker (forces refill on 1st bit)
        EXX                  ; park SRC/reservoir in bank P
        POP  HL              ; bank M: DST = 0110h (pushed by the setup block)

LOOP:                        ; main token loop, bank M (HL = DST)
        LD   A,H
        CP   OUT_END_HI
        JR   NZ,TOK
        LD   A,L
        CP   OUT_END_LO
        RET  Z               ; DST == out_end -> pop 0100h (pushed by setup)
                             ; and run the decompressed program; else fall

TOK:    EXX                  ; -> bank P (SRC/reservoir)
        CALL GETBIT          ; CY = control bit
        INC  HL              ; GETRAW: A = next stream byte
        LD   A,(HL)
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
        LD   D,B             ;   offset high = 0 (B is 0 in bank M)
        LD   E,A             ;   offset low  = first byte
        EXX                  ; -> bank P
        XOR  A               ; a = 0
        ; fall through to LF

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
        LD   C,A             ; BC = a (B is 0 in bank M)
        INC  BC              ; BC = a + 1 = byte count
        LDIR
        EX   DE,HL           ; HL = DST advanced past the copy
        JR   LOOP

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
        ADC  A,B             ; offset high = C + carry (B is 0 in bank P)
        EXX
        LD   D,A             ;   bank M: offset high
        EXX
        LD   A,1             ; a = 1
        JR   LF

FORM3:                       ; 13-bit offset from 6 low bits + 1 streamed byte
        AND  03Fh            ; A = first & 3fh (clears CY)
        RRA                  ; A >>= 1; CY = bit0
        EXX
        LD   D,A             ;   bank M: offset high = (first&3f)>>1
        EXX
        INC  HL              ; GETRAW second byte
        LD   A,(HL)
        RRA                  ; A = (savedbit<<7)|(byte>>1); CY = byte&1 = b0
        EXX
        LD   E,A             ;   bank M: offset low
        EXX
        LD   A,2             ; a = 2
        JR   NC,COPY         ; b0 = 0 -> length 3
        LD   BC,0201h        ; b0 = 1 -> extended length: c = 1, B = 2 (unary slots)
        JR   ULOOP           ; (a already = 2)

; ---- GETBIT: next stream bit -> CY (bank P).  Clobbers D' (and HL' on refill).
; The reservoir D' holds the remaining data bits MSB-first, followed by a 1
; marker bit and zero fill.  SLA shifts the next bit into CY; when the byte
; goes zero the bit just shifted out was the marker (always 1), so D' is
; refilled from *++SRC and RL re-inserts that 1 as the new marker while
; yielding the first data bit.  CALL is absolute, so lzpack relocates each
; call operand per file (z80_getbit_fix[]).
GETBIT: SLA  D               ; CY = next bit; Z = reservoir now empty
        RET  NZ              ; data bits remain
        INC  HL              ; refill: D' = *++SRC
        LD   D,(HL)
        RL   D               ; insert marker (CY = 1); CY = first data bit
        RET
