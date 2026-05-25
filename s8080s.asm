; 8080 setup block - runs in place at the stub load address (stub_v).
; restores the 16 literal header bytes, relocates the decompressor to high
; memory (STUB_RUN), then jumps into it, with payload relocation done by
; the decompressor block itself (so it runs from safe high memory); SLn
; loop targets relocate by +stub_v; LXI/JMP operands below are patch slots.

LIT_SRC     EQU 0    ; patch: vaddr of 16 literal bytes
DCMP_SRCTOP EQU 0    ; patch: top vaddr of file-resident decompressor block
DCMP_DSTTOP EQU 0    ; patch: top vaddr of relocated decompressor block
DCMP_LEN    EQU 0    ; patch: decompressor block length
DCMP_RUN    EQU 0    ; patch: run address of decompressor (STUB_RUN)

        ORG 0
ENTRY:
        ; 1. restore 16 literal bytes -> 0100h
        LXI  H, LIT_SRC
        LXI  D, 0100h
        MVI  B, 16
SL1:    MOV  A,M
        STAX D
        INX  H
        INX  D
        DCR  B
        JNZ  SL1
        ; 2. relocate decompressor block high (backward copy, dest > src)
        LXI  H, DCMP_SRCTOP
        LXI  D, DCMP_DSTTOP
        LXI  B, DCMP_LEN
SL3:    MOV  A,M
        STAX D
        DCX  H
        DCX  D
        DCX  B
        MOV  A,B
        ORA  C
        JNZ  SL3
        ; 3. enter the relocated decompressor
        JMP  DCMP_RUN
