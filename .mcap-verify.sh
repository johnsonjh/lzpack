#!/bin/sh
# .mcap-verify.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# 90e0efb4-60c3-11f1-83ad-80ee73e9b8e7

################################################################################

set -eu

cd "$(dirname "$0")"

################################################################################

[ -x ./lzpack ] || {
  printf '%s\n' "ERROR: host ./lzpack missing; run 'make' first."
  exit 1
}

for com in cpm-z80/lzpack.com cpm-z80/lzunpack.com cpm-z80/stubasm.com \
  cpm-8080/lzpack.com cpm-8080/lzunpack.com cpm-8080/stubasm.com; do
  [ -f "${com}" ] || {
    printf '%s\n' "ERROR: ${com} missing; run 'make cpm' first."
    exit 1
  }
done

################################################################################

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/mcap-verify.XXXXXX")
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

  ./lzpack -e "$@" -m "${hi}" -o "${TMPD}/t.p" "${bs_f}" > /dev/null 2>&1 || {
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

    if [ "${prog}" = "stubasm" ]; then
      minc="-"
      exp=$((min + 128))
      bind=${min}
    else
      minc=$(bisect "${raw}" -C)
      exp=$((min + 176))
      bind=${minc}
    fi

    if [ "${min}" -lt 0 ] || [ "${bind}" -lt 0 ] || [ "${rec}" -lt "${bind}" ]; then
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

      SUGGEST="${SUGGEST}  ${name}: ${rec} -> ${exp}"
    fi

    printf '%-14s %8s %8s %9s %9s %7s  %s\n' \
      "${arch}/${prog}" "${min}" "${minc}" "${exp}" "${rec}" \
      "$((rec - bind))" "${verdict}"
  done
done

################################################################################

[ -n "${SUGGEST}" ] && {
  printf '%s\n' "" ">> Update .build-cpm.sh:"
  printf '%s' "${SUGGEST}"
}

exit "${RC}"
