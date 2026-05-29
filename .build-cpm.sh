#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: c6be098c-58d5-11f1-ad23-80ee73e9b8e7
# shellcheck disable=SC1007

# Build the CP/M-80 (z88dk) version of lzpack (and stubasm).
#
# Builds are supported using either a locally installed z88dk or with Docker.
#
# The CP/M build uses the streaming compressor (-DLZPACK_STREAM): the input is
# read from disk through a sliding window (sized dynamically at runtime to the
# largest the host's heap allows) and the payload is staged in a temp file, so
# working RAM is fixed and large executables pack on 8080 48K TPA CP/M systems.
#
# Build backend (CPM_BACKEND environment variable):
#   auto    (default) use the local 'zcc' if found in PATH, otherwise fall
#           back to attempting Docker.  A local install must have the full
#           and very recent z88dk toolchain including having ZCCCFG set up.
#   local   require a local 'zcc'; fail if it is not in PATH.
#   docker  always build using the z88dk Docker image, ignore local 'zcc'.
#
# Tunables (override via the environment):
#   CLIB     z88dk library: 'ixiy' (Z80, default) or '8080' (8080/8085).
#   HSZ      hash-table entries (power of two).
#   MZXFILE  maximum input size accepted (the 65535-byte header limit).
#   STACKSZ  stack reserve; the rest of RAM becomes the heap (and the window).
#   PACK     1 (default) = ship .COMs packed with the host -e self-extractor.
#   ZCC      local z88dk compiler driver (default 'zcc'; Docker mode ignores it).
#   DOCKER   docker command (e.g. "sudo docker" if you are not in the group).
#   IMAGE    z88dk image to use (Docker mode only).
#   TNYLPO   path to the tnylpo binary for the optional smoke test.

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

printf '\n%s\n' ">>>>>>>>>>> Starting CP/M-80 build <<<<<<<<<<<"

CPM_BACKEND="${CPM_BACKEND:-auto}"
ZCC="${ZCC:-zcc}"
IMAGE="${IMAGE:-z88dk/z88dk:latest}"
DOCKER="${DOCKER:-docker}"
TNYLPO="${TNYLPO:-tnylpo}"
ARCHS="${ARCHS:-ixiy 8080}"
HSZ="${HSZ:-1024}"
MZXFILE="${MZXFILE:-65535L}"

# stack reserve; the rest of RAM becomes heap, so
# the dynamic window grows as large as the TPA allows
STACKSZ="${STACKSZ:-2048}"

# 1 = pack the .COMs with the host optimal (-e)
# self-extractor; 0 = leave them raw
PACK="${PACK:-1}"

# 48K-system memory ceiling (= MEMTOP); the build
# fails if a tool's runtime footprint won't fit
TPA48="${TPA48:-0xBDFF}"

# smallest window lzpack will fall back to (matches
# the WINMIN in lzpack.c)
WINMIN="${WINMIN:-1024}"

cd "$(dirname "$0")"

# shellcheck disable=SC2065
test -f "./${0##*/}" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate script in current directory."
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
find_command "${MAKE:-make}" awk chown cp head ls mv pwd rm sed wc

export FIND_COMMAND_FATAL=0
find_command "${TNYLPO:?}" || :

here="$(pwd -P)"

# Choose a build backend.
zccpath=
case "${CPM_BACKEND}" in
auto)
  if zccpath="$(command -v "${ZCC}" 2> /dev/null)"; then
    CPM_BACKEND=local
  else
    export FIND_COMMAND_FATAL=1
    find_command "${DOCKER}"
    export FIND_COMMAND_FATAL=0
    find_command sudo || :
    CPM_BACKEND=docker
  fi
  ;;
local)
  if ! zccpath="$(command -v "${ZCC}" 2> /dev/null)"; then
    printf '\n%s\n' "FATAL: CPM_BACKEND='local' but '${ZCC}' is not in PATH (override with ZCC variable)" >&2
    printf '\n%s\n\n' ">> Dockerized build mode also available: try '${MAKE:-make} cpm-docker'"
    exit 1
  fi
  ;;
docker) : ;;
*)
  printf '\n%s\n\n' "FATAL: CPM_BACKEND must be 'auto', 'local', or 'docker' (got '${CPM_BACKEND}')" >&2
  exit 1
  ;;
esac

