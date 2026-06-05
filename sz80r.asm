; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 16b2de42-60c2-11f1-a574-80ee73e9b8e7

; CALL-able Z80 decompressor for lzpack's in-RAM -R restore on CP/M-80.
; This is the sz80d.asm self-extractor core with the setup block's payload
; relocation (the leading LDDR) removed and the "run the program at 0100h" exit
; replaced by RET, so lzpack can decompress into a buffer and regain control
; instead of launching.
;
; lzpack copies this block to a writable scratch buffer, relocates its absolute
; operands by +base (decomprz80_fix[]: the JP LOOP back-edge and the five CALL
; GETBIT sites -- everything else is PC-relative JR/DJNZ and needs no fix-up),
; patches the four per-call slots below, then calls it like an ordinary near
; subroutine; it RETs once the output pointer reaches out_end.  The four slots:
;   SRCV_INIT  = compressed source pointer (payload bottom in the buffer)
;   DSTV_INIT  = output pointer (buffer + 16 restored literal bytes)
;   OUT_END_HI/LO = the output address at which decoding stops
;
; State lives in registers via the alternate bank (EXX), exactly as in the
; stub; no BDOS call happens between entry and RET.  The routine clobbers both
; register banks (caller-saved under z88dk's near-call convention) and touches
; neither IX nor IY.
;
;   bank M (main, active at entry / LOOP / RET)  HL = DST (output pointer)
;                          DE = match offset (D = high, E = low) during a match
;                          A  = scratch / current length value 'a' ; B = 0
;   bank P (after EXX)     HL'= SRC (compressed stream pointer)
;                          E' = bit reservoir  (see GETBIT)
;                          D' = current stream byte being shifted out
;                          C' = 'c' length base / FORM2 high accumulator

SRCV_INIT  EQU 0         ; patch: compressed source pointer
DSTV_INIT  EQU 0         ; patch: output pointer (buffer + 16)
OUT_END_HI EQU 0         ; patch: (out_end>>8)
OUT_END_LO EQU 0         ; patch: (out_end&0ffh)

        ORG 0
START:
        LD   HL,DSTV_INIT    ; bank M: DST = output pointer (patched per call)
        LD   B,0             ; bank M: B = 0 (offset-high source / count high)
        EXX                  ; -> bank P
        LD   HL,SRCV_INIT    ; bank P: SRC = compressed pointer (patched per call)
        LD   E,080h          ; reservoir seeded empty (forces refill on 1st bit)
        EXX                  ; -> bank M (active for LOOP)

LOOP:                        ; main token loop, bank M (HL = DST)
        LD   A,H
        CP   OUT_END_HI
        JR   NZ,TOK
        LD   A,L
        CP   OUT_END_LO
        JR   NZ,TOK
        RET                  ; DST == out_end -> return to caller

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
        JP   LOOP            ; absolute back-edge (relocated by +base)

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
        LD   A,(HL)          ; GETRAW second byte
        INC  HL
        RRA                  ; A = (savedbit<<7)|(byte>>1); CY = byte&1 = b0
        EXX
        LD   E,A             ;   bank M: offset low
        EXX
        LD   A,2             ; a = 2
        JR   NC,COPY         ; b0 = 0 -> length 3
        LD   BC,0201h        ; b0 = 1 -> extended length: c = 1, B = 2 (unary slots)
        JR   ULOOP           ; (a already = 2)

; ---- GETBIT: next stream bit -> CY (bank P).  Clobbers D' (and HL' on refill).
; The reservoir marker E' rotates; when it wraps (CY set) D' is refilled from
; *SRC++.  Rotating D' then yields the next data bit (MSB first) in CY.  CALL is
; absolute, so lzpack relocates each call operand per file (decomprz80_fix[]).
GETBIT: RLC  E               ; rotate marker; CY set on wrap (reservoir empty)
        JR   NC,GBROT        ; still bits buffered -> just rotate D'
        LD   D,(HL)          ; refill: D' = *SRC++
        INC  HL
GBROT:  RLC  D               ; CY = next data bit
        RET
