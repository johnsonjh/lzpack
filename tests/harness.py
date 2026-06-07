#!/usr/bin/env python3
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 8db6de5c-58d5-11f1-9580-80ee73e9b8e7

"""End-to-end test harness for lzpack.

Tests both correctness of the C round-trip (-R) AND real self-extraction of the
produced .COM on CP/M-80 (using the tnylpo emulator).  Running CP/M tests is
the only way we can actually prove the Z80 and 8080 decompression stubs work.

Usage:
  harness.py native            # exercise the host ./lzpack binary
  harness.py cpm               # exercise the CP/M-built lzpack.com via tnylpo
  harness.py compare           # run both, diff PASS/FAIL + sizes

A "runner" is whatever turns argv into an lzpack invocation in a scratch dir.
"""

import os, re, sys, shutil, subprocess, glob, tempfile, threading
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.join(ROOT, "corpus")
PROJECT = os.path.dirname(ROOT)

# Emulators: a name on PATH by default; set TNYLPO/CPMEMU/EMU2 to a path otherwise.
TNYLPO = os.environ.get("TNYLPO", "tnylpo")
CPMEMU = os.environ.get("CPMEMU", "cpm")
EMU2 = os.environ.get("EMU2", "emu2")
NATIVE = os.path.join(PROJECT, "lzpack")
CPMCOM = os.environ.get("CPMCOM", os.path.join(PROJECT, "cpm-z80", "lzpack.com"))
CPMUNP = os.environ.get("CPMUNP", os.path.join(PROJECT, "cpm-z80", "lzunpack.com"))
CPMCMD = os.environ.get("CPMCMD", os.path.join(PROJECT, "cpm-86", "lzpack.cmd"))

# Timeout for each test step (compression, extraction, round-trip)
TEST_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", 360))

# Parallel workers.  Every task is subprocess-bound (the packer and the
# emulators do the work), so plain threads scale to the CPUs without GIL
# concerns.  LZ_TEST_JOBS=1 forces today's serial order; the default is the
# machine's CPU count.  Rows print live as tasks complete, so the output
# interleaves across tasks when parallel.
try:
    JOBS = int(os.environ.get("LZ_TEST_JOBS", ""))
except ValueError:
    JOBS = 0
if JOBS < 1:
    JOBS = os.cpu_count() or 1

EMIT_LOCK = threading.Lock()

# corpus file -> (expected_marker_substring, expectation, expected_stub)
# expectation: 'ok' = must self-extract; 'skip' = compressor should refuse
#              (inefficient or too big) and NOT write output.
# expected_stub: the stub the autodetector must pick in auto mode -- '8080' for
#              a pure-8080 program, 'Z80' for one that uses a Z80-only opcode --
#              or None to not assert it (skip files, and ckrep whose random
#              filler makes its detected arch data-dependent).  The -8 and -Z
#              overrides are always checked, regardless of this field.
CORPUS_FILES = [
    ("tiny.com", b"TINY-MARK-A1", "skip", None),  # 200B < stub overhead -> skipped
    ("small.com", b"SMALL-MARK-B2", "ok", "8080"),
    ("med.com", b"MED-MARK-C3", "ok", "8080"),
    ("big16.com", b"BIG16-MARK-D4", "ok", "8080"),
    ("big24.com", b"BIG24-MARK-E5", "ok", "8080"),
    ("big46.com", b"BIG46-MARK-F6", "ok", "8080"),
    ("incomp.com", b"INCOMP-MARK-G7", "skip", None),  # random -> inefficient
    ("over.com", b"OVER-MARK-H8", "skip", None),  # too big to self-extract
    # genuine Z80 program (uses LDIR/ED B0): must autodetect as Z80, and the
    # resulting Z80-stub self-extractor must run on the Z80 emulators.
    ("z80.com", b"Z80-MARK-Z1", "ok", "Z80"),
    # self-checksumming: the marker prints only if the WHOLE decompressed image
    # sums to the value baked in at build time -> byte-exact decode is verified.
    ("ckzero.com", b"CKZERO-OK", "ok", "8080"),  # long runs -> extended-length codes
    ("cktext.com", b"CKTEXT-OK", "ok", "8080"),  # text -> FORM2/FORM3 mix
    ("ckrep.com", b"CKREP-OK", "ok", None),  # far repeats; random filler -> arch n/a
]
MODES = [[], ["-e"], ["-8"], ["-e", "-8"], ["-Z"]]


def run_tnylpo(workdir, comname, args=None, pre=None):
    """Run a CP/M .COM under tnylpo in workdir; return decoded stdout.
    pre holds tnylpo's own options (e.g. ["-m", "16K"] to shrink the TPA)."""
    cmd = [TNYLPO] + (pre or []) + ["-n", comname] + (args or [])
    p = subprocess.run(
        cmd,
        cwd=workdir,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TEST_TIMEOUT,
    )
    return p.stdout.decode("latin-1", "replace")


def run_cpmemu(workdir, comname, args=None):
    """Run a CP/M .COM under the cpm emulator (--exec; name sans .com)."""
    name = comname[:-4] if comname.lower().endswith(".com") else comname
    cmd = [CPMEMU, "--exec", name] + (args or [])
    p = subprocess.run(
        cmd,
        cwd=workdir,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TEST_TIMEOUT,
    )
    return p.stdout.decode("latin-1", "replace")


