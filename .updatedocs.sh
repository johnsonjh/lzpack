#!/bin/sh
# .updatedocs.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 4ba45bfa-5b11-11f1-88d0-80ee73e9b8e7

# For use by the maintainer only - not the general public.
# It requires the GNU coreutils `du` to work correctly.

if [ -n "${ZSH_VERSION-}" ]; then
  emulate sh
  setopt sh_word_split
fi

# shellcheck disable=SC2065
test -f "./${0##*/}" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate script in current directory."
  exit 1
}

test -d "/opt/freeware/bin" && {
  export PATH="/opt/freeware/bin:${PATH:-}"
}

test -d "/usr/pkg/gnu/bin" && {
  export PATH="${PATH:-}:/usr/pkg/gnu/bin"
}

# shellcheck disable=SC2065
test -f "./.common.sh" > /dev/null 2>&1 || {
  printf '%s\n' "ERROR: Could not locate .common.sh in current directory."
  exit 1
}

set -e

export CPE1704TKS=1

# shellcheck disable=SC1091
. ./.common.sh

if [ "${DU:-}x" = "x" ]; then
  DU="$(command -v du 2> /dev/null || printf '%s\n' 'du')"
fi

export FIND_COMMAND_FATAL=1
find_command "${AWK:-awk}" "${DU:?}" grep mv mkdir rmdir ./lzpack

"${DU:?}" --version 2>&1 | grep -q 'GNU coreutils' 2> /dev/null \
  || {
    printf '%s\n' "ERROR: '${DU:?}' is not GNU coreutils du."
    exit 1
  }

if [ ! -d "bindist" ] || [ ! -f "README.md" ]; then
  printf '%s\n' "ERROR: No bindist/ and/or README.md found!" >&2
  exit 1
fi

SIZES="$("${DU:?}" -Sh --block-size=KiB bindist/*)"
USAGE="$(./lzpack -h 2>&1)"

TMP_README="$(mktemp 2> /dev/null || mktemp_lzpack)"

# shellcheck disable=SC2016
"${AWK:-awk}" -v sizes_raw="${SIZES}" -v usage_raw="${USAGE}" '
BEGIN {
  FS = "|"
  OFS = "|"

  n = split(sizes_raw, lines, "\n")

  for (i = 1; i <= n; i++) {
    if (lines[i] == "") continue

    split(lines[i], parts, /[ \t]+/)
    sz = parts[1]
    pth = parts[2]

    fname = pth
    sub(/.*\//, "", fname)

    sub(/KiB/, "\\&nbsp;KiB", sz)

    file_sizes[fname] = sz
  }

  if ("LZPCKELK.Z" in file_sizes) {
    file_sizes["LZPCKELF.Z"] = file_sizes["LZPCKELK.Z"]
  }
}

{
  if ($0 ~ /^```$/) {
    if (in_block == 0 && usage_done == 0) {
      in_block = 1
      print $0
      print usage_raw
      next
    } else if (in_block == 1) {
      in_block = 0
      print $0
      usage_done = 1
      next
    }
  }

  if (in_block == 1) {
    next
  }

  if ($0 ~ /^\|.*\[.*\]\(.*\).*\|.*\|/) {
    m_start = index($2, "[")
    m_end = index($2, "]")

    if (m_start > 0 && m_end > m_start) {
        current_fname = substr($2, m_start + 1, m_end - m_start - 1)

        if (current_fname in file_sizes) {
            $3 = " " file_sizes[current_fname] " "
        }
    }
  }

  print $0
}
' "README.md" > "${TMP_README:?}"

mv -f "${TMP_README:?}" "README.md"

printf '%s\n' "Successfully updated README.md with archive sizes and usage information."
