#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 6a9d6f12-58d5-11f1-b371-80ee73e9b8e7

set -eu

cd "$(dirname "$0")/.."

TNYLPO="${TNYLPO:-tnylpo}"
CPMEMU="${CPMEMU:-cpm}"
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

exit "${rc}"
