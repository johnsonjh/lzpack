; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0

; 8080 decompressor block - runs relocated at STUB_RUN (just above out_end).
; pointers/bit-buffer live in fixed CP/M-80 scratch RAM (0040h-004Fh).
; only JMP/CALL targets (labels) need per-file relocation by +STUB_RUN.
; OUT_END_HI / OUT_END_LO are per-file patch slots.

SRCV    EQU 0040h        ; (2) compressed source pointer
DSTV    EQU 0042h        ; (2) output pointer
BITDAT  EQU 0044h        ; (1) bit reservoir
BITCNT  EQU 0045h        ; (1) bits remaining in reservoir
OFFV    EQU 0046h        ; (2) match offset

OUT_END_HI EQU 0         ; patch: (out_end>>8)
OUT_END_LO EQU 0         ; patch: (out_end&0xff)
PL_SRCTOP  EQU 0         ; patch: top vaddr of file-resident payload
PL_DSTTOP  EQU 0         ; patch: top vaddr of relocated payload
PL_LEN     EQU 0         ; patch: payload length
PL_DSTBOT  EQU 0         ; patch: bottom vaddr of relocated payload (SRCV start)

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
        LXI  H, PL_DSTBOT
        SHLD SRCV
        LXI  H, 0110h
        SHLD DSTV
        XRA  A
        STA  BITCNT
LOOP:
        LHLD DSTV
        MOV  A,H
        CPI  OUT_END_HI
        JNZ  TOK
        MOV  A,L
        CPI  OUT_END_LO
        JZ   DONE
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
DONE:
        JMP  0100h           ; run the decompressed program

; ---- match: A = first byte ----
ISMTCH:
        MOV  B,A             ; B = first byte (preserved across GETBIT)
        ANI  80h
        JZ   FORM1
        MOV  A,B
        ANI  40h
        JZ   FORM2
        ; ---- FORM3: 13-bit offset, second raw byte ----
        MOV  A,B
        ANI  3Fh
        ORA  A               ; clear carry
        RAR                  ; A = (first&3f)>>1 = off high ; CY = (first&3f)&1
        STA  OFFV+1
        CALL GETRAW          ; A = second byte (GETRAW keeps CY)
        RAR                  ; A = (cy<<7)|(second>>1) ; CY = second&1 = b0
        STA  OFFV
        MVI  D,2             ; a = 2
        JC   F3LONG          ; b0=1 -> extended length
        JMP  COPY            ; b0=0 -> length 3
F3LONG:
        MVI  C,1             ; c = 1
        JMP  LC

FORM1:
        ; off = first byte (0..127) ; a=0
        MOV  A,B
        STA  OFFV
        XRA  A
        STA  OFFV+1
        MVI  D,0             ; a = 0
        JMP  LF

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

; ---- length grammar ; D=a, C=c ----
LF:
        MOV  C,D             ; c = a
        INR  D               ; a++
        CALL GETBIT
        JNC  COPY
LC:
        INR  D
        CALL GETBIT
        JNC  COPY
        INR  D
        CALL GETBIT
        JNC  COPY
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
GETBIT:
        LDA  BITCNT
        DCR  A
        JP   GB1
        LHLD SRCV
        MOV  A,M
        INX  H
        SHLD SRCV
        STA  BITDAT
        MVI  A,7
GB1:
        STA  BITCNT
        LDA  BITDAT
        RLC
        STA  BITDAT
        RET

; ---- GETRAW: A = *SRCV++ ; preserves CY, B, C, D, E ----
GETRAW:
        LHLD SRCV
        MOV  A,M
        INX  H
        SHLD SRCV
        RET
