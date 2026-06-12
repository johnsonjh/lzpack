; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: d481d522-585c-11f1-8c19-80ee73e9b8e7

; 8080 decompressor block - runs relocated at STUB_RUN (just above out_end).
; the bit reservoir lives in fixed CP/M-80 scratch RAM (0044h); the match
; offset is carried on the stack (pushed negated by the form parsers, popped
; by the copy).  only JMP/CALL targets (labels) need per-file relocation by
; +STUB_RUN.  OUT_END_HI / OUT_END_LO are per-file patch slots.

; The setup block parks two words on the stack: 0110h (the initial DSTV,
; popped at START) under 0100h (the exit address, consumed by the RZ in LOOP).

; Register Allocation:
; HL = SRCV (compressed source pointer, pre-increment: always points at the
;            last byte consumed)
; DE = DSTV (output pointer); freed for scratch inside a match (DSTV pushed)
; B  = 'a' (length value)
; C  = 'c' (length base) or scratch
; A  = bits / scratch

BITDAT  EQU 0044h        ; (1) bit reservoir (sentinel-marked; see GETBIT)

OUT_END_HI EQU 0         ; patch: (out_end>>8)
OUT_END_LO EQU 0         ; patch: (out_end&0xff)
PL_SRCTOP  EQU 0         ; patch: top vaddr of file-resident payload
PL_DSTTOP  EQU 0         ; patch: top vaddr of relocated payload
PL_LEN     EQU 0         ; patch: payload length

        ORG 0
START:
        ; relocate payload high (backward copy, dest > src) then init
        LXI  H, PL_SRCTOP
        LXI  D, PL_DSTTOP
        LXI  B, PL_LEN
PRL:    MOV  A,M
        STAX D
        DCX  H
        DCX  D
        DCX  B
        MOV  A,B
        ORA  C
        JNZ  PRL
        ; DE now = relocated payload bottom - 1: exactly the pre-increment
        ; SRCV pointer
        XCHG                 ; HL = SRCV
        POP  D               ; DE = DSTV = 0110h (pushed by the setup block)
        MVI  A,80h           ; seed reservoir empty (sentinel; forces refill on 1st bit)
        STA  BITDAT

LOOP:
        MOV  A,D
        CPI  OUT_END_HI
        JNZ  TOK
        MOV  A,E
        CPI  OUT_END_LO
        RZ                   ; DST == out_end -> pop 0100h (pushed by setup)
                             ; and run the decompressed program
TOK:
        CALL GETBIT          ; CY = control bit
        INX  H               ; GETRAW inline (HL = SRCV)
        MOV  A,M
        JC   ISMTCH
        ; literal: store A
        STAX D
        INX  D
        JMP  LOOP

; ---- match: A = first byte; the two ADD A shifts dispatch on bits 7/6 and
; the form parsers recover their fields from the shifted A (no backup copy) ----
ISMTCH:
        PUSH D               ; Save DSTV (DE) to free DE for match decoding
        ADD  A               ; CY = bit 7 ; A = first<<1
        JNC  FORM1
        ADD  A               ; CY = bit 6 ; A = first<<2
        JNC  FORM2

        ; ---- FORM3: 13-bit offset, second raw byte ----
        ANA  A               ; clear CY (set by the dispatch ADD)
        RAR
        RAR
        RAR                  ; A = (first&3f)>>1 = off high ; CY = b0 of high bits
        CMA                  ; ~high for the DAD D source calculation
        MOV  D,A
        INX  H               ; GETRAW inline
        MOV  A,M
        RAR                  ; CY = b0 of second byte, A = (savedbit<<7)|(byte>>1)
        CMA                  ; ~low
        MOV  E,A
        PUSH D               ; park ~off for COPY
        LXI  B,0201h         ; B = a = 2, C = c = 1 (unary slots)
        JNC  C_REST          ; b0=0 -> length 3
        MVI  E,2             ; b0=1 -> extended length counter
        JMP  ULOOP

FORM2:
        ; A = first<<2 ; recover first&7f, then 4 bits -> offset
        RAR                  ; (CY = 0 here: the dispatch ADD took the JNC)
        RAR                  ; A = first & 7f
        MOV  D,A             ; accumulator low
        MVI  C,0             ; accumulator high
        MVI  E,4             ; count
F2L:
        CALL GETBIT          ; CY = bit
        MOV  A,D
        RAL                  ; D = (D<<1)|bit ; CY = old bit7
        MOV  D,A
        MOV  A,C
        RAL                  ; C = (C<<1)|CY
        MOV  C,A
        DCR  E
        JNZ  F2L
        MOV  A,D
        ADI  80h             ; A = D+80h ; CY = overflow
        CMA                  ; ~low
        MOV  E,A
        MOV  A,C
        ACI  0               ; A = C + CY
        CMA                  ; ~high
        MOV  D,A
        PUSH D               ; park ~off for COPY
        MVI  B,1             ; a = 1
        JMP  LF

FORM1:
        ; A = first<<1 ; off = first byte (0..127) ; a=0
        RAR                  ; A = first (CY = 0 here: the dispatch ADD took the JNC)
        CMA                  ; ~low
        MOV  E,A
        MVI  D,0FFh          ; ~high = ~0
        PUSH D               ; park ~off for COPY
        XRA  A
        MOV  B,A             ; a = 0

; ---- length grammar ; B=a, C=c ----
LF:
        MOV  C,B             ; c = a
        MVI  E,3             ; FORM1/FORM2: up to 3 unary length slots
ULOOP:
        INR  B               ; a++
        CALL GETBIT
        JNC  C_REST
        DCR  E
        JNZ  ULOOP

        MVI  B,2             ; a = 2
LEXT:
        CALL GETBIT
        JNC  LEXTD
        INR  B
        MOV  A,B
        CPI  7
        JNZ  LEXT
LEXTD:
        MOV  D,B             ; D = b (bit count)
        MVI  B,1             ; B = a (accumulator)
LRD:
        CALL GETBIT
        MOV  A,B
        RAL                  ; a = (a<<1)|bit
        MOV  B,A
        DCR  D
        JNZ  LRD
        MOV  A,B
        ADD  C               ; a = (a + c) & ff
        MOV  B,A

C_REST:
        POP  D               ; DE = ~off (parked by the form parser)

; ---- copy B+1 bytes from (DSTV - (off+1)) to DSTV ----
COPY:
        XTHL                 ; HL = DSTV ; SRCV parked in its place
        XCHG                 ; HL = ~off, DE = DSTV
        DAD  D               ; HL = DSTV - off - 1 (since ~off = -off-1)
        ; HL = source, DE = dest
        INR  B               ; B = count (1..256)
CPL:
        MOV  A,M
        STAX D
        INX  H
        INX  D
        DCR  B
        JNZ  CPL
        POP  H               ; Restore SRCV
        JMP  LOOP

; ---- GETBIT: returns next stream bit in CY. Clobbers A. ----
; Sentinel reservoir: BITDAT holds the live bits left-justified with a single
; marker '1' bit below them.  ADD A shifts the MSB into CY; when the marker
; falls out (A becomes 0) we refill and RAL re-seeds the marker into bit 0.
GETBIT:
        LDA  BITDAT
        ADD  A               ; A<<=1 ; CY = next bit (MSB) ; Z when marker gone
        JNZ  GBST
        INX  H               ; refill: A = *++SRCV
        MOV  A,M
        RAL                  ; A = (byte<<1)|1 ; CY = bit7 (the marker enters bit 0)
GBST:
        STA  BITDAT
        RET
