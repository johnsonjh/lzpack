; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: e3feb582-5d53-11f1-9461-80ee73e9b8e7

; This is the CALL-able decompressor used for lzpack's in-RAM -R restore on
; the 8086 targets (CP/M-86 / Aztec C, ELKS / ia16-elf-gcc, MS-DOS / OW2).

; * Intel-syntax mnemonics, colon labels.
; * 8086 instructions only so no immediate shift counts > 1.
; * Every shift/rotate is by 1 so FORM3's <<7 is done with a single RCR.
; * No LEA addressing forms; match source computed with MOV/ADD.
; * Decimal immediates only.

;   SI = source ptr (compressed payload)   DS:SI
;   DI = dest ptr   (output buffer + 16)   ES:DI   (wrapper sets ES = DS)
;   BP = out_end    (near addr where decoding stops)
;   AH = 128        (reservoir seeded empty -> refill on first GETBIT)
;   DF = 0          (wrapper did CLD; required for LODS/STOS/MOVS)

; On completion (DI reaches out_end) it jumps to LZ86DONE (in the wrapper).

; Register usage:
;   SI  source     DI  dest        BP  out_end
;   AL  scratch / token byte / accumulator temp
;   AH  bit reservoir (sentinel-marked, MSB first)   -- see LZ86GB
;   BX  match offset stored as ~off (= -off-1):  match_src = DI + BX
;   DL  length value 'a'    DH  length base 'c'    (both survive GETBIT)
;   CL  loop counter (unary slots / extended bit count)    CH  scratch/zero

LZ86L:                          ; main token loop (8080 LOOP)
        cmp     di,bp           ; DST == out_end ?
        jb      LZ86GO          ;   not yet -> decode a token
        jmp     LZ86DONE        ;   reached out_end -> return (near jmp: any dist)
LZ86GO:
        call    LZ86GB          ; CF = control bit            (8080 TOK CALL GETBIT)
        lodsb                   ; AL = *SI++  (GETRAW; LODSB leaves flags intact)
        jc      LZ86MT          ; control bit 1 -> match
        stosb                   ; literal: *DI++ = AL
        jmp     LZ86L

; ---- match: AL = first token byte ----                     (8080 ISMTCH)
LZ86MT:
        test    al,128          ; bit7 ?
        jz      LZ86F1          ;   clear -> FORM1
        test    al,64           ; bit6 ?
        jz      LZ86F2          ;   clear -> FORM2
        ; ---- FORM3: 13-bit offset, second raw byte ----    (8080 FORM3)
        and     al,63           ; AL = first & 0x3f  (clears CF)
        shr     al,1            ; AL = off-high (oh) ; CF = cy (saved bit0)
        mov     bh,al           ; BH = oh  (MOV preserves CF = cy)
        lodsb                   ; AL = second raw byte (LODSB preserves CF = cy)
        rcr     al,1            ; AL = (cy<<7)|(byte>>1) = off-low ; CF = byte&1 = nc
        mov     bl,al           ; BL = off-low
        not     bl
        not     bh              ; BX = ~off
        mov     dl,2            ; a = 2
        jnc     LZ86CP          ; nc = 0 -> length 3 (ml = a+1)
        mov     dh,1            ; c = 1
        mov     cl,2            ; FORM3 extended: 2 unary length slots
        jmp     LZ86U3

LZ86F2:                         ; FORM2: 7-bit field + 4 streamed bits -> 12-bit
        and     al,127          ; AL = first & 0x7f
        mov     dl,al           ; DL = accumulator low
        xor     dh,dh           ; DH = accumulator high (carry-out bits)
        mov     cl,4            ; 4 bits to stream in
LZ86F2L:
        call    LZ86GB          ; CF = bit
        rcl     dl,1            ; DL = (DL<<1)|bit ; CF = old DL bit7
        rcl     dh,1            ; DH = (DH<<1)|CF
        dec     cl
        jnz     LZ86F2L
        mov     al,dl
        add     al,128          ; off-low = low + 0x80 ; CF = overflow
        mov     bl,al
        mov     al,dh
        adc     al,0            ; off-high = high + carry
        mov     bh,al
        not     bl
        not     bh              ; BX = ~off
        mov     dl,1            ; a = 1
        jmp     LZ86LF

LZ86F1:                         ; FORM1: off = first byte (0..127) ; a = 0
        mov     bl,al
        not     bl
        mov     bh,255          ; ~0 high byte
        xor     dl,dl           ; a = 0
        ; fall through to LF

; ---- length grammar: a in DL, c in DH ----                 (8080 LF/ULOOP)
LZ86LF:
        mov     dh,dl           ; c = a
        mov     cl,3            ; FORM1/FORM2: up to 3 unary length slots
LZ86U:
        inc     dl              ; a++
        call    LZ86GB
        jnc     LZ86CP          ; bit 0 -> ml = a+1
        dec     cl
        jnz     LZ86U
        jmp     LZ86LX          ; slots exhausted -> extended length
LZ86U3:                         ; FORM3 extended entry (a=2, c=1, 2 slots)
        inc     dl              ; a++
        call    LZ86GB
        jnc     LZ86CP
        dec     cl
        jnz     LZ86U3
LZ86LX:
        mov     dl,2            ; a = 2 ; decode extended bit length in DL
LZ86LE:
        call    LZ86GB
        jnc     LZ86LD
        inc     dl
        cmp     dl,7
        jnz     LZ86LE
LZ86LD:
        mov     cl,dl           ; CL = b (number of value bits)
        mov     dl,1            ; a = 1 (implicit leading 1)
LZ86LR:
        call    LZ86GB
        rcl     dl,1            ; a = (a<<1)|bit
        dec     cl
        jnz     LZ86LR
        add     dl,dh           ; a = (a + c) & 0xff
        ; fall through to CP

LZ86CP:                         ; ml = a+1 ; copy ml bytes from (DI + ~off)
        mov     cl,dl
        xor     ch,ch
        inc     cx              ; CX = a + 1 = byte count (1..256)
        push    si              ; save stream pointer
        mov     si,di
        add     si,bx           ; SI = DI + ~off = DI - off - 1 (match source)
        rep     movsb           ; copy forward (overlap-safe); DI advances
        pop     si              ; restore stream pointer
        jmp     LZ86L

; ---- GETBIT: next stream bit -> CF.  Clobbers AL, AH (and SI on refill). ----
; AH is a sentinel reservoir: live bits left-justified above a single marker
; '1' bit.  SHL shifts the MSB into CF; when the marker falls out (AH becomes
; 0) we refill from *SI++ and RCL re-seeds the marker into bit0.  Matches the
; 8080 GETBIT (ADD A / refill / RAL) exactly.
LZ86GB:
        shl     ah,1            ; CF = next bit (MSB) ; ZF=1 when marker gone
        jnz     LZ86GR
        lodsb                   ; refill: AL = *SI++  (LODSB preserves CF=1 marker)
        stc                     ; ensure marker bit = 1 (documents the invariant)
        rcl     al,1            ; AL = (byte<<1)|1 ; CF = old bit7 = first data bit
        mov     ah,al           ; reservoir = refilled byte
LZ86GR:
        ret