if [ "${CPM_BACKEND}" = docker ]; then
  # Fall back to sudo if the user cannot reach the Docker daemon directly.
  if ! ${DOCKER} info > /dev/null 2>&1; then
    # shellcheck disable=SC2086
    if command -v sudo > /dev/null 2>&1 && sudo ${DOCKER} info > /dev/null 2>&1; then
      printf '\n%s\n' "NOTE: Trying 'sudo docker' - the current user could not reach the daemon."
      DOCKER="sudo ${DOCKER}"
    else
      printf '\n%s\n' "FATAL: Failed to talk to the Docker daemon; set DOCKER variable (or join the 'docker' group)" >&2
      printf '\n%s\n\n' ">> Standard build mode also available: try '${MAKE:-make} cpm-local'"
      exit 1
    fi
  fi
  printf '\n%s\n' ">>>>>>>>>>> Build mode: Docker (zcc from '${IMAGE}') <<<<<<<<<<<"
  printf '\n%s\n' ">> Standard build mode also available: try '${MAKE:-make} cpm-local'"
else
  printf '\n%s\n' ">>>>>>>>>>> Build mode: local (zcc from '${zccpath}') <<<<<<<<<<<"
  printf '\n%s\n' ">> Dockerized build mode also available: try '${MAKE:-make} cpm-docker'"
fi

# Run a z88dk tool.  Call sites pass 'zcc' as the first word; in local mode we
# drop it and invoke the resolved local driver (${ZCC}), in Docker mode we run
# the whole command inside the image with the source tree bind-mounted at /src.
run_zcc()
{
  if [ "${CPM_BACKEND}" = docker ]; then
    ${DOCKER} run --rm -v "${here}":/src -w /src "${IMAGE}" "$@"
  else
    shift
    "${ZCC}" "$@"
  fi
}

# z88dk's +cpm appmake writes the (zeroed) BSS into the .COM, but the CP/M CRT
# zeroes BSS at startup, so the file only needs CODE+DATA -- everything below
# __BSS_head.  Trimming it shrinks the image on disk without changing what runs.
trim_bss()
{
  bss= keep= cur=
  [ -f "$2" ] || return 0
  bss=$(sed -n 's/^__BSS_head *= *\$\([0-9A-Fa-f]*\).*/\1/p' "$2" | head -1)
  [ -n "${bss}" ] || return 0
  keep=$((0x${bss} - 0x100))
  cur=$(wc -c < "$1")
  if [ "${keep}" -gt 0 ] && [ "${keep}" -lt "${cur}" ]; then
    head -c "${keep}" "$1" > "$1.t" && mv -f "$1.t" "$1"
    printf '%s\n' ">> trimmed $1 BSS padding: ${cur} -> ${keep} bytes"
  fi
}

# Pack a CP/M-80 .COM into a smaller self-extracting one with the host's optimal
# size (-e) compressor.  Leaves the file on any failures (i.e., incompressible).
pack()
{
  before= after=
  [ "${PACK}" = 0 ] && return 0
  [ -x ./lzpack ] || {
    printf '%s\n' ">> host ./lzpack missing; not packing $1"
    return 0
  }
  before=$(wc -c < "$1")
  if ./lzpack -e -o "$1.p" "$1" > /dev/null 2>&1 && [ -f "$1.p" ]; then
    after=$(wc -c < "$1.p")
    mv -f "$1.p" "$1"
    printf '%s\n' ">> packed $1 (host -e self-extractor): ${before} -> ${after} bytes"
  else
    rm -f "$1.p"
    printf '%s\n' ">> $1 did not pack smaller; left raw"
  fi
}

# Verify a tool's *runtime* footprint fits a 48K system.  The map's __BSS_END is
# where static storage ends; on top of that the program needs its smallest
# window (3*WINMIN), stdio buffers, and the stack.  Fails the build if that peak
# would run past the 48K ceiling -- catching any future z88dk size regression.
check_48k()
{
  end= peak= ceil= avail= win=
  if [ ! -f "$2" ]; then
    printf '%s\n' ">> [$1] no map ($2); cannot size-check"
    return 0
  fi
  end=$(sed -n 's/^__BSS_END_head *= *\$\([0-9A-Fa-f]*\).*/\1/p' "$2" | head -1)
  if [ -z "${end}" ]; then
    printf '%s\n' ">> [$1] no __BSS_END in map; skip check"
    return 0
  fi
  # runtime peak = end of static storage + extra heap (e.g. lzpack's smallest
  # window) + stdio buffers + stack.
  peak=$((0x${end} + $3 + 2048 + STACKSZ))
  ceil=$((TPA48))
  if [ "${peak}" -gt "${ceil}" ]; then
    # shellcheck disable=SC2312
    printf ">> [%s] %s: runtime peak 0x%04X exceeds 48K ceiling 0x%04X\n" \
      "$1" "$([ "$4" = 1 ] && printf '%s\n' FAIL || printf '%s\n' WARN)" "${peak}" "${ceil}"
    [ "$4" = 1 ] && FITFAIL=1
  elif [ "$3" -gt 0 ]; then
    avail=$((ceil - 0x${end} - 2048 - STACKSZ))
    win=$((avail / 3))
    printf ">> [%s] fits 48K: peak 0x%04X <= 0x%04X (room for ~%d-byte window)\n" \
      "$1" "${peak}" "${ceil}" "${win}"
  else
    printf ">> [%s] fits 48K: peak 0x%04X <= 0x%04X\n" "$1" "${peak}" "${ceil}"
  fi
}

