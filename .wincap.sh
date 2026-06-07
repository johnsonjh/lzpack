#!/bin/sh
# .wincap.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: f8547e7a-1356-4e66-8281-8612103dff82

################################################################################

# Final CP/M-80 build check: report, for each shipped lzpack.com, the
# minimum TPA (in bytes, as set by the tnylpo -m patch) at which the
# streaming compressor reaches a 1K, 2K, 4K, and 8K match window, found by
# bisection under emulation.  The window a packer gets decides how tightly
# it packs, so these floors are the real capability of the shipped binaries
# on small systems.
#
# The Z80 packer (WINTPA_GATE_DIR) must reach its 8K window within
# WINTPA_Z80_8K bytes of TPA -- default 52,978, the measured full MSX-DOS
# fleet floor (the smallest fleet member, the Sanyo MPC-100, has a 52,992
# byte TPA, 14 bytes above it) -- and this script FAILS when it cannot, so
# a static-footprint regression that would demote real MSX machines to a
# 4K window breaks the build instead of slipping out.
#
# Needs tnylpo with the -m (TPA size) patch; prints a loud warning and
# exits 0 when either is unavailable, so plain builds still succeed.

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

cd "$(dirname "$0")"

################################################################################

# shellcheck disable=SC2065
test -f "./${0##*/}" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate script in current directory."
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

TNYLPO="${TNYLPO:-tnylpo}"

# Hard floor for the Z80 packer's 8K window (bytes of TPA); see the header.
WINTPA_Z80_8K="${WINTPA_Z80_8K:-52978}"

# The directory whose lzpack.com the WINTPA_Z80_8K gate applies to.  The -e
# (OUT_SUFFIX) builds trade window headroom for the larger parse block by
# design, so only the standard Z80 build is gated; the rest are reported.
WINTPA_GATE_DIR="${WINTPA_GATE_DIR:-cpm-z80}"

# tnylpo's -m patch accepts 4K..63K.
WINTPA_LO=4096
WINTPA_HI=64512

################################################################################

command -v "${TNYLPO}" > /dev/null 2>&1 || {
  printf '\n%s\n' ">> WARNING: '${TNYLPO}' not found; SKIPPING window checks!"
  printf '%s\n\n' \
    ">> WARNING: 8K-window-at-${WINTPA_Z80_8K}-byte-TPA floor is UNVERIFIED!"
  exit 0
}

################################################################################

export FIND_COMMAND_FATAL=1
find_command grep head mktemp sed tr wc

################################################################################

# Optional KiB equivalents under each byte column, used when bc(1) exists.

export FIND_COMMAND_FATAL=0

# shellcheck disable=SC2310
if find_command bc > /dev/null 2>&1; then
  HAVE_BC=1
else
  HAVE_BC=0
fi

export FIND_COMMAND_FATAL=1

################################################################################

# "(51.4K)" for a positive byte count, "-" for anything else (e.g. the -1
# an unreachable window reports).  Only called when HAVE_BC is 1.

kb_cell()
{
  case "$1" in
  '' | *[!0-9]*)
    printf '%s\n' "-"
    ;;
  *)
    # shellcheck disable=SC2312
    printf '(%sK)\n' "$(printf 'scale=1; %s / 1024\n' "$1" | bc)"
    ;;
  esac
}

################################################################################

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/wincap.XXXXXX" 2> /dev/null \
  || printf '%s\n' "${TMPDIR:-/tmp}/wincap.$$$$")
mkdir -p "${TMPD:?}"
trap 'rm -rf "${TMPD}"' EXIT INT TERM

################################################################################

# Deterministic, definitely-not-packed test input; its content does not
# matter (the window is sized before the first input byte is read), only
# that it is accepted, so anything over 48 bytes works.

head -c 2000 ./lzpack.c > "${TMPD}/in.bin"

################################################################################

# Run the packer under a t-byte TPA and print the window size it reports
# (empty output when it refuses to run or gets no window at all).  Marginal
# TPAs can hang the emulated packer outright (stack-graze zones; one burned
# hours before the LZ_STD_RESERVE margin was widened), so every probe is
# capped when a timeout command exists: a timed-out probe is a refusal.

TGUARD=""
command -v timeout > /dev/null 2>&1 && TGUARD="timeout 60"

probe()
{
  pr_t=$1
  rm -f "${TMPD:?}/in.pop" "${TMPD:?}/lztmp."* 2> /dev/null || :
  (
    # shellcheck disable=SC2086
    cd "${TMPD:?}" && ${TGUARD} "${TNYLPO}" -m "${pr_t}" -n lzpack.com in.bin
  ) 2> /dev/null \
    | tr -d '\r' \
    | sed -n 's/.* window \([0-9][0-9]*\) bytes.*/\1/p' \
    | head -1
}

