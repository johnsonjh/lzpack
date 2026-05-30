; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: d481d522-585c-11f1-8c19-80ee73e9b8e7

; 8080 decompressor block - runs relocated at STUB_RUN (just above out_end).
; bit-buffer and backups live in fixed CP/M-80 scratch RAM (0044h-0047h).
; only JMP/CALL targets (labels) need per-file relocation by +STUB_RUN.
; OUT_END_HI / OUT_END_LO are per-file patch slots.

; Register Allocation:
; HL = SRCV (compressed source pointer)
; DE = DSTV (output pointer)
; B  = 'a' (length value)
; C  = 'c' (length base) or scratch
; A  = bits / scratch

BITDAT  EQU 0044h        ; (1) bit reservoir (sentinel-marked; see GETBIT)
OFFV    EQU 0046h        ; (2) match offset (stored as ~offset for DAD D)

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
        INX  D               ; DE now = relocated payload bottom (= SRC start)
        XCHG                 ; HL = SRCV
        LXI  D,0110h         ; DE = DSTV
        MVI  A,80h           ; seed reservoir empty (sentinel; forces refill on 1st bit)
        STA  BITDAT

LOOP:
        MOV  A,D
        CPI  OUT_END_HI
        JNZ  TOK
        MOV  A,E
        CPI  OUT_END_LO
        JZ   0100h            ; DST == out_end -> run the decompressed program
TOK:
        CALL GETBIT          ; CY = control bit
        MOV  A,M             ; GETRAW inline (HL = SRCV)
        INX  H
        JC   ISMTCH
        ; literal: store A
        STAX D
        INX  D
        JMP  LOOP

; ---- match: A = first byte ----
ISMTCH:
        MOV  C,A             ; C = first byte
        PUSH D               ; Save DSTV (DE) to free DE for match decoding
        MOV  A,C
        ADD  A               ; CY = bit 7
        JNC  FORM1
        ADD  A               ; CY = bit 6
        JNC  FORM2

        ; ---- FORM3: 13-bit offset, second raw byte ----
        MOV  A,C
        ANI  3Fh             ; (ANI clears CY on 8080 and Z80)
        RAR                  ; CY = b0 of high bits, A = off high
        CMA                  ; Store ~high for DAD D source calculation
        STA  OFFV+1
        MOV  A,M             ; GETRAW inline
        INX  H
        RAR                  ; CY = b0 of second byte, A = (savedbit<<7)|(byte>>1)
        CMA                  ; Store ~low
        STA  OFFV
        LXI  B,0201h         ; B = a = 2, C = c = 1 (unary slots)
        JNC  C_REST          ; b0=0 -> length 3
        MVI  E,2             ; b0=1 -> extended length counter
        JMP  ULOOP

FORM2:
        ; a = first & 7f ; 4 bits -> offset
        MOV  A,C
        ANI  7Fh
        MOV  D,A             ; accumulator low
        MVI  E,0             ; accumulator high
        MVI  C,4             ; count
F2L:
        CALL GETBIT          ; CY = bit
        MOV  A,D
        RAL                  ; D = (D<<1)|bit ; CY = old bit7
        MOV  D,A
        MOV  A,E
        RAL                  ; E = (E<<1)|CY
        MOV  E,A
        DCR  C
        JNZ  F2L
        MOV  A,D
        ADI  80h             ; A = D+80h ; CY = overflow
        CMA                  ; Store ~low
        STA  OFFV
        MOV  A,E
        ACI  0               ; A = E + CY
        CMA                  ; Store ~high
        STA  OFFV+1
        MVI  B,1             ; a = 1
        JMP  LF

FORM1:
        ; off = first byte (0..127) ; a=0
        MOV  A,C
        CMA
        STA  OFFV
        MVI  A,0FFh          ; ~0
        STA  OFFV+1
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
        POP  D               ; Restore DSTV (DE)

; ---- copy B+1 bytes from (DSTV - (off+1)) to DSTV ----
COPY:
        PUSH H               ; Save advanced SRCV (HL)
        LHLD OFFV            ; HL = ~off
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
        MOV  A,M             ; refill: A = *SRCV++
        INX  H
        RAL                  ; A = (byte<<1)|1 ; CY = bit7 (the marker enters bit 0)
GBST:
        STA  BITDAT
        RET