def run_emu2_cpm86(workdir, cmdname, args=None):
    """Run a native CP/M-86 .cmd under emu2-cpm86 in workdir; return decoded stdout."""
    cmd = [EMU2, cmdname] + (args or [])
    p = subprocess.run(
        cmd,
        cwd=workdir,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TEST_TIMEOUT,
    )
    return p.stdout.decode("latin-1", "replace")


# Emulator used to *run* .COMs (set per command in main)
# The native tests still use tnylpo to self-extract .pop outputs!
EMU = run_tnylpo

# Whether the packer under test really autodetects the stub in auto mode.
# The CP/M-80 split binaries are built with LZPACK_NO_AUTOARCH (the table
# costs TPA better spent on the match window) and always emit their build's
# default stub; probe_auto_stub() reads `lzpack -V` to find out.
AUTO_STUB = {"auto": True, "default": None}


def probe_auto_stub(runner):
    wd = tempfile.mkdtemp(prefix="lz_")
    try:
        _rc, out = runner(wd, ["-V"])
    finally:
        shutil.rmtree(wd, ignore_errors=True)
    m = re.search(r"Stub autodetect off: defaults to (8080|Z80)", out)
    if m:
        AUTO_STUB["auto"] = False
        AUTO_STUB["default"] = m.group(1)


def lzpack_native(workdir, args):
    p = subprocess.run(
        [NATIVE] + args,
        cwd=workdir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TEST_TIMEOUT,
    )
    return p.returncode, p.stdout.decode("latin-1", "replace")


def lzpack_cpm(workdir, args):
    # Both emulators map CP/M's upper-cased names to lowercase Unix files, so
    # the command .com and the data files on disk are lowercase.  The CP/M-80
    # distribution is a split pair: lzpack.com is compress-only, so -R (and
    # -L) invocations route to the lzunpack.com binary instead.
    if "-R" in args or "-L" in args:
        shutil.copy(CPMUNP, os.path.join(workdir, "lzunpack.com"))
        out = EMU(workdir, "lzunpack.com", args)
    else:
        shutil.copy(CPMCOM, os.path.join(workdir, "lzpack.com"))
        out = EMU(workdir, "lzpack.com", args)
    return 0, out


def lzpack_cpm86(workdir, args):
    # The packer is a native CP/M-86 program, so it runs under emu2-cpm86.  Its
    # self-extracting .pop output is still CP/M-80 (a Z80/8080 stub) which emu2
    # cannot execute, so EMU (tnylpo) self-extracts the .pop, exactly as for the
    # CP/M-80 runs.  emu2 maps CP/M's upper-cased names to lowercase Unix files.
    shutil.copy(CPMCMD, os.path.join(workdir, "lzpack.cmd"))
    return 0, run_emu2_cpm86(workdir, "lzpack.cmd", args)


def find_one(workdir, *patterns):
    for pat in patterns:
        hits = glob.glob(os.path.join(workdir, pat))
        if hits:
            return hits[0]
    return None


def emit(results, tag, status, size, note):
    """Record a result row and print it immediately (flushed), so progress
    streams line-by-line instead of all at once -- tests can be slow, and a
    piped stdout would otherwise stay fully buffered until the run ends.
    Under parallel runs rows appear in completion order; the lock keeps the
    row list and the printed lines consistent."""
    with EMIT_LOCK:
        results.append((tag, status, size, note))
        print("  [%s] %-22s size=%-11s %s" % (status, tag, size, note), flush=True)


def parse_arch(log):
    """Return the stub kind ('8080' or 'Z80') from lzpack's '[..]' verbose tag,
    or '?' if absent.  The tag is '[8080]'/'[Z80]' when forced by -8/-Z and
    '[8080 auto]'/'[Z80 auto]' when chosen by the autodetector."""
    m = re.search(r"\[(8080|Z80)( auto)?\]", log)
    return m.group(1) if m else "?"


