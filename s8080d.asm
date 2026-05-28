; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: d481d522-585c-11f1-8c19-80ee73e9b8e7

; 8080 decompressor block - runs relocated at STUB_RUN (just above out_end).
; pointers/bit-buffer live in fixed CP/M-80 scratch RAM (0040h-004Fh).
; only JMP/CALL targets (labels) need per-file relocation by +STUB_RUN.
; OUT_END_HI / OUT_END_LO are per-file patch slots.

SRCV    EQU 0040h        ; (2) compressed source pointer
DSTV    EQU 0042h        ; (2) output pointer
BITDAT  EQU 0044h        ; (1) bit reservoir (sentinel-marked; see GETBIT)
OFFV    EQU 0046h        ; (2) match offset

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
        XCHG
        SHLD SRCV
        LXI  H, 0110h
        SHLD DSTV
        MVI  A,80h            ; seed reservoir empty (sentinel; forces refill on 1st bit)
        STA  BITDAT
LOOP:                         ; entered only with HL = DST (init / literal / COPY)
        MOV  A,H
        CPI  OUT_END_HI
        JNZ  TOK
        MOV  A,L
        CPI  OUT_END_LO
        JZ   0100h            ; DST == out_end -> run the decompressed program
TOK:
        CALL GETBIT          ; CY = control bit
        CALL GETRAW          ; A  = raw byte
        JC   ISMTCH
        ; literal: store A
        LHLD DSTV
        MOV  M,A
        INX  H
        SHLD DSTV
        JMP  LOOP

; ---- match: A = first byte ----
ISMTCH:
        MOV  B,A             ; B = first byte (preserved; each form reloads A from B)
        ADD  A               ; CY = bit7
        JNC  FORM1
        ADD  A               ; CY = bit6 (of original first byte)
        JNC  FORM2
        ; ---- FORM3: 13-bit offset, second raw byte ----
        MOV  A,B
        ANI  3Fh             ; (ANI clears CY on 8080 and Z80)
        RAR                  ; A = (first&3f)>>1 = off high ; CY = (first&3f)&1
        STA  OFFV+1
        CALL GETRAW          ; A = second byte (GETRAW keeps CY)
        RAR                  ; A = (cy<<7)|(second>>1) ; CY = second&1 = b0
        STA  OFFV
        MVI  D,2             ; a = 2
        JNC  COPY            ; b0=0 -> length 3 (CY still = b0 here)
        MVI  C,1             ; b0=1 -> extended length: c = 1
        MVI  B,2             ; FORM3: up to 2 unary length slots (a already = 2)
        JMP  ULOOP

FORM2:
        ; a = first & 7f ; 4 bits -> {E:D} ; off = (E + carry)<<8 | (D+80h)
        MOV  A,B
        ANI  7Fh
        MOV  D,A             ; D = low accumulator
        MVI  E,0             ; E = high (overflow)
        MVI  B,4             ; loop count (B free now)
F2L:
        CALL GETBIT          ; CY = bit
        MOV  A,D
        RAL                  ; D = (D<<1)|bit ; CY = old bit7
        MOV  D,A
        MOV  A,E
        RAL                  ; E = (E<<1)|CY
        MOV  E,A
        DCR  B
        JNZ  F2L
        MOV  A,D
        ADI  80h             ; A = D+80h ; CY = overflow
        STA  OFFV            ; off low
        MOV  A,E
        ACI  0               ; A = E + CY
        STA  OFFV+1          ; off high
        MVI  D,1             ; a = 1
        JMP  LF

FORM1:
        ; off = first byte (0..127) ; a=0
        MOV  A,B
        STA  OFFV
        XRA  A
        STA  OFFV+1
        MVI  D,0             ; a = 0

; ---- length grammar ; D=a, C=c ----
LF:
        MOV  C,D             ; c = a
        MVI  B,3             ; FORM1/FORM2: up to 3 unary length slots
ULOOP:
        INR  D               ; a++
        CALL GETBIT
        JNC  COPY
        DCR  B
        JNZ  ULOOP
        MVI  D,2             ; a = 2
LEXT:
        CALL GETBIT
        JNC  LEXTD
        INR  D
        MOV  A,D
        CPI  7
        JNZ  LEXT
LEXTD:
        MOV  E,D             ; E = b (value-bit count)
        MVI  D,1             ; a = 1
LRD:
        CALL GETBIT
        MOV  A,D
        RAL                  ; a = (a<<1)|bit
        MOV  D,A
        DCR  E
        JNZ  LRD
        MOV  A,D
        ADD  C               ; a = (a + c) & ff
        MOV  D,A

; ---- copy D+1 bytes from (DSTV-off-1) to DSTV ----
COPY:
        MOV  B,D             ; B = length-1 (D holds 'a' on every COPY entry)
        INR  B               ; B = length (1..256; 0 means 256)
        LHLD DSTV
        XCHG                 ; DE = dest
        LHLD OFFV
        INX  H               ; off+1
        MOV  A,E
        SUB  L
        MOV  L,A
        MOV  A,D
        SBB  H
        MOV  H,A             ; HL = dest - (off+1) = source
CPL:
        MOV  A,M
        STAX D
        INX  H
        INX  D
        DCR  B
        JNZ  CPL
        XCHG                 ; HL = new dest
        SHLD DSTV
        JMP  LOOP

; ---- GETBIT: returns next stream bit in CY. Clobbers A, HL. ----
; Sentinel reservoir: BITDAT holds the live bits left-justified with a single
; marker '1' bit below them.  ADD A shifts the MSB into CY; when the marker
; falls out (A becomes 0) we refill and RAL re-seeds the marker into bit 0.
GETBIT:
        LDA  BITDAT
        ADD  A               ; A<<=1 ; CY = next bit (MSB) ; Z when marker gone
        JNZ  GBST
        LHLD SRCV
        MOV  A,M             ; A = *SRC++
        INX  H
        SHLD SRCV
        RAL                  ; A = (byte<<1)|1 ; CY = bit7 (the marker enters bit 0)
GBST:
        STA  BITDAT
        RET

; ---- GETRAW: A = *SRCV++ ; preserves CY, B, C, D, E ----
GETRAW:
        LHLD SRCV
        MOV  A,M
        INX  H
        SHLD SRCV
        RET
