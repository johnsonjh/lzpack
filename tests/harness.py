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
import os, re, sys, shutil, subprocess, glob, tempfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.join(ROOT, "corpus")
PROJECT = os.path.dirname(ROOT)

# Emulators: a name on PATH by default; set TNYLPO/CPMEMU/EMU2 to a path otherwise.
TNYLPO = os.environ.get("TNYLPO", "tnylpo")
CPMEMU = os.environ.get("CPMEMU", "cpm")
EMU2 = os.environ.get("EMU2", "emu2")
NATIVE = os.path.join(PROJECT, "lzpack")
CPMCOM = os.environ.get("CPMCOM", os.path.join(PROJECT, "cpm-z80", "lzpack.com"))
CPMCMD = os.environ.get("CPMCMD", os.path.join(PROJECT, "cpm-86", "lzpack.cmd"))

# Timeout for each test step (compression, extraction, round-trip)
TEST_TIMEOUT = int(os.environ.get("TEST_TIMEOUT", 5))

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


def run_tnylpo(workdir, comname, args=None):
    """Run a CP/M .COM under tnylpo in workdir; return decoded stdout."""
    cmd = [TNYLPO, "-n", comname] + (args or [])
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
    # the command .com and the data files on disk are lowercase.
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
    piped stdout would otherwise stay fully buffered until the run ends."""
    results.append((tag, status, size, note))
    print("  [%s] %-22s size=%-11s %s" % (status, tag, size, note), flush=True)


def parse_arch(log):
    """Return the stub kind ('8080' or 'Z80') from lzpack's '[..]' verbose tag,
    or '?' if absent.  The tag is '[8080]'/'[Z80]' when forced by -8/-Z and
    '[8080 auto]'/'[Z80 auto]' when chosen by the autodetector."""
    m = re.search(r"\[(8080|Z80)( auto)?\]", log)
    return m.group(1) if m else "?"


def test_file(runner, fname, marker, expect, expect_arch, results):
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

    for mode in MODES:
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
                continue

            if pop is None:
                emit(
                    results,
                    tag,
                    "FAIL",
                    "-",
                    "no .pop produced; log=" + log.strip()[:80],
                )
                continue
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
            else:
                want_arch = expect_arch
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
    print("===========================================\n", flush=True)
    results = []
    for fname, marker, expect, expect_arch in CORPUS_FILES:
        test_file(runner, fname, marker, expect, expect_arch, results)
    npass = sum(1 for r in results if r[1] == "PASS")
    print("\n  **** %d/%d passed ****" % (npass, len(results)), flush=True)
    return 0 if npass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
