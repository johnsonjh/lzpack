#!/bin/sh
# tests/run.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 6a9d6f12-58d5-11f1-b371-80ee73e9b8e7

################################################################################

if [ -n "${ZSH_VERSION-}" ]; then
  emulate sh
  setopt sh_word_split
fi

################################################################################

test -d "/opt/freeware/bin" && {
  export PATH="/opt/freeware/bin:${PATH:-}"
}

################################################################################

test -d "/usr/pkg/gnu/bin" && {
  export PATH="${PATH:-}:/usr/pkg/gnu/bin"
}

################################################################################

set -eu

################################################################################

cd "$(dirname "$0")/.."

################################################################################

# shellcheck disable=SC2065
test -f "./tests/${0##*/}" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate script in ./tests/ directory."
  exit 1
}

################################################################################

# shellcheck disable=SC2065
test -f "./.common.sh" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate .common.sh in current directory."
  exit 1
}

################################################################################

export CPE1704TKS=1

# shellcheck disable=SC1091
. ./.common.sh

################################################################################

TEST_TIMEOUT="${TEST_TIMEOUT:-360}"

export TEST_TIMEOUT

################################################################################

# Parallel harness workers: every harness task is an independent
# pack/extract in a private scratch dir, so it scales to the CPU count.
# LZ_TEST_JOBS=1 forces the old serial order.

if [ -z "${LZ_TEST_JOBS:-}" ]; then
  LZ_TEST_JOBS="$(nproc 2> /dev/null || getconf _NPROCESSORS_ONLN \
    2> /dev/null || sysctl -n hw.ncpu 2> /dev/null || printf '%s\n' 1)"
fi

case ${LZ_TEST_JOBS} in
'' | *[!0-9]*)
  LZ_TEST_JOBS=1
  ;;
*) : ;;
esac

export LZ_TEST_JOBS

################################################################################

# Wall-clock section timing ('date +%s' is non-POSIX but ubiquitous; where
# it is unsupported the sections simply report 0s).

now_s()
{
  _n="$(date +%s 2> /dev/null)" || _n=
  case ${_n} in
  '' | *[!0-9]*)
    _n=0
    ;;
  *) : ;;
  esac
  printf '%s\n' "${_n}"
}

SUITE_T0="$(now_s)"

sec_begin()
{
  SEC_T0="$(now_s)"
}

sec_end()
{
  # shellcheck disable=SC2312
  printf '%s\n' ">> section time: $(($(now_s) - SEC_T0))s"
}

################################################################################

CC="$(command -v cc 2> /dev/null || command -v "${GCC_CMD:-gcc}" 2> /dev/null \
  || command -v "${CLANG_CMD:-clang}" 2> /dev/null || printf '%s\n' cc)"

export CC

################################################################################

export FIND_COMMAND_FATAL=1

find_command "${AWK:-awk}" "${CC:-cc}" grep mkdir python3 rm rmdir sleep

TNYLPO="${TNYLPO:-tnylpo}"
CPMEMU="${CPMEMU:-cpm}"
EMU2="${EMU2:-emu2}"
rc=0

export FIND_COMMAND_FATAL=0

# shellcheck disable=SC2310
if out=$(
  find_command "${CPMEMU:-cpm}" "${EMU2:-emu2}" "${TNYLPO:-tnylpo}" \
    "${TIMEOUT_CMD:-timeout}" 2>&1
); then
  status=0
else
  status="$?"
fi

width="$(detect_width)"

# shellcheck disable=SC2310
printf '%s\n' "${out:-}" \
  | wrap "${width:?}"

unset NEED_PAUSE

if [ "${status:?}" -ne 0 ]; then
  NEED_PAUSE=1
fi

################################################################################

case ${OVERRIDE_PAUSE:-} in
'' | *[!0-9]*)
  unset OVERRIDE_PAUSE
  ;;
*) : ;;
esac

test "${NEED_PAUSE:-0}" -ne 1 || {
  printf '%s\n' \
    "Some checks will be skipped! [pausing ${OVERRIDE_PAUSE:-10}s]" \
    | wrap "${width:?}"
  sleep "${OVERRIDE_PAUSE:-10}"
}

################################################################################

if command -v "${TIMEOUT_CMD:-timeout}" > /dev/null 2>&1; then
  # shellcheck disable=SC2086
  if "${TIMEOUT_CMD:-timeout}" -p "${TEST_TIMEOUT}" \
    sleep 0 > /dev/null 2>&1; then
    _TIMEOUT="${TIMEOUT_CMD:-timeout} -p ${TEST_TIMEOUT}"
  else
    _TIMEOUT="${TIMEOUT_CMD:-timeout} ${TEST_TIMEOUT}"
  fi
else
  _TIMEOUT=""
fi

################################################################################

printf '\n%s' ">> Starting tests; "
printf '%s\n' \
  "test timeout is ${TEST_TIMEOUT} seconds (override via TEST_TIMEOUT)."
printf '%s\n' \
  ">> Parallel harness jobs: ${LZ_TEST_JOBS} (override via LZ_TEST_JOBS)."

################################################################################

# Generate any missing test corpus up front: harness.py would lazily rebuild
# it, but doing it once here keeps parallel consumers from ever racing on it.

for cf in tiny small med big16 big24 big46 incomp over z80 \
  ckzero cktext ckrep; do
  test -f "./tests/corpus/${cf}.com" || {
    python3 ./tests/gen.py
    break
  }
done

################################################################################

