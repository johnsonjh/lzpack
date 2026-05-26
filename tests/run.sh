#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 6a9d6f12-58d5-11f1-b371-80ee73e9b8e7

set -eu

cd "$(dirname "$0")/.."

TNYLPO="${TNYLPO:-${HOME}/src/tnylpo/tnylpo}"
CPMEMU="${CPMEMU:-${HOME}/src/cpm/cpm}"
rc=0

printf '%s\n' "========== native =========="
TNYLPO="${TNYLPO}" python3 tests/harness.py native || rc=1

if [ -f "lzpack.com" ] && [ -x "${TNYLPO}" ]; then
  printf '%s\n' "========== tnylpo =========="
  TNYLPO="${TNYLPO}" python3 tests/harness.py cpm || rc=1
fi

if [ -f "lzpack.com" ] && [ -x "${CPMEMU}" ]; then
  printf '%s\n' "========== cpm =========="
  CPMEMU="${CPMEMU}" python3 tests/harness.py cpm2 || rc=1
fi

exit "${rc}"
