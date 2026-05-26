#!/usr/bin/env python3
# scspell-id: 8db6de5c-58d5-11f1-9580-80ee73e9b8e7
"""End-to-end test harness for lzpack.

Tests both correctness of the C round-trip (-R) AND real self-extraction of the
produced .COM on the tnylpo CP/M-80 emulator (the only thing that proves the
Z80/8080 stub works).

Usage:
  harness.py native            # exercise the host ./lzpack binary
  harness.py cpm               # exercise the CP/M-built lzpack.com via tnylpo
  harness.py compare           # run both, diff PASS/FAIL + sizes

A "runner" is whatever turns argv into an lzpack invocation in a scratch dir.
"""
import os, sys, shutil, subprocess, glob, tempfile

ROOT    = os.path.dirname(os.path.abspath(__file__))
CORPUS  = os.path.join(ROOT, 'corpus')
PROJECT = os.path.dirname(ROOT)
TNYLPO  = os.environ.get(
    'TNYLPO', os.path.expanduser('~/src/tnylpo/tnylpo'))
CPMEMU  = os.environ.get('CPMEMU', os.path.expanduser('~/src/cpm/cpm'))
NATIVE  = os.path.join(PROJECT, 'lzpack')
CPMCOM  = os.environ.get('CPMCOM', os.path.join(PROJECT, 'lzpack.com'))

# corpus file -> (expected_marker_substring, expectation)
# expectation: 'ok' = must self-extract; 'skip' = compressor should refuse
#              (inefficient or too big) and NOT write output.
CORPUS_FILES = [
    ('tiny.com',   b'TINY-MARK-A1',   'skip'),  # 200B < stub overhead -> skipped
    ('small.com',  b'SMALL-MARK-B2',  'ok'),
    ('med.com',    b'MED-MARK-C3',    'ok'),
    ('big16.com',  b'BIG16-MARK-D4',  'ok'),
    ('big24.com',  b'BIG24-MARK-E5',  'ok'),
    ('big46.com',  b'BIG46-MARK-F6',  'ok'),
    ('incomp.com', b'INCOMP-MARK-G7', 'skip'),  # random -> inefficient
    ('over.com',   b'OVER-MARK-H8',   'skip'),  # too big to self-extract
]
MODES = [[], ['-e'], ['-8'], ['-e', '-8']]