################################################################################

# True when the tnylpo -m (TPA size) patch is present and active: -m 64K is
# just above the largest possible TPA, so a working patch must refuse it at
# startup; a stock tnylpo rejects -m as an unknown option instead.

tpa_patch_ok()
{
  (
    cd "${TMPD:?}" && "${TNYLPO}" -m 64K -n lzpack.com
  ) 2>&1 | grep -q 'out of range'
}

################################################################################

# Smallest TPA in [lo, hi] whose window reaches `want', or -1 when even hi
# cannot; the window grows monotonically with the TPA, so plain bisection.

bisect_win()
{
  bw_want=$1
  bw_lo=$2
  bw_hi=$3
  bw_w=$(probe "${bw_hi}")

  if [ "${bw_w:-0}" -lt "${bw_want}" ]; then
    printf '%s\n' "-1"
    return 0
  fi

  while [ "${bw_lo}" -lt "${bw_hi}" ]; do
    bw_mid=$(((bw_lo + bw_hi) / 2))
    bw_w=$(probe "${bw_mid}")

    if [ "${bw_w:-0}" -ge "${bw_want}" ]; then
      bw_hi=${bw_mid}
    else
      bw_lo=$((bw_mid + 1))
    fi
  done

  printf '%s\n' "${bw_lo}"
}

################################################################################

RC=0
probed=0

printf '\n%s\n' \
  ">>>>>>>>>>> Minimum TPA per compression window (bytes) <<<<<<<<<<<"
printf '\n%-14s %8s %8s %8s %8s  %s\n' \
  "BINARY" "1K" "2K" "4K" "8K" "STATUS"
printf '%-14s %8s %8s %8s %8s  %s\n' \
  "============" "======" "======" "======" "======" "======"

[ "$#" -gt 0 ] || set -- cpm-z80 cpm-8080

for d in "$@"; do
  com="${d}/lzpack.com"

  [ -f "${com}" ] || {
    printf '%-14s %8s %8s %8s %8s  %s\n' \
      "${d}" "-" "-" "-" "-" "SKIP (no ${com})"
    continue
  }

  cp -f "${com}" "${TMPD:?}/lzpack.com"

  if [ "${probed}" -eq 0 ]; then
    # shellcheck disable=SC2310
    tpa_patch_ok || {
      printf '\n%s\n' \
        ">> WARNING: tnylpo -m (TPA) patch missing; SKIPPING window checks!"
      printf '%s\n\n' \
        ">> WARNING: 8K-window-at-${WINTPA_Z80_8K}-byte-TPA floor UNVERIFIED!"
      exit 0
    }
    probed=1
  fi

  lo="${WINTPA_LO}"
  t1k='' t2k='' t4k='' t8k=''

  for want in 1024 2048 4096 8192; do
    t=$(bisect_win "${want}" "${lo}" "${WINTPA_HI}")

    if [ "${t}" -gt 0 ]; then
      lo="${t}"
    fi

    case "${want}" in
    1024) t1k="${t}" ;;
    2048) t2k="${t}" ;;
    4096) t4k="${t}" ;;
    *) t8k="${t}" ;;
    esac
  done

  if [ "${d}" = "${WINTPA_GATE_DIR}" ]; then
    if [ "${t8k}" -gt 0 ] && [ "${t8k}" -le "${WINTPA_Z80_8K}" ]; then
      verdict="PASS (8K @ ${t8k} <= ${WINTPA_Z80_8K})"
    else
      verdict="FAIL (8K window needs ${t8k} > ${WINTPA_Z80_8K})"
      RC=1
    fi
  else
    verdict="-"
  fi

  printf '%-14s %8s %8s %8s %8s  %s\n' \
    "${d}" "${t1k}" "${t2k}" "${t4k}" "${t8k}" "${verdict}"

  if [ "${HAVE_BC}" -eq 1 ]; then
    # shellcheck disable=SC2312
    printf '%-14s %8s %8s %8s %8s\n' \
      "" "$(kb_cell "${t1k}")" "$(kb_cell "${t2k}")" \
      "$(kb_cell "${t4k}")" "$(kb_cell "${t8k}")"
  fi
done

################################################################################

[ "${RC}" -eq 0 ] || {
  printf '\n%s\n' \
    "FATAL: the ${WINTPA_GATE_DIR} packer lost its 8K window at the MSX floor"
  printf '%s\n' \
    ">> Real MSX-DOS machines (TPA ~52,992) would drop to a 4K window;"
  printf '%s\n' \
    ">> shrink the packer's static footprint (or consciously override"
  printf '%s\n' \
    ">> WINTPA_Z80_8K) before shipping."
}

exit "${RC}"

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
