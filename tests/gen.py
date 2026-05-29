#!/usr/bin/env python3
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: b0a7050e-58d5-11f1-a28c-80ee73e9b8e7

# Generate CP/M-80 .COM test programs that print a unique marker via BDOS fn 9,
# then JMP 0.  Padded to a target size with either compressible or random filler
# placed AFTER the terminating jump (never executed).
#
# make_cksum() additionally builds self-verifying programs: they 16-bit-sum their
# whole filler region and print "<tag>-OK" only if it matches a value baked in at
# build time.  Self-extracting one then proves the stub reproduced every byte,
# not just that the entry code ran.

import os, sys, random


def _filler(fill, n):
    if fill == "zero":
        return b"\x00" * n
    if fill == "text":
        unit = b"The quick brown fox jumps over the lazy dog. 0123456789. "
        return (unit * (n // len(unit) + 1))[:n]
    if fill == "rand":
        random.seed(1234)
        return bytes(random.randrange(256) for _ in range(n))
    if fill == "rep":
        # far-apart repeats (>1152) + long runs -> exercises FORM3 and the
        # extended-length grammar.
        random.seed(99)
        blk = bytes(random.randrange(256) for _ in range(400))
        gap = bytes(random.randrange(256) for _ in range(1500))
        out = bytearray()
        while len(out) < n:
            out += blk + gap + blk + blk + b"\xa5" * 300
        return bytes(out[:n])
    raise SystemExit("bad fill")


def make_cksum(name, target, tag, fill):
    # Self-verifying program: 16-bit-sum the filler region and compare to the
    # value computed here, printing "<tag>-OK" only on an exact match (else
    # "<tag>-BAD").  Because the program checksums its WHOLE decompressed filler,
    # the self-extraction test proves byte-exact decode through the real stub --
    # not merely that the entry code ran.  Code is 0x35 bytes; then the expected
    # word (0x135), the two messages (0x137+), then the summed filler.
    # These programs are always round-trip-tested, and CP/M stores files in
    # 128-byte records: a restored file is padded up to a record boundary, so a
    # non-multiple target would fail the byte-exact compare in the CP/M harness.
    if target % 128:
        raise SystemExit("%s: cksum target must be a 128-byte multiple" % name)
    msgok = (tag + "-OK").encode("ascii") + b"\r\n$"
    msgbad = (tag + "-BAD").encode("ascii") + b"\r\n$"
    msgok_addr = 0x137
    msgbad_addr = msgok_addr + len(msgok)
    sumdata_addr = msgbad_addr + len(msgbad)
    count = target - (sumdata_addr - 0x100)
    if count < 1:
        raise SystemExit("target too small for %s" % name)
    filler = _filler(fill, count)
    expect = sum(filler) & 0xFFFF
    code = bytes(
        [
            0x21,
            sumdata_addr & 0xFF,
            sumdata_addr >> 8,  # LXI H,SUMDATA
            0x01,
            count & 0xFF,
            (count >> 8) & 0xFF,  # LXI B,count
            0x11,
            0x00,
            0x00,  # LXI D,0  (sum)
            0x7E,
            0x83,
            0x5F,
            0x7A,
            0xCE,
            0x00,
            0x57,  # DE += *HL
            0x23,
            0x0B,
            0x78,
            0xB1,
            0xC2,
            0x09,
            0x01,  # INX H;DCX B;..;JNZ 0109
            0x2A,
            0x35,
            0x01,  # LHLD 0135 (expected)
            0x7B,
            0xBD,
            0xC2,
            0x2A,
            0x01,  # MOV A,E;CMP L;JNZ BAD
            0x7A,
            0xBC,
            0xC2,
            0x2A,
            0x01,  # MOV A,D;CMP H;JNZ BAD
            0x11,
            0x37,
            0x01,  # LXI D,MSGOK
            0xC3,
            0x2D,
            0x01,  # JMP PRINT
            0x11,
            msgbad_addr & 0xFF,
            msgbad_addr >> 8,  # BAD: LXI D,MSGBAD
            0x0E,
            0x09,
            0xCD,
            0x05,
            0x00,  # PRINT: MVI C,9;CALL 5
            0xC3,
            0x00,
            0x00,  # JMP 0
        ]
    )
    assert len(code) == 0x35, len(code)
    body = code + bytes([expect & 0xFF, expect >> 8]) + msgok + msgbad + filler
    assert len(body) == target, (len(body), target)
    open(name, "wb").write(body)
    print(
        "  %-22s %6d bytes  marker=%r fill=%s (self-cksum)"
        % (os.path.basename(name), target, tag + "-OK", fill)
    )


def make(name, target, marker, fill):
    msg = marker.encode("ascii") + b"\r\n$"
    # code at 0x100:
    #   LXI D,msg / MVI C,9 / CALL 5 / JMP 0
    code = bytes([0x11, 0x0B, 0x01, 0x0E, 0x09, 0xCD, 0x05, 0x00, 0xC3, 0x00, 0x00])
    assert len(code) == 11  # msg starts at 0x10B
    body = code + msg
    pad = target - len(body)
    if pad < 0:
        raise SystemExit("target too small for %s" % name)
    if fill == "zero":
        filler = b"\x00" * pad
    elif fill == "text":
        unit = b"The quick brown fox jumps over the lazy dog. 0123456789. "
        filler = (unit * (pad // len(unit) + 1))[:pad]
    elif fill == "rand":
        random.seed(1234)
        filler = bytes(random.randrange(256) for _ in range(pad))
    else:
        raise SystemExit("bad fill")
    open(name, "wb").write(body + filler)
    print(
        "  %-22s %6d bytes  marker=%r fill=%s"
        % (os.path.basename(name), target, marker, fill)
    )


def make_z80(name, target, marker):
    # A genuine Z80 program, used to prove the architecture autodetector tags a
    # file that uses a Z80-only opcode as Z80 (and that the resulting Z80-stub
    # self-extractor runs).  It LDIR-copies its marker into a scratch buffer and
    # prints the copy via BDOS fn 9; LDIR is ED B0, which exists only on the
    # Z80 -- on a real 8080 ED decodes as CALL, so this could not run there.
    # The ED prefix at offset 9 is what the detector keys on.  Zero filler (no
    # stray prefix bytes); size is a 128-byte-record multiple so the restored
    # file also compares byte-for-byte under CP/M (2048 = 16 * 128).
    msg = marker.encode("ascii") + b"\r\n$"
    msg_addr = 0x100 + 22  # code below is 22 bytes, so msg starts at 0x116
    buf_addr = msg_addr + len(msg)  # scratch buffer lives in the zero filler
    code = bytes(
        [
            0x21,
            msg_addr & 0xFF,
            msg_addr >> 8,  # LXI/LD HL, msg
            0x11,
            buf_addr & 0xFF,
            buf_addr >> 8,  # LXI/LD DE, buf
            0x01,
            len(msg) & 0xFF,
            len(msg) >> 8,  # LXI/LD BC, len
            0xED,
            0xB0,  # LDIR        (Z80-only)
            0x11,
            buf_addr & 0xFF,
            buf_addr >> 8,  # LXI/LD DE, buf
            0x0E,
            0x09,  # MVI/LD C, 9
            0xCD,
            0x05,
            0x00,  # CALL 5
            0xC3,
            0x00,
            0x00,  # JMP/JP 0
        ]
    )
    assert len(code) == 22, len(code)
    body = code + msg
    pad = target - len(body)
    if pad < 0:
        raise SystemExit("target too small for %s" % name)
    if target % 128:
        raise SystemExit("%s: Z80 target must be a 128-byte multiple" % name)
    open(name, "wb").write(body + b"\x00" * pad)
    print(
        "  %-22s %6d bytes  marker=%r (Z80: LDIR/ED B0)"
        % (os.path.basename(name), target, marker)
    )


d = os.path.dirname(__file__) + "/corpus"
make(d + "/tiny.com", 200, "TINY-MARK-A1", "text")
make(d + "/small.com", 2048, "SMALL-MARK-B2", "text")
make(d + "/med.com", 8192, "MED-MARK-C3", "text")
make(d + "/big16.com", 16384, "BIG16-MARK-D4", "text")
make(d + "/big24.com", 24576, "BIG24-MARK-E5", "zero")

# near 0xBDFF ceiling
make(d + "/big46.com", 46848, "BIG46-MARK-F6", "text")

# should be 'inefficient, skipped'
make(d + "/incomp.com", 4096, "INCOMP-MARK-G7", "rand")

# expected: too big / would not fit
make(d + "/over.com", 52000, "OVER-MARK-H8", "text")

# genuine Z80 program (uses LDIR) -> must autodetect as Z80 and self-extract
make_z80(d + "/z80.com", 2048, "Z80-MARK-Z1")

# Self-checksumming programs: verify the ENTIRE decompressed image byte-for-byte
# through the real Z80/8080 stub (prints "<tag>-OK" only on an exact sum match).
# Cover the long-run/extended-length, text (FORM2/FORM3), and far-match paths.
# Sizes must be 128-byte-record multiples (see make_cksum) so the restored file
# matches byte-for-byte under CP/M too: 6144=48*128, 12288=96*128, 9216=72*128.
make_cksum(d + "/ckzero.com", 6144, "CKZERO", "zero")
make_cksum(d + "/cktext.com", 12288, "CKTEXT", "text")
make_cksum(d + "/ckrep.com", 9216, "CKREP", "rep")
