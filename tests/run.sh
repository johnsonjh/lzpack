#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 6a9d6f12-58d5-11f1-b371-80ee73e9b8e7

if [ -n "${ZSH_VERSION-}" ]; then
  emulate sh
  setopt sh_word_split
fi

test -d "/opt/freeware/bin" && {
  export PATH="/opt/freeware/bin:${PATH:-}"
}

test -d "/usr/pkg/gnu/bin" && {
  export PATH="${PATH:-}:/usr/pkg/gnu/bin"
}

set -eu

cd "$(dirname "$0")/.."

# shellcheck disable=SC2065
test -f "./tests/${0##*/}" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate script in ./tests/ directory."
  exit 1
}

# shellcheck disable=SC2065
test -f "./.common.sh" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate .common.sh in current directory."
  exit 1
}

export CPE1704TKS=1

# shellcheck disable=SC1091
. ./.common.sh

export FIND_COMMAND_FATAL=1
find_command awk grep make python3 sleep ${TNYLPO:-tnylpo} uname

export FIND_COMMAND_FATAL=0
find_command "${CPMEMU:-cpm}" "${EMU2:-emu2}" || :

TNYLPO="${TNYLPO:-tnylpo}"
CPMEMU="${CPMEMU:-cpm}"
EMU2="${EMU2:-emu2}"
rc=0

printf '\n%s\n' "================= NATIVE =================="

TNYLPO="${TNYLPO}" $(command -v python3) tests/harness.py native || rc=1

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 TNYLPO ==============="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm || rc=1
fi

if [ -f "./cpm-8080/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "=============== 8080 CPMEMU ==============="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm2 || rc=1
fi

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 TNYLPO ==============="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-z80/lzpack.com" \
    $(command -v python3) tests/harness.py cpm || rc=1
fi

if [ -f "./cpm-z80/lzpack.com" ] \
  && command -v "${CPMEMU}" > /dev/null 2>&1; then
  printf '\n%s\n' "================ Z80 CPMEMU ==============="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-z80/lzpack.com" \
    $(command -v python3) tests/harness.py cpm2 || rc=1
fi

if [ -f "./cpm-86/lzpack.cmd" ] \
  && command -v "${EMU2}" > /dev/null 2>&1 \
  && "${EMU2}" -h 2>&1 | grep -q "DOS and CP/M-86 Emulator" \
  && command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '\n%s\n' "============== EMU2-CP/M-86 ==============="
  EMU2="${EMU2}" TNYLPO="${TNYLPO}" CPMCMD="./cpm-86/lzpack.cmd" \
    $(command -v python3) tests/harness.py cpm86 || rc=1
fi

exit "${rc}"
