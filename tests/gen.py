#!/usr/bin/env python3
# scspell-id: b0a7050e-58d5-11f1-a28c-80ee73e9b8e7

# Generate CP/M-80 .COM test programs that print a unique marker via BDOS fn 9,
# then JMP 0.  Padded to a target size with either compressible or random filler
# placed AFTER the terminating jump (never executed).
import os, sys, random

def make(name, target, marker, fill):
    msg = marker.encode('ascii') + b'\r\n$'
    # code at 0x100:
    #   LXI D,msg / MVI C,9 / CALL 5 / JMP 0
    code = bytes([0x11,0x0B,0x01, 0x0E,0x09, 0xCD,0x05,0x00, 0xC3,0x00,0x00])
    assert len(code) == 11           # msg starts at 0x10B
    body = code + msg
    pad = target - len(body)
    if pad < 0:
        raise SystemExit("target too small for %s" % name)
    if fill == 'zero':
        filler = b'\x00' * pad
    elif fill == 'text':
        unit = b'The quick brown fox jumps over the lazy dog. 0123456789. '
        filler = (unit * (pad // len(unit) + 1))[:pad]
    elif fill == 'rand':
        random.seed(1234)
        filler = bytes(random.randrange(256) for _ in range(pad))
    else:
        raise SystemExit("bad fill")
    open(name,'wb').write(body + filler)
    print("  %-22s %6d bytes  marker=%r fill=%s" % (os.path.basename(name), target, marker, fill))

d = os.path.dirname(__file__) + '/corpus'
make(d+'/tiny.com',    200,   'TINY-MARK-A1',          'text')
make(d+'/small.com',   2048,  'SMALL-MARK-B2',         'text')
make(d+'/med.com',     8192,  'MED-MARK-C3',           'text')
make(d+'/big16.com',   16384, 'BIG16-MARK-D4',         'text')
make(d+'/big24.com',   24576, 'BIG24-MARK-E5',         'zero')
make(d+'/big46.com',   46848, 'BIG46-MARK-F6',         'text')   # near 0xBDFF ceiling
make(d+'/incomp.com',  4096,  'INCOMP-MARK-G7',        'rand')   # should be 'inefficient, skipped'
make(d+'/over.com',    52000, 'OVER-MARK-H8',          'text')   # expected: too big / would not fit
