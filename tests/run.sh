#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 6a9d6f12-58d5-11f1-b371-80ee73e9b8e7

set -eu

cd "$(dirname "$0")/.."

TNYLPO="${TNYLPO:-tnylpo}"
CPMEMU="${CPMEMU:-cpm}"
rc=0

printf '%s\n' "========== native =========="
TNYLPO="${TNYLPO}" $(command -v python3) tests/harness.py native || rc=1

if [ -f "./cpm-8080/lzpack.com" ] && [ -x "${TNYLPO}" ]; then
  printf '%s\n' "========== 8080 tnylpo =========="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm || rc=1
fi

if [ -f "./cpm-8080/lzpack.com" ] && [ -x "${CPMEMU}" ]; then
  printf '%s\n' "========== 8080 cpm =========="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm2 || rc=1
fi

if [ -f "./cpm-z80/lzpack.com" ] && [ -x "${TNYLPO}" ]; then
  printf '%s\n' "========== Z80 tnylpo =========="
  TNYLPO="${TNYLPO}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm || rc=1
fi

if [ -f "./cpm-z80/lzpack.com" ] && [ -x "${CPMEMU}" ]; then
  printf '%s\n' "========== Z80 cpm =========="
  CPMEMU="${CPMEMU}" CPMCOM="./cpm-8080/lzpack.com" \
    $(command -v python3) tests/harness.py cpm2 || rc=1
fi

exit "${rc}"
