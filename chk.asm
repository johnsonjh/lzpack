; Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
; SPDX-License-Identifier: MIT-0
; scspell-id: 450ec798-6021-11f1-8c06-80ee73e9b8e7

; Optional (-C) runtime memory check.  Runs in place at the stub load address
; (stub_v) before ANY memory is modified; the setup block is appended right
; after it at pack time, so the passing path simply falls through into it.
; Pure 8080 instructions, so this one block prefixes both the 8080 and the
; Z80 stubs.
;
; The pack-time fit check (-m) cannot see the run-time machine: a TPA smaller
; than the one packed for, or a TSR/driver that lowered the BDOS pointer at
; 0006h, would be silently overwritten during decompression.  This block
; verifies at run time that the highest address the unpack will write
; (patched into DST_LIM as dcmp/stub dsttop + 1) lies below the BDOS base
; AND below the live inherited stack (SP_LIM = dsttop + 1 + slack): the stub
; borrows the CCP stack, so relocating over it would be fatal even when the
; BDOS bound is satisfied.  On failure it prints "No room" and warm boots;
; nothing has been touched, so the system stays intact.
;
; Internal absolute operands (FAIL, MSG, PASS) become +stub_v fixups emitted
; by stubasm; DST_LIM/SP_LIM are the two per-file patch slots.

DST_LIM EQU 0        ; patch: highest unpack write + 1
SP_LIM  EQU 0        ; patch: highest unpack write + 1 + stack slack

        ORG 0
ENTRY:
        ; require dsttop < BDOS base: CY on (0006h) - DST_LIM means
        ; BDOS base < dsttop+1, i.e. the unpack would reach the BDOS
        LHLD 0006h           ; HL = BDOS base (top of usable TPA + 1)
        LXI  D, DST_LIM
        MOV  A,L
        SUB  E
        MOV  A,H
        SBB  D               ; CY iff HL < DE
        JC   FAIL
        ; require dsttop + slack < SP: the relocated block must stay clear
        ; of the active stack region (pushes land just below SP, and the
        ; warm-boot return word sits at SP)
        LXI  H, 0
        DAD  SP              ; HL = SP
        LXI  D, SP_LIM
        MOV  A,L
        SUB  E
        MOV  A,H
        SBB  D               ; CY iff SP < dsttop+1+slack
        JNC  PASS
FAIL:
        LXI  D, MSG
        MVI  C, 9            ; BDOS print string
        CALL 0005h
        JMP  0000h           ; warm boot; the image was never modified
MSG:
        ; one DB per element: stubasm splits operands at the first comma
        ; before DB parses the list, so a combined line would be truncated
        DB   'No room'
        DB   13
        DB   10
        DB   '$'
PASS:
        ; the setup block is appended here at pack time (fall through)