printf '\n%s\n' "================== UNIT ==================="
sec_begin

# The autodetector "stop at the logical/LRBC length and not at physical EOF"
# logic cannot be reached through the CP/M 2.2 test emulations (no LRBC, so
# the logical length already equals physical EOF), so we build a small test
# program that #includes lzpack.c and calls the detector, once for streaming
# as is_z80_file(), and once as the in-RAM is_z80_image(), and we check that
# both stop at the correct logical length.  Needs only a working C compiler.
# t_memtop.c likewise unit-tests the -m (MEMTOP) value parser in both builds.

# shellcheck disable=SC2119
ut="$(mktemp 2> /dev/null || mktemp_lzpack)"

for usrc in tests/t_autoarch.c tests/t_memtop.c; do
  for udef in "-DLZPACK_STREAM" ""; do
    # shellcheck disable=SC2086,SC2248
    if "${CC:-cc}" ${udef} -I. -o "${ut}" "${usrc}" \
      && ${_TIMEOUT:-} "${ut}"; then
      :
    else
      rc=1
    fi
  done
done

rm -f "${ut}"
sec_end

################################################################################

printf '\n%s\n' "================ STREAM ==================="
sec_begin

# Emulator-free check of the streaming (-DLZPACK_STREAM) build's in-place -R.
# That build decodes into a single overlapping buffer (the payload sits at the
# top of the output buffer and is consumed as the output grows up into it),
# unlike the in-RAM decoder the NATIVE section exercises, so round-trip it
# against the corpus byte-for-byte here.  Needs only a C compiler.

# shellcheck disable=SC2119
st="$(mktemp 2> /dev/null || mktemp_lzpack)"
sdir="$(mktemp -d 2> /dev/null || printf '%s\n' "${TMPDIR:-/tmp}/lzst.$$")"
mkdir -p "${sdir}"

# shellcheck disable=SC2086,SC2248
if "${CC:-cc}" -DLZPACK_STREAM -DHSZ=1024 -I. -o "${st}" lzpack.c; then
  s_ok=0
  s_run=0
  for f in tests/corpus/*.com; do
    b="${f##*/}"
    b="${b%.com}"
    # Compress with the streaming build itself; "skip" corpus files (too small
    # or incompressible) produce no .pop and are simply not round-tripped.
    "${st}" -O "${sdir}/${b}.pop" "${f}" > /dev/null 2>&1 || :
    test -f "${sdir}/${b}.pop" || continue
    s_run=$((s_run + 1))
    "${st}" -R -O "${sdir}/${b}.unp" "${sdir}/${b}.pop" > /dev/null 2>&1 || :
    if cmp -s "${f}" "${sdir}/${b}.unp"; then
      s_ok=$((s_ok + 1))
    else
      printf '  [FAIL] stream in-place -R %s\n' "${b}"
      rc=1
    fi
  done
  printf '  stream in-place -R round-tripped %d/%d corpus files\n' \
    "${s_ok}" "${s_run}"
else
  printf '  [FAIL] streaming build (-DLZPACK_STREAM) failed to compile\n'
  rc=1
fi

rm -rf "${sdir}"
rm -f "${st}"
sec_end

################################################################################

printf '\n%s\n' "================= NATIVE =================="
sec_begin

TNYLPO="${TNYLPO}" python3 tests/harness.py native || rc=1
sec_end

################################################################################

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 TNYLPO ==============="
  sec_begin
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-8080/lzpack.com" \
    python3 tests/harness.py cpm || rc=1
  sec_end
fi

################################################################################

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 CPMEMU ==============="
  sec_begin
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-8080/lzpack.com" \
    python3 tests/harness.py cpm2 || rc=1
  sec_end
fi

################################################################################

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 TNYLPO ==============="
  sec_begin
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-z80/lzpack.com" \
    python3 tests/harness.py cpm || rc=1
  sec_end
fi

################################################################################

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 CPMEMU ==============="
  sec_begin
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-z80/lzpack.com" \
    python3 tests/harness.py cpm2 || rc=1
  sec_end
fi

################################################################################

if [ -f "./cpm-86/lzpack.cmd" ] \
  && command -v "${EMU2}" > /dev/null 2>&1 \
  && "${EMU2}" -h 2>&1 | grep -q "DOS and CP/M-86 Emulator" \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "============== EMU2-CP/M-86 ==============="
  sec_begin
  EMU2="${EMU2}" TNYLPO="${TNYLPO}" CPMCMD="./cpm-86/lzpack.cmd" \
    python3 tests/harness.py cpm86 || rc=1
  sec_end
fi

################################################################################

# shellcheck disable=SC2312
printf '\n%s\n' ">> Total suite time: $(($(now_s) - SUITE_T0))s"

if [ "${rc}" != 0 ]; then
  printf '\n%s\n\n' ">> Testing suite completed BUT SOME TESTS FAILED!!!"
else
  printf '\n%s\n\n' ">> Testing suite completed successfully."
fi

################################################################################

exit "${rc}"

################################################################################

# Local Variables:
# mode: shell
# indent-tabs-mode: nil
# sh-basic-offset: 2
# tab-width: 2
# fill-column: 80
# eval: (add-hook 'before-save-hook 'untabify nil t)
# eval: (setq-local display-fill-column-indicator-column 80)
# eval: (display-fill-column-indicator-mode 1)
# End:

################################################################################
# vim: set ft=sh ts=2 sw=2 tw=0 ai expandtab cc=80 :
################################################################################