def run_tnylpo(workdir, comname, args=None):
    """Run a CP/M .COM under tnylpo in workdir; return decoded stdout."""
    cmd = [TNYLPO, '-n', comname] + (args or [])
    p = subprocess.run(cmd, cwd=workdir, stdin=subprocess.DEVNULL,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    return p.stdout.decode('latin-1', 'replace')


def run_cpmemu(workdir, comname, args=None):
    """Run a CP/M .COM under the cpm emulator (--exec; name sans .com)."""
    name = comname[:-4] if comname.lower().endswith('.com') else comname
    cmd = [CPMEMU, '--exec', name] + (args or [])
    p = subprocess.run(cmd, cwd=workdir, stdin=subprocess.DEVNULL,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    return p.stdout.decode('latin-1', 'replace')


# emulator used to *run* .COMs (set per command in main); native still uses
# tnylpo to self-extract its .pop output.
EMU = run_tnylpo


def lzpack_native(workdir, args):
    p = subprocess.run([NATIVE] + args, cwd=workdir, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=300)
    return p.returncode, p.stdout.decode('latin-1', 'replace')


def lzpack_cpm(workdir, args):
    # Both emulators map CP/M's upper-cased names to lowercase Unix files, so
    # the command .com and the data files on disk are lowercase.
    shutil.copy(CPMCOM, os.path.join(workdir, 'lzpack.com'))
    out = EMU(workdir, 'lzpack.com', args)
    return 0, out


def find_one(workdir, *patterns):
    for pat in patterns:
        hits = glob.glob(os.path.join(workdir, pat))
        if hits:
            return hits[0]
    return None


def test_file(runner, fname, marker, expect, results):
    src = os.path.join(CORPUS, fname)
    base = fname[:-4]                     # strip .com
    orig = open(src, 'rb').read()

    # reference: original program's console output
    wd = tempfile.mkdtemp(prefix='lz_')
    try:
        shutil.copy(src, os.path.join(wd, fname))
        ref_out = EMU(wd, fname)
        ref_ok = marker.decode('ascii') in ref_out
    finally:
        shutil.rmtree(wd, ignore_errors=True)

    for mode in MODES:
        tag = '%s %s' % (fname, ' '.join(mode) if mode else '(plain)')
        wd = tempfile.mkdtemp(prefix='lz_')
        try:
            # tnylpo maps CP/M's upper-cased names back to lowercase Unix
            # files, so the on-disk names must be lowercase for both runners.
            shutil.copy(src, os.path.join(wd, fname))
            rc, log = runner(wd, mode + [fname])
            pop = find_one(wd, base + '.pop', base + '.POP',
                           base.upper() + '.pop')

            if expect == 'skip':
                # success == no output file produced
                ok = pop is None
                results.append((tag, 'PASS' if ok else 'FAIL',
                                '-', 'correctly refused' if ok
                                else 'UNEXPECTED OUTPUT ' + os.path.basename(pop)))
                continue

            if pop is None:
                results.append((tag, 'FAIL', '-', 'no .pop produced; log=' + log.strip()[:80]))
                continue
            psize = os.path.getsize(pop)

            # self-extract: run the .pop as a .COM
            shutil.copy(pop, os.path.join(wd, 'run.com'))
            se_out = EMU(wd, 'run.com')
            se_ok = (marker.decode('ascii') in se_out)

            # C round-trip via -R.  Use the default .unp name (the -o flag is
            # upper-cased to -O by CP/M's CCP and not recognized).  -R streams
            # via malloc, so it may legitimately refuse a file too big for the
            # heap -- a clean refusal is acceptable, but any output it DOES
            # produce must match the original exactly.
            shutil.copy(pop, os.path.join(wd, 'in.pop'))
            rrc, rlog = runner(wd, ['-R', 'in.pop'])
            outb = find_one(wd, 'in.unp', 'IN.unp', 'IN.UNP')
            if outb and open(outb, 'rb').read() == orig:
                rt = 'OK'
            elif outb:
                rt = 'MISMATCH'
            elif ('too large' in rlog) or ('out of memory' in rlog):
                rt = 'refused(too-big)'
            else:
                rt = 'none'

            # PASS requires correct self-extraction; a -R MISMATCH (silent
            # corruption) or a missing/garbled restore is a hard failure.
            ok = se_ok and ref_ok and rt in ('OK', 'refused(too-big)')
            note = 'self-extract=%s roundtrip=%s' % ('OK' if se_ok else 'BAD', rt)
            results.append((tag, 'PASS' if ok else 'FAIL', psize, note))
        finally:
            shutil.rmtree(wd, ignore_errors=True)


def main():
    global EMU
    which = sys.argv[1] if len(sys.argv) > 1 else 'native'
    # which -> (lzpack runner, emulator used to run .COMs)
    table = {
        'native': (lzpack_native, run_tnylpo),   # self-extract under tnylpo
        'cpm':    (lzpack_cpm,    run_tnylpo),    # CP/M lzpack.com via tnylpo
        'cpm2':   (lzpack_cpm,    run_cpmemu),    # CP/M lzpack.com via cpm
    }
    if which not in table:
        print('usage: harness.py native|cpm|cpm2'); return 2
    runner, EMU = table[which]
    # The corpus .com files are git-ignored; regenerate them if absent.
    if not os.path.exists(os.path.join(CORPUS, CORPUS_FILES[0][0])):
        subprocess.run([sys.executable, os.path.join(ROOT, 'gen.py')], check=True)
    results = []
    for fname, marker, expect in CORPUS_FILES:
        test_file(runner, fname, marker, expect, results)
    print('\n=== %s ===' % which)
    npass = sum(1 for r in results if r[1] == 'PASS')
    for tag, status, size, note in results:
        print('  [%s] %-22s size=%-7s %s' % (status, tag, size, note))
    print('  %d/%d passed' % (npass, len(results)))
    return 0 if npass == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
