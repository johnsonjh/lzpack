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

TEST_TIMEOUT=5
export TEST_TIMEOUT

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
  if "${TIMEOUT_CMD:-timeout}" -p "${TEST_TIMEOUT}" sleep 0 > /dev/null 2>&1; then
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

################################################################################

printf '\n%s\n' "================== UNIT ==================="

# The autodetector "stop at the logical/LRBC length and not at physical EOF"
# logic cannot be reached through the CP/M 2.2 test emulations (no LRBC, so
# the logical length already equals physical EOF), so we build a small test
# program that #includes lzpack.c and calls the detector, once for streaming
# as is_z80_file(), and once as the in-RAM is_z80_image(), and we check that
# both stop at the correct logical length.  Needs only a working C compiler.

# shellcheck disable=SC2119
ut="$(mktemp 2> /dev/null || mktemp_lzpack)"

for udef in "-DLZPACK_STREAM" ""; do
  # shellcheck disable=SC2086,SC2248
  if "${CC:-cc}" ${udef} -I. -o "${ut}" tests/t_autoarch.c \
    && ${_TIMEOUT:-} "${ut}"; then
    :
  else
    rc=1
  fi
done

rm -f "${ut}"

################################################################################

printf '\n%s\n' "================= NATIVE =================="

TNYLPO="${TNYLPO}" python3 tests/harness.py native || rc=1

################################################################################

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 TNYLPO ==============="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-8080/lzpack.com" \
    python3 tests/harness.py cpm || rc=1
fi

################################################################################

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 CPMEMU ==============="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-8080/lzpack.com" \
    python3 tests/harness.py cpm2 || rc=1
fi

################################################################################

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 TNYLPO ==============="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-z80/lzpack.com" \
    python3 tests/harness.py cpm || rc=1
fi

################################################################################

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 CPMEMU ==============="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-z80/lzpack.com" \
    python3 tests/harness.py cpm2 || rc=1
fi

################################################################################

if [ -f "./cpm-86/lzpack.cmd" ] \
  && command -v "${EMU2}" > /dev/null 2>&1 \
  && "${EMU2}" -h 2>&1 | grep -q "DOS and CP/M-86 Emulator" \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "============== EMU2-CP/M-86 ==============="
  EMU2="${EMU2}" TNYLPO="${TNYLPO}" CPMCMD="./cpm-86/lzpack.cmd" \
    python3 tests/harness.py cpm86 || rc=1
fi

################################################################################

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