# Build both tools with requested z88dk C library into "$2", using -m to emit
# the maps used for 48K limit check and BSS trimming.  For lzpack builds, set
# CRT_STACK_SIZE (and not -DAMALLOC, which caps the heap at 3/4 of free RAM!)
# which leaves all of the RAM (except a small stack) for the heap so that the
# (runtime auto-sized) compression window will be as large as the TPA allows.
build_arch()
{
  clib="$1"
  out="$2"
  [ "${out}" = "." ] || mkdir -p "${out}"
  lc="${out}/lzpack.com"
  lm="${out}/lzpack.map"
  sc="${out}/stubasm.com"
  sm="${out}/stubasm.map"
  printf '%s\n' ">> [${clib}] building ${lc}  (HSZ=${HSZ} MZXFILE=${MZXFILE} STACK=${STACKSZ})"
  run_zcc zcc +cpm -O3 --opt-code-size -m lzpack.c -clib="${clib}" -o "${lc}" \
    -DLZPACK_STREAM=1 "-DHSZ=${HSZ}" "-DMZXFILE=${MZXFILE}" \
    "-pragma-define:CRT_STACK_SIZE=${STACKSZ}"
  printf '%s\n' ">> [${clib}] building ${sc}"
  run_zcc zcc +cpm -O3 --opt-code-size -m stubasm.c -clib="${clib}" -o "${sc}" \
    -DAMALLOC -DMAXSYM=96 -DMAXREF=96 -DMAXCODE=768
  # shellcheck disable=SC2249
  case "${DOCKER}" in
  sudo*) sudo chown "$(id -u):$(id -g)" "${lc}" "${lm}" "${sc}" "${sm}" 2> /dev/null || : ;;
  esac
  printf '%s\n' ""
  check_48k "${clib} lzpack" "${lm}" "$((3 * WINMIN))" 1
  check_48k "${clib} stubasm" "${sm}" 0 0
  printf '%s\n' ""
  trim_bss "${lc}" "${lm}"
  trim_bss "${sc}" "${sm}"
  printf '%s\n' ""
  pack "${lc}"
  pack "${sc}"
  printf '%s\n' ""
  ls -l "${lc}" "${sc}"
  printf '%s\n' ""
}

# Host build: Stub tables (cs8080.h, csz80.h) and the native lzpack used for packing
printf '%s\n' ""
printf '%s\n\n' ">> building native host lzpack"
"${MAKE:-make}" lzpack
printf '%s\n' ""

# Build each requested architecture
FITFAIL=0
for arch in ${ARCHS}; do
  case "${arch}" in
  ixiy) build_arch ixiy "cpm-z80" ;;
  8080) build_arch 8080 "cpm-8080" ;;
  *) build_arch "${arch}" "cpm-${arch}" ;;
  esac
done

if [ "${FITFAIL}" = 1 ]; then
  printf '\n%s\n\n' "FATAL: a tool's runtime footprint will not fit a 48K CP/M-80 system" >&2
  exit 1
fi

printf '%s\n\n' ">>>>>>>>>>> Finished CP/M-80 build <<<<<<<<<<<"

# Optional smoke test: Confirm the .COM loads and prints its usage banner
if command -v "${TNYLPO}" > /dev/null 2>&1; then
  printf '%s\n\n' ">> tnylpo z80 smoke test"
  "${TNYLPO}" -n ./cpm-z80/lzpack.com -v || :
  printf '\n%s\n\n' ">> tnylpo 8080 smoke test"
  "${TNYLPO}" -n ./cpm-8080/lzpack.com -v || :
  printf '\n%s\n' ">> for full round-trip self-extraction tests: '${MAKE:-make} test'"
else
  printf '\n%s\n' ">> tnylpo not found (set TNYLPO=...); skipping smoke test"
fi
