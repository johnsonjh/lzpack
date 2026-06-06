#!/bin/sh
# .mcap-verify.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# 90e0efb4-60c3-11f1-83ad-80ee73e9b8e7

################################################################################

# For use by the maintainer only - not the general public.
# This script requires GNU coreutils `du` to work correctly.

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

[ -x ./lzpack ] || {
  printf '%s\n' "ERROR: './lzpack' missing; run 'make' first."
  exit 1
}

################################################################################

for com in ./cpm-z80/lzpack.com ./cpm-z80/lzunpack.com ./cpm-z80/stubasm.com \
  ./cpm-8080/lzpack.com ./cpm-8080/lzunpack.com ./cpm-8080/stubasm.com; do
  [ -f "${com}" ] || {
    printf '%s\n' "ERROR: '${com}' missing; run 'make cpm' first."
    exit 1
  }
done

################################################################################

export FIND_COMMAND_FATAL=1
find_command ./.build-cpm.sh ./lzpack mktemp sed tr

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

# "(22.6K)" for a positive byte count, "-" for anything else (e.g. the -1
# a failed bisect reports).  Only called when HAVE_BC is 1.

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

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/mcap-verify.XXXXXX" 2> /dev/null \
  || printf '%s\n' "${TMPDIR:-/tmp}/mcap-verify.$$$$")
mkdir -p "${TMPD:?}" > /dev/null 2>&1 || :
trap 'rm -rf "${TMPD}"' EXIT INT TERM

################################################################################

recorded()
{
  eval "rec_env=\${$1:-}"
  rec_def=$(sed -n "/^$1=/s/.*:-\\([0-9][0-9]*\\)}.*/\\1/p" .build-cpm.sh)
  printf '%s\n' "${rec_env:-${rec_def}}"
}

################################################################################

bisect()
{
  bs_f=$1
  shift
  lo=256 hi=65535

  ./lzpack -e "$@" -m "${hi}" -o "${TMPD}/t.p" "${bs_f}" > /dev/null 2>&1 \
    || {
      printf '%s\n' "-1"
      return 0
    }

  while [ "${lo}" -lt "${hi}" ]; do
    mid=$(((lo + hi) / 2))

    if ./lzpack -e "$@" -m "${mid}" -o "${TMPD}/t.p" "${bs_f}" \
      > /dev/null 2>&1; then
      hi=${mid}
    else
      lo=$((mid + 1))
    fi
  done

  printf '%s\n' "${lo}"
}

################################################################################

RC=0
SUGGEST=

printf '%-14s %8s %8s %9s %9s %7s  %s\n' \
  "BINARY" "MIN" "MIN-C" "EXPECTED" "RECORDED" "SLACK" "STATUS"
printf '%-14s %8s %8s %9s %9s %7s  %s\n' \
  "============" "=====" "=====" "========" "========" "=====" "======"

for arch in z80 8080; do
  for prog in lzpack lzunpack stubasm; do
    raw="${TMPD}/${arch}-${prog}.raw"
    ./lzpack -R -o "${raw}" "cpm-${arch}/${prog}.com" > /dev/null 2>&1 || {
      printf '%s\n' "ERROR: cannot restore cpm-${arch}/${prog}.com"
      exit 1
    }

    name="MCAP_$(printf '%s' "${arch}" | tr '[:lower:]' '[:upper:]')"
    name="${name}_$(printf '%s' "${prog}" | tr '[:lower:]' '[:upper:]')"
    rec=$(recorded "${name}")

    min=$(bisect "${raw}")

    minc=$(bisect "${raw}" -C)
    exp=$((min + 176))
    bind=${minc}

    if [ "${min}" -lt 0 ] \
      || [ "${bind}" -lt 0 ] \
      || [ "${rec}" -lt "${bind}" ]; then
      verdict="FAIL (build would refuse)"
      RC=1
    elif [ "${rec}" -eq "${exp}" ]; then
      verdict="OK"
    else
      d=$((rec - exp))

      if [ "${d}" -gt 0 ]; then verdict="LOOSE (+${d})"; else
        verdict="TIGHT (${d})"
      fi

      [ "${RC}" -eq 0 ] && RC=2

      SUGGEST="${SUGGEST}  ${name}: ${rec} -> ${exp}\$"
    fi

    printf '%-14s %8s %8s %9s %9s %7s  %s\n' \
      "${arch}/${prog}" "${min}" "${minc}" "${exp}" "${rec}" \
      "$((rec - bind))" "${verdict}"

    if [ "${HAVE_BC}" -eq 1 ]; then
      # shellcheck disable=SC2312
      printf '%-14s %8s %8s %9s %9s\n' \
        "" "$(kb_cell "${min}")" "$(kb_cell "${minc}")" \
        "$(kb_cell "${exp}")" "$(kb_cell "${rec}")"
    fi
  done
done

################################################################################

[ -n "${SUGGEST}" ] && {
  printf '%s\n\n' "" ">> Update .build-cpm.sh:"
  printf '%s\n' "${SUGGEST}" | sed -e 's|\$|\n|g' -e 's|:|\t|g'
}

exit "${RC}"