def test_mode(runner, fname, marker, expect, expect_arch, mode, results):
    """One (corpus file, mode) combination -- the parallel unit of work.
    Each task is fully independent: it re-derives the original program's
    reference output itself (the originals just print a marker and exit, so
    the duplication is cheap, and it keeps the task graph barrier-free)."""
    src = os.path.join(CORPUS, fname)
    base = fname[:-4]  # strip .com
    orig = open(src, "rb").read()

    # reference: original program's console output
    wd = tempfile.mkdtemp(prefix="lz_")
    try:
        shutil.copy(src, os.path.join(wd, fname))
        ref_out = EMU(wd, fname)
        ref_ok = marker.decode("ascii") in ref_out
    finally:
        shutil.rmtree(wd, ignore_errors=True)

    tag = "%s %s" % (fname, " ".join(mode) if mode else "(plain)")
    wd = tempfile.mkdtemp(prefix="lz_")
    try:
        # tnylpo maps CP/M's upper-cased names back to lowercase Unix
        # files, so the on-disk names must be lowercase for both runners.
        shutil.copy(src, os.path.join(wd, fname))
        rc, log = runner(wd, mode + [fname])
        pop = find_one(wd, base + ".pop", base + ".POP", base.upper() + ".pop")

        if expect == "skip":
            # success == no output file produced
            ok = pop is None
            emit(
                results,
                tag,
                "PASS" if ok else "FAIL",
                "-",
                (
                    "correctly refused"
                    if ok
                    else "UNEXPECTED OUTPUT " + os.path.basename(pop)
                ),
            )
            return

        if pop is None:
            emit(
                results,
                tag,
                "FAIL",
                "-",
                "no .pop produced; log=" + log.strip()[:80],
            )
            return
        psize = os.path.getsize(pop)
        # lzpack writes the self-extracting file at its exact length.  The
        # native build (and any LRBC-aware CP/M target) therefore produces a
        # file that is *not* a multiple of 128 -- that size is already exact.
        # A plain CP/M target still stores whole 128-byte records and pads
        # the final one (NUL on some runtimes and 0x1A from most everything),
        # so only there do we recover the logical size by stripping the last
        # record's fill, clamped to one record so a payload that legitimately
        # ends in fill bytes is never undercounted.
        if psize % 128:
            netsize = psize
        else:
            with open(pop, "rb") as f:
                netsize = max(len(f.read().rstrip(b"\x00\x1a")), psize - 127)
        sizestr = "%d(%d)" % (psize, netsize)

        # self-extract: run the .pop as a .COM
        shutil.copy(pop, os.path.join(wd, "run.com"))
        se_out = EMU(wd, "run.com")
        se_ok = marker.decode("ascii") in se_out

        # C round-trip via -R.  Uses the default .unp name.  Because -R
        # streams via malloc, it may legitimately refuse a file too big
        # for the heap - a refusal is acceptable, but any output it DOES
        # produce must match the original exactly for the test to pass.
        shutil.copy(pop, os.path.join(wd, "in.pop"))
        rrc, rlog = runner(wd, ["-R", "in.pop"])
        outb = find_one(wd, "in.unp", "IN.unp", "IN.UNP")
        if outb and open(outb, "rb").read() == orig:
            rt = "OK"
        elif outb:
            rt = "MISMATCH"
        elif ("too large" in rlog) or ("out of memory" in rlog):
            rt = "refused(too-big)"
        else:
            rt = "none"

        # Architecture selection: lzpack's '[..]' tag names the stub it
        # used.  -8/-Z force the choice (always checked); otherwise the
        # autodetector must pick expect_arch.  A wrong stub is a hard fail:
        # picking Z80 for an 8080 program would strip its 8080 portability.
        arch = parse_arch(log)
        if "-8" in mode:
            want_arch = "8080"
        elif "-Z" in mode:
            want_arch = "Z80"
        elif AUTO_STUB["auto"]:
            want_arch = expect_arch
        else:
            # no autodetector in this build: auto mode = the build default
            want_arch = AUTO_STUB["default"]
        arch_ok = want_arch is None or arch == want_arch

        # PASS requires correct self-extraction and stub choice; a -R
        # MISMATCH (silent corruption) or a missing/garbled restore is a
        # hard failure.
        ok = se_ok and ref_ok and arch_ok and rt in ("OK", "refused(too-big)")
        note = "stub=%s self-extract=%s roundtrip=%s" % (
            arch,
            "OK" if se_ok else "BAD",
            rt,
        )
        if not arch_ok:
            note = "WRONG STUB %s (want %s)  " % (arch, want_arch) + note
        emit(results, tag, "PASS" if ok else "FAIL", sizestr, note)
    finally:
        shutil.rmtree(wd, ignore_errors=True)


# -m / MEMTOP override: the flag is only an accept/reject ceiling for the
# relocated stub, so the checks are (1) bad values fail at parse time with no
# output, (2) accepted-but-too-small values refuse per file AFTER the parse,
# (3) every spelling of one value packs byte-identically (and identically to
# the default, which -m never alters for a file that fits), (4) a raised
# ceiling really unlocks a file the 48K default refuses -- and the result
# must self-extract and round-trip -- and (5) -R skips the -m value rather
# than taking it for an input file.
MEMTOP_FORMS = [
    "32",
    "32K",
    "32k",
    "0x7DFF",
    "0X7DFF",
    "32255",
    "64K",
    "0xFDFF",
    "65023",
]
MEMTOP_BAD = ["0", "4", "1K", "65K", "0x800", "65536", "0x10000", "12Q", "X"]


def _pack(runner, wd, fname, extra, out):
    """Pack fname in wd as out (clearing any stale copy first); return the
    output bytes (or None when lzpack refused) and the run's log text."""
    p = os.path.join(wd, out)
    if os.path.exists(p):
        os.remove(p)
    _rc, log = runner(wd, extra + ["-O", out, fname])
    return (open(p, "rb").read() if os.path.exists(p) else None), log


def _scratch(*corpus):
    """Make a task-private scratch dir preloaded with corpus files."""
    wd = tempfile.mkdtemp(prefix="lz_")
    for f in corpus:
        shutil.copy(os.path.join(CORPUS, f), os.path.join(wd, f))
    return wd


def test_memtop_bad(runner, results):
    # (1) parse-time rejects: no output and the bad-value diagnostic
    wd = _scratch("small.com")
    try:
        for bad in MEMTOP_BAD:
            data, log = _pack(runner, wd, "small.com", ["-m", bad], "t.pop")
            ok = data is None and "bad -m" in log
            emit(
                results,
                "-m %s" % bad,
                "PASS" if ok else "FAIL",
                "-",
                "rejected" if ok else "NOT REJECTED log=" + log.strip()[:60],
            )
        data, log = _pack(runner, wd, "small.com", ["-m"], "t.pop")  # no value
        ok = data is None and "bad -m" in log
        emit(
            results,
            "-m (no value)",
            "PASS" if ok else "FAIL",
            "-",
            "rejected" if ok else "NOT REJECTED log=" + log.strip()[:60],
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def test_memtop_fit(runner, results, fname, top):
    # (2) parses fine but too small for the file: per-file refusal
    wd = _scratch(fname)
    try:
        data, log = _pack(runner, wd, fname, ["-m", top], "t.pop")
        ok = data is None and "would not fit" in log
        emit(
            results,
            "%s -m %s" % (fname, top),
            "PASS" if ok else "FAIL",
            "-",
            "correctly refused" if ok else "log=" + log.strip()[:60],
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def test_memtop_forms(runner, results):
    # (3) equivalence: every spelling == the default pack's exact bytes, and
    # (5) -R must skip the -m value, not open it as an input file
    wd = _scratch("med.com")
    try:
        ref, log = _pack(runner, wd, "med.com", [], "ref.pop")
        emit(
            results,
            "med.com (-m ref)",
            "PASS" if ref is not None else "FAIL",
            str(len(ref)) if ref is not None else "-",
            "reference pack",
        )
        for form in MEMTOP_FORMS:
            data, log = _pack(runner, wd, "med.com", ["-m", form], "t.pop")
            ok = ref is not None and data == ref
            emit(
                results,
                "med.com -m %s" % form,
                "PASS" if ok else "FAIL",
                str(len(data)) if data is not None else "-",
                "== default" if ok else "DIFFERS FROM DEFAULT PACK",
            )

        _rc, log = runner(wd, ["-R", "-m", "48", "-O", "med.unp", "ref.pop"])
        outb = os.path.join(wd, "med.unp")
        morig = open(os.path.join(CORPUS, "med.com"), "rb").read()
        ok = (
            os.path.exists(outb)
            and open(outb, "rb").read() == morig
            and "cannot read" not in log
        )
        emit(
            results,
            "-R -m 48 ref.pop",
            "PASS" if ok else "FAIL",
            "-",
            "restored" if ok else "RESTORE BROKE log=" + log.strip()[:60],
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def test_memtop_over(runner, results):
    # (4) -m 64 must unlock over.com (refused at the 48K default), and
    # the result must really self-extract and round-trip
    wd = _scratch("over.com")
    try:
        orig = open(os.path.join(CORPUS, "over.com"), "rb").read()
        data, log = _pack(runner, wd, "over.com", ["-m", "64"], "ov.pop")
        if data is None:
            emit(
                results,
                "over.com -m 64",
                "FAIL",
                "-",
                "no .pop; log=" + log.strip()[:60],
            )
            return
        shutil.copy(os.path.join(wd, "ov.pop"), os.path.join(wd, "run.com"))
        se_ok = "OVER-MARK-H8" in EMU(wd, "run.com")
        _rc, rlog = runner(wd, ["-R", "-O", "ov.unp", "ov.pop"])
        outb = os.path.join(wd, "ov.unp")
        if os.path.exists(outb) and open(outb, "rb").read() == orig:
            rt = "OK"
        elif ("too large" in rlog) or ("out of memory" in rlog):
            rt = "refused(too-big)"
        else:
            rt = "BAD"
        ok = se_ok and rt in ("OK", "refused(too-big)")
        emit(
            results,
            "over.com -m 64",
            "PASS" if ok else "FAIL",
            str(len(data)),
            "self-extract=%s roundtrip=%s" % ("OK" if se_ok else "BAD", rt),
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


# -C (runtime memory check): the packed file grows by exactly the check
# block (CHK_LEN, parsed from the generated cschk.h), still self-extracts
# under both stubs, still round-trips through -R, and -- run under a TPA too
# small for the decompressed image (tnylpo -m; tnylpo runs only) -- refuses
# with "No room" before touching memory instead of crashing the machine.
def _chk_len():
    """CHK_LEN from the generated cschk.h, or None when unavailable."""
    try:
        m = re.search(r"CHK_LEN (\d+)", open(os.path.join(PROJECT, "cschk.h")).read())
        return int(m.group(1)) if m else None
    except OSError:
        return None


def _net(data):
    """Logical size of a packed blob: a CP/M target stores whole 128-byte
    records, so strip the final record's fill, clamped to one record,
    exactly as test_mode does (the blobs genuinely end in decompressor
    code, never in fill bytes)."""
    if len(data) % 128:
        return len(data)
    return max(len(data.rstrip(b"\x00\x1a")), len(data) - 127)


def test_checked_variant(runner, results, mode):
    # checked packs must self-extract and round-trip with both stubs
    wd = _scratch("med.com")
    try:
        morig = open(os.path.join(CORPUS, "med.com"), "rb").read()
        data, log = _pack(runner, wd, "med.com", mode, "c.pop")
        tag = "med.com %s" % " ".join(mode)
        if data is None:
            emit(results, tag, "FAIL", "-", "no .pop; log=" + log.strip()[:60])
            return
        shutil.copy(os.path.join(wd, "c.pop"), os.path.join(wd, "runc.com"))
        se_ok = "MED-MARK-C3" in EMU(wd, "runc.com")
        _rc, _rlog = runner(wd, ["-R", "-O", "c.unp", "c.pop"])
        outb = os.path.join(wd, "c.unp")
        rt_ok = os.path.exists(outb) and open(outb, "rb").read() == morig
        ok = se_ok and rt_ok
        emit(
            results,
            tag,
            "PASS" if ok else "FAIL",
            str(len(data)),
            "self-extract=%s roundtrip=%s"
            % ("OK" if se_ok else "BAD", "OK" if rt_ok else "BAD"),
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def test_checked_size(runner, results):
    # the -C pack is the plain pack plus exactly the check block
    wd = _scratch("med.com")
    try:
        chk_len = _chk_len()
        plain, _log = _pack(runner, wd, "med.com", [], "p.pop")
        cdata, _log = _pack(runner, wd, "med.com", ["-C"], "c.pop")
        if plain is None or cdata is None or chk_len is None:
            emit(
                results,
                "med.com -C size",
                "FAIL",
                "-",
                "missing pack output or cschk.h",
            )
            return
        grew = _net(cdata) - _net(plain)
        ok = grew == chk_len
        emit(
            results,
            "med.com -C size",
            "PASS" if ok else "FAIL",
            "%d(+%d)" % (len(cdata), grew),
            (
                "== plain + CHK_LEN"
                if ok
                else "expected +%d over plain %d" % (chk_len, _net(plain))
            ),
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def test_checked_small(runner, results):
    # the refusal path needs a small TPA, which only tnylpo can emulate
    if EMU is not run_tnylpo:
        return
    wd = _scratch("big24.com")
    try:
        data, log = _pack(runner, wd, "big24.com", ["-C"], "b24.pop")
        if data is None:
            emit(
                results,
                "big24.com -C @16K",
                "FAIL",
                "-",
                "no .pop; log=" + log.strip()[:60],
            )
            return
        shutil.copy(os.path.join(wd, "b24.pop"), os.path.join(wd, "runb.com"))
        # Functional probe for the tnylpo -m (TPA size) patch: -m 64K is just
        # above the largest possible TPA, so a working patch must refuse it at
        # startup ("argument out of range").  A stock tnylpo rejects -m as an
        # unknown option, and a stale or half-patched build can ACCEPT -m yet
        # silently run with the full 64K TPA -- the usage text alone cannot
        # tell those apart, and either one would fail the small-TPA leg for
        # reasons that have nothing to do with the -C stub, so SKIP instead.
        probe = run_tnylpo(wd, "runb.com", pre=["-m", "64K"])
        if "out of range" not in probe:
            emit(
                results,
                "big24.com -C @16K",
                "SKIP",
                "-",
                "tnylpo -m (TPA size) patch missing or inactive",
            )
            return
        full = run_tnylpo(wd, "runb.com")
        small = run_tnylpo(wd, "runb.com", pre=["-m", "16K"])
        pos_ok = "BIG24-MARK-E5" in full
        neg_ok = "No room" in small and "BIG24-MARK-E5" not in small
        ok = pos_ok and neg_ok
        note = "full-TPA=%s 16K-TPA=%s" % (
            "OK" if pos_ok else "BAD",
            "refused" if neg_ok else "NOT REFUSED",
        )
        # surface the emulator's actual output on failure so a remote
        # report is diagnosable without a repro
        if not neg_ok:
            note += " out=" + re.sub(r"\s+", " ", small).strip()[:60]
        emit(
            results, "big24.com -C @16K", "PASS" if ok else "FAIL", str(len(data)), note
        )
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def _tpa_patch_ok(wd, comname):
    """True when the tnylpo -m (TPA size) patch is present and active: -m 64K
    is just above the largest possible TPA, so a working patch must refuse it
    at startup (see the longer rationale in test_checked_small)."""
    return "out of range" in run_tnylpo(wd, comname, pre=["-m", "64K"])


# -F (runtime floor): the floored pack must be the plain -C pack with only
# the DST_LIM bound raised (same length, one or two differing bytes), must
# still self-extract and -R round-trip on a full TPA, and must refuse
# ("No room") on a TPA that the plain -C pack -- whose bound covers only
# the unpack writes -- happily extracts into.
def test_checked_floor(runner, results):
    tag = "med.com -F floor"
    wd = _scratch("med.com")
    try:
        morig = open(os.path.join(CORPUS, "med.com"), "rb").read()
        cdata, _clog = _pack(runner, wd, "med.com", ["-C"], "c.pop")
        fdata, flog = _pack(runner, wd, "med.com", ["-F", "0xBDFF"], "f.pop")
        if fdata is None and "unknown option" in flog:
            emit(results, tag, "SKIP", "-", "packer has no -F (COMPRESS_ONLY)")
            return
        if cdata is None or fdata is None:
            emit(results, tag, "FAIL", "-", "missing pack; log=" + flog.strip()[:60])
            return
        if len(cdata) != len(fdata):
            emit(
                results,
                tag,
                "FAIL",
                str(len(fdata)),
                "length differs from -C pack %d" % len(cdata),
            )
            return
        diffs = sum(1 for a, b in zip(cdata, fdata) if a != b)
        size_ok = 1 <= diffs <= 2  # DST_LIM low/high (low byte may coincide)
        _rc, _rlog = runner(wd, ["-R", "-O", "f.unp", "f.pop"])
        outb = os.path.join(wd, "f.unp")
        rt_ok = os.path.exists(outb) and open(outb, "rb").read() == morig
        if EMU is not run_tnylpo:
            ok = size_ok and rt_ok
            emit(
                results,
                tag,
                "PASS" if ok else "FAIL",
                str(len(fdata)),
                "diffs=%d roundtrip=%s (no tnylpo: TPA legs skipped)"
                % (diffs, "OK" if rt_ok else "BAD"),
            )
            return
        shutil.copy(os.path.join(wd, "f.pop"), os.path.join(wd, "runf.com"))
        shutil.copy(os.path.join(wd, "c.pop"), os.path.join(wd, "runc.com"))
        if not _tpa_patch_ok(wd, "runf.com"):
            emit(results, tag, "SKIP", "-", "tnylpo -m (TPA) patch missing or inactive")
            return
        full = run_tnylpo(wd, "runf.com")
        small = run_tnylpo(wd, "runf.com", pre=["-m", "32K"])
        ctrl = run_tnylpo(wd, "runc.com", pre=["-m", "32K"])
        pos_ok = "MED-MARK-C3" in full
        neg_ok = "No room" in small and "MED-MARK-C3" not in small
        # control: the same TPA satisfies the plain -C unpack bound, so the
        # refusal above is the floor's doing, not the unpack check's
        ctrl_ok = "MED-MARK-C3" in ctrl
        ok = size_ok and rt_ok and pos_ok and neg_ok and ctrl_ok
        note = "diffs=%d roundtrip=%s full=%s 32K=%s ctrl=%s" % (
            diffs,
            "OK" if rt_ok else "BAD",
            "OK" if pos_ok else "BAD",
            "refused" if neg_ok else "NOT REFUSED",
            "OK" if ctrl_ok else "BAD",
        )
        if not ok:
            note += " out=" + re.sub(r"\s+", " ", small).strip()[:40]
        emit(results, tag, "PASS" if ok else "FAIL", str(len(fdata)), note)
    finally:
        shutil.rmtree(wd, ignore_errors=True)


# -L must report the -C check block and its embedded floor: no check ->
# "no -C check"; plain -C -> the unpack-bound floor without the (-F) mark;
# -F -> the exact requested floor with it.  A foreign (never packed) file
# must list as not-PopCom with no check/floor line at all -- and, above
# all, must never crash the lister.  On the CP/M-80 split pair the -L legs
# route to lzunpack.com, so this also proves the shipped lister's report.
def test_list_floor(runner, results):
    tag = "med.com -L floor"
    wd = _scratch("med.com")
    try:
        pdata, _plog = _pack(runner, wd, "med.com", [], "p.pop")
        cdata, _clog = _pack(runner, wd, "med.com", ["-C"], "c.pop")
        fdata, flog = _pack(runner, wd, "med.com", ["-F", "0xBDFF"], "f.pop")
        if pdata is None or cdata is None:
            emit(results, tag, "FAIL", "-", "missing pack output")
            return
        _rc, lp = runner(wd, ["-L", "p.pop"])
        _rc, lc = runner(wd, ["-L", "c.pop"])
        _rc, lx = runner(wd, ["-L", "med.com"])
        p_ok = "no -C check" in lp and "floor 0x" not in lp
        c_ok = (
            re.search(r"-C check; floor 0x[0-9A-F]{4}", lc) is not None
            and "(-F)" not in lc
        )
        x_ok = "not a PopCom" in lx and "check" not in lx
        if fdata is None and "unknown option" in flog:
            f_ok = None  # packer has no -F (COMPRESS_ONLY); skip that leg
        else:
            _rc, lf = runner(wd, ["-L", "f.pop"])
            f_ok = re.search(r"-C check; floor 0xBDFF \(-F\)", lf) is not None
        ok = p_ok and c_ok and x_ok and f_ok is not False
        note = "plain=%s -C=%s -F=%s foreign=%s" % (
            "OK" if p_ok else "BAD",
            "OK" if c_ok else "BAD",
            "skipped" if f_ok is None else ("OK" if f_ok else "BAD"),
            "OK" if x_ok else "BAD",
        )
        if not ok:
            bad = lp if not p_ok else (lc if not c_ok else (lx if not x_ok else lf))
            note += " out=" + re.sub(r"\s+", " ", bad).strip()[:60]
        emit(results, tag, "PASS" if ok else "FAIL", "-", note)
    finally:
        shutil.rmtree(wd, ignore_errors=True)


# The build's tunables (.build-cpm.sh): the shipped floors are derived from
# them, so the probes below must mirror any override.
STACKSZ = int(os.environ.get("STACKSZ", 1024))
WINMIN = int(os.environ.get("WINMIN", 1024))
LZ_RESERVE = int(os.environ.get("LZ_RESERVE", 1544))


def _map_floor_tpa(compath):
    """First safe TPA size (bytes, for tnylpo -m) of a shipped checked tool,
    from the .map beside its .com: the build patches DST_LIM = __BSS_END +
    STACKSZ + 128 (chk_floor in .build-cpm.sh), and the BDOS base for a TPA
    of t bytes is 0x100 + t, so the edge TPA is DST_LIM - 0x100.  The packer
    floors higher, at its window-fit boundary (win_floor: + 3*WINMIN +
    LZ_RESERVE more), so its whole useless no-window fringe refuses."""
    mp = os.path.splitext(compath)[0] + ".map"
    try:
        m = re.search(r"^__BSS_END_head\s*=\s*\$([0-9A-Fa-f]+)", open(mp).read(), re.M)
    except OSError:
        return None
    if not m:
        return None
    tpa = int(m.group(1), 16) + STACKSZ + 128 - 0x100
    if os.path.basename(compath).lower().startswith("lzpack"):
        # win_floor's margin is 256 where chk_floor's is 128: add the rest
        tpa += 3 * WINMIN + LZ_RESERVE + 128
    return tpa


# Shipped split-pair startup floor (cpm/tnylpo runs only): the build packs
# lzpack.com and lzunpack.com with -F, so every TPA below the map-derived
# floor must refuse cleanly ("No room", no HALT) -- including the zone where
# the CRT used to wipe the BDOS during BSS clearing and the zone where the
# heap carve-out wrapped negative and the first malloc scribbled (measured
# un-floored: wild HALTs, hangs, and a "successful" 16K window through the
# BDOS).  At the exact floor the tool must instead get far enough to fail
# (or work) in its own C code.
def test_checked_startup(runner, results):
    if EMU is not run_tnylpo:
        return
    wd = _scratch("med.com")
    try:
        _pdata, _plog = _pack(runner, wd, "med.com", [], "m.pop")
        legs = [
            # (tag, shipped binary, argv, acceptable at-floor outputs)
            # at the packer's window-aware floor the minimum window fits,
            # so a small input packs; OOM stays accepted against drift
            (
                "lzpack startup floor",
                CPMCOM,
                ["med.com"],
                ("=>", "out of memory for compression window"),
            ),
            (
                "lzunpack startup floor",
                CPMUNP,
                ["-O", "m.unp", "m.pop"],
                ("too large to restore", "=>"),
            ),
            # stubasm works entirely in static storage, so its usage
            # banner proves the CRT startup survived at the floor
            (
                "stubasm startup floor",
                os.path.join(os.path.dirname(CPMCOM), "stubasm.com"),
                [],
                ("Usage: stubasm",),
            ),
        ]
        probed = False
        for tag, com, args, want in legs:
            if not os.path.isfile(com):
                emit(results, tag, "SKIP", "-", "missing %s" % com)
                continue
            floor = _map_floor_tpa(com)
            if floor is None:
                emit(results, tag, "SKIP", "-", "no .map beside %s" % com)
                continue
            name = os.path.basename(com)
            shutil.copy(com, os.path.join(wd, name))
            if not probed:
                if not _tpa_patch_ok(wd, name):
                    emit(
                        results,
                        tag,
                        "SKIP",
                        "-",
                        "tnylpo -m (TPA size) patch missing or inactive",
                    )
                    return
                probed = True
            # mid BSS-wipe zone, top of the wrapped-heap zone, first safe TPA
            wipe = run_tnylpo(wd, name, args, pre=["-m", str(floor - 1024 - 512)])
            edge = run_tnylpo(wd, name, args, pre=["-m", str(floor - 1)])
            atf = run_tnylpo(wd, name, args, pre=["-m", str(floor)])
            wipe_ok = "No room" in wipe and "HALT" not in wipe
            edge_ok = "No room" in edge and "HALT" not in edge
            at_ok = (
                "No room" not in atf
                and "HALT" not in atf
                and any(w in atf for w in want)
            )
            ok = wipe_ok and edge_ok and at_ok
            note = "wipe=%s edge=%s floor@%d=%s" % (
                "refused" if wipe_ok else "BAD",
                "refused" if edge_ok else "BAD",
                floor,
                "clean" if at_ok else "BAD",
            )
            if not ok:
                bad = wipe if not wipe_ok else (edge if not edge_ok else atf)
                note += " out=" + re.sub(r"\s+", " ", bad).strip()[:40]
            emit(results, tag, "PASS" if ok else "FAIL", "-", note)
    finally:
        shutil.rmtree(wd, ignore_errors=True)


def main():
    global EMU
    which = sys.argv[1] if len(sys.argv) > 1 else "native"
    # which -> (lzpack runner, emulator used to run .COMs)
    table = {
        "native": (lzpack_native, run_tnylpo),  # self-extract under tnylpo
        "cpm": (lzpack_cpm, run_tnylpo),  # CP/M lzpack.com via tnylpo
        "cpm2": (lzpack_cpm, run_cpmemu),  # CP/M lzpack.com via cpm
        "cpm86": (lzpack_cpm86, run_tnylpo),  # CP/M-86 lzpack.cmd via emu2
    }
    if which not in table:
        print("usage: harness.py native|cpm|cpm2|cpm86")
        return 2
    runner, EMU = table[which]

    # Fail cleanly (no traceback) when a needed emulator or CP/M binary is
    # absent.  Emulators default to a name on PATH; set TNYLPO/CPMEMU/EMU2
    # otherwise.  cpm86 needs two: the CP/M-86-capable fork of emu2 to run
    # the packer, and tnylpo to run the CP/M-80 .pop files that it emits.
    needed = {
        "native": [(TNYLPO, "TNYLPO")],
        "cpm": [(TNYLPO, "TNYLPO")],
        "cpm2": [(CPMEMU, "CPMEMU")],
        "cpm86": [(EMU2, "EMU2"), (TNYLPO, "TNYLPO")],
    }[which]

    def have(e):
        return (os.access(e, os.X_OK) and os.path.isfile(e)) or shutil.which(e)

    for e, ev in needed:
        if not have(e):
            sys.stderr.write(
                "error: emulator %r not found; put it on PATH or set %s\n" % (e, ev)
            )
            return 2
    if which in ("cpm", "cpm2") and not os.path.isfile(CPMCOM):
        sys.stderr.write(
            "error: %s not found; run 'make cpm' first (or set CPMCOM)\n" % CPMCOM
        )
        return 2
    if which == "cpm86" and not os.path.isfile(CPMCMD):
        sys.stderr.write(
            "error: %s not found; run 'make cpm86' first (or set CPMCMD)\n" % CPMCMD
        )
        return 2
    env = " + ".join(ev for _, ev in needed)
    cpm8680 = env == "EMU2 + TNYLPO"

    # The test corpus .com files are reproduced by gen.py if any are missing.
    if any(
        not os.path.exists(os.path.join(CORPUS, f)) for f, _m, _e, _a in CORPUS_FILES
    ):
        subprocess.run([sys.executable, os.path.join(ROOT, "gen.py")], check=True)
    if cpm8680:
        print("===== Using EMU2-86+TNYLPO Combo Mode =====", flush=True)
    else:
        print("===== Using %s for CP/M emulation =====" % env, flush=True)
    print("=====      %3d parallel job(s)        =====" % JOBS, flush=True)
    print("===========================================\n", flush=True)

    # Auto-mode stub expectations depend on whether this packer build kept
    # the autodetector (see AUTO_STUB above).
    probe_auto_stub(runner)
    if not AUTO_STUB["auto"]:
        print(
            "(no stub autodetect in this build: auto mode expects %s)\n"
            % AUTO_STUB["default"],
            flush=True,
        )
    results = []

    # Every task is independent (private scratch dirs; read-only shared
    # inputs), so they fan out across LZ_TEST_JOBS threads and their rows
    # print live, interleaved in completion order.  A task that dies (e.g.
    # a hung emulator hitting TEST_TIMEOUT) becomes a FAIL row under its
    # task tag instead of killing the whole run.
    tasks = []
    for fname, marker, expect, expect_arch in CORPUS_FILES:
        for mode in MODES:
            tag = "%s %s" % (fname, " ".join(mode) if mode else "(plain)")
            tasks.append(
                (
                    tag,
                    test_mode,
                    (runner, fname, marker, expect, expect_arch, mode, results),
                )
            )
    tasks.append(("-m bad values", test_memtop_bad, (runner, results)))
    tasks.append(
        ("med.com -m 0x1FFF", test_memtop_fit, (runner, results, "med.com", "0x1FFF"))
    )
    tasks.append(
        ("big46.com -m 32", test_memtop_fit, (runner, results, "big46.com", "32"))
    )
    tasks.append(("-m spellings", test_memtop_forms, (runner, results)))
    tasks.append(("over.com -m 64", test_memtop_over, (runner, results)))
    for mode in (["-C"], ["-C", "-8"], ["-C", "-Z"]):
        tasks.append(
            (
                "med.com %s" % " ".join(mode),
                test_checked_variant,
                (runner, results, mode),
            )
        )
    tasks.append(("med.com -C size", test_checked_size, (runner, results)))
    tasks.append(("big24.com -C @16K", test_checked_small, (runner, results)))
    tasks.append(("med.com -F floor", test_checked_floor, (runner, results)))
    tasks.append(("med.com -L floor", test_list_floor, (runner, results)))
    # the shipped split pair's own startup floor (the binaries under test)
    if which == "cpm":
        tasks.append(("startup floor", test_checked_startup, (runner, results)))

    def guarded(tag, fn, args):
        try:
            fn(*args)
        except Exception as e:  # noqa: BLE001 -- surface, do not propagate
            emit(results, tag, "FAIL", "-", "exception: %s" % str(e)[:60])

    if JOBS > 1:
        with ThreadPoolExecutor(max_workers=JOBS) as ex:
            for fut in [ex.submit(guarded, t, fn, a) for t, fn, a in tasks]:
                fut.result()
    else:
        for t, fn, a in tasks:
            guarded(t, fn, a)

    npass = sum(1 for r in results if r[1] == "PASS")
    nskip = sum(1 for r in results if r[1] == "SKIP")
    print(
        "\n  **** %d/%d passed%s ****"
        % (npass, len(results) - nskip, " (%d skipped)" % nskip if nskip else ""),
        flush=True,
    )
    return 0 if npass + nskip == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
