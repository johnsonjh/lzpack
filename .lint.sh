#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: be82fe80-58d5-11f1-8fcd-80ee73e9b8e7

# This script isn't ready for use by the general public just yet.
# The author doesn't expect every machine to have all these tools,
# so it's likely this script won't run successfully for anyone else.

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

rc=0

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> distclean <<<<<<<<<<<<<<<<"

(
  set -x
  "${MAKE:-make}" distclean > /dev/null
)

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppi <<<<<<<<<<<<<<<<"

"${MAKE:-make}" cs8080.h csz80.h > /dev/null

for f in ./cs8080.h ./csz80.h ./stubasm.c ./lzpack.c; do
  if (
    set -x
    cppi -a --check "${f}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
done

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppcheck <<<<<<<<<<<<<<<<"

if (
  set -x
  "${CPPCHECK:-cppcheck}" \
    --enable=warning,style,performance,portability,unusedFunction \
    --force --check-level=exhaustive --std=c89 --platform=unix64 \
    --inline-suppr --inconclusive --quiet --error-exitcode=2 \
    -D__CPPCHECK__ -D__LINT__ -j 1 ./stubasm.c ./lzpack.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer standard <<<<<<<<<<<<<<<<"

"${MAKE:-make}" distclean > /dev/null 2>&1 || :

if (
  set -x
  "${MAKE:-make}" \
    CC="${GCC_CMD:-gcc}" \
    CFLAGS="-std=c89 -pedantic -ansi -Wall -Werror -Wpedantic -Wextra -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fanalyzer"
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer streaming <<<<<<<<<<<<<<<<"

"${MAKE:-make}" distclean > /dev/null 2>&1 || :

if (
  set -x
  "${MAKE:-make}" \
    CC="${GCC_CMD:-gcc}" \
    CFLAGS="-std=c89 -pedantic -ansi -Wall -Werror -Wpedantic -Wextra -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -DLZPACK_STREAM -fanalyzer"
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build standard <<<<<<<<<<<<<<<<"

"${MAKE:-make}" distclean > /dev/null 2>&1 || :
TMPID=$$$$

if (
  set -x
  "${SCAN_BUILD_CMD:-scan-build}" \
    --status-bugs \
    -o /tmp/"lzpack-scan.${TMPID}" "${MAKE:-make}" all > /dev/null 2>&1
); then
  :
else
  printf '\n%s\n' "*** scan-build reported issues (see /tmp/lzpack-scan.${TMPID})"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> pvs-studio standard <<<<<<<<<<<<<<<<"

rm -f compile_commands.json log.pvs 2> /dev/null
rm -f -r ./pvsreport 2> /dev/null 2>&1
"${MAKE:-make}" distclean > /dev/null 2>&1 || :

(
  set -x
  "${BEAR_CMD:-bear}" -- "${MAKE:-make}" > /dev/null
)

(
  set -x
  pvs-studio-analyzer analyze -q --intermodular -j 1 -o log.pvs
)

if (
  set -x
  plog-converter -a "GA:1,2,3" -t fullhtml log.pvs \
    -o pvsreport --indicateWarnings
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build stream <<<<<<<<<<<<<<<<"

"${MAKE:-make}" distclean > /dev/null 2>&1 || :
TMPID=$$$$

if (
  set -x
  "${SCAN_BUILD_CMD:-scan-build}" \
    --status-bugs \
    -o /tmp/"lzpack-scan.${TMPID}" "${MAKE:-make}" \
    CFLAGS="-O -DLZPACK_STREAM" all > /dev/null 2>&1
); then
  :
else
  printf '\n%s\n' "*** scan-build reported issues (see /tmp/lzpack-scan.${TMPID})"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> pvs-studio stream <<<<<<<<<<<<<<<<"

rm -f compile_commands.json log.pvs 2> /dev/null
rm -f -r ./pvsreport 2> /dev/null 2>&1
"${MAKE:-make}" distclean > /dev/null 2>&1 || :

(
  set -x
  "${BEAR_CMD:-bear}" -- "${MAKE:-make}" > /dev/null
)

(
  set -x
  pvs-studio-analyzer analyze -q --intermodular -j 1 -o log.pvs
)

if (
  set -x
  plog-converter -a "GA:1,2,3" -t fullhtml log.pvs \
    -o pvsreport --indicateWarnings
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> ch <<<<<<<<<<<<<<<<"

if (
  set -x
  "${CH_CMD:-ch}" -n ./stubasm.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

if (
  set -x
  "${CH_CMD:-ch}" -n ./lzpack.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> oracle lint <<<<<<<<<<<<<<<<"

if (
  set -x
  /opt/oracle/developerstudio12.6/bin/lint \
    -DLZPACK_STREAM -O -fd -std=c89 -err=warn -XCC=no \
    -errchk=structarg,parentheses,locfmtchk lzpack.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

if (
  set -x
  /opt/oracle/developerstudio12.6/bin/lint \
    -O -fd -std=c89 -err=warn -XCC=no \
    -errchk=structarg,parentheses,locfmtchk lzpack.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

if (
  set -x
  /opt/oracle/developerstudio12.6/bin/lint \
    -O -fd -std=c89 -err=warn -XCC=no \
    -errchk=structarg,parentheses,locfmtchk stubasm.c
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> reuse <<<<<<<<<<<<<<<<"

if (
  set -x
  "${REUSE_CMD:-reuse}" lint -q || "${REUSE_CMD:-reuse}" lint
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shellcheck <<<<<<<<<<<<<<<<"

if (
  set -x
  "${SHELLCHECK_CMD:-shellcheck}" -o any,all \
    ./.common.sh \
    ./.build-cpm.sh \
    ./.lint.sh \
    ./tests/run.sh
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shfmt <<<<<<<<<<<<<<<<"

if (
  set -x
  "${SHFMT_CMD:-shfmt}" -bn -sr -fn -i 2 -s -d \
    ./.common.sh \
    ./.build-cpm.sh \
    ./.lint.sh \
    ./tests/run.sh
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> black <<<<<<<<<<<<<<<<"

if (
  set -x
  "${BLACK_CMD:-black}" --quiet --check \
    ./tests/gen.py \
    ./tests/harness.py
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  ("${BLACK_CMD:-black}" --check \
    ./tests/gen.py \
    ./tests/harness.py || :)
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch standard <<<<<<<<<<<<<<<<"

"${MAKE:-make}" clean > /dev/null 2>&1 || :
if (
  set -x
  "${MAKE:-make}" \
    CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
    CC="${HOME}/src/smatch/cgcc"
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch stream <<<<<<<<<<<<<<<<"

"${MAKE:-make}" clean > /dev/null 2>&1 || :
if (
  set -x
  "${MAKE:-make}" \
    CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
    CFLAGS="-O -DLZPACK_STREAM" CC="${HOME}/src/smatch/cgcc"
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

if [ "${rc}" = 0 ]; then
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> lint SUCCESSFUL <<<<<<<<<<<<<<<<"
else
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> lint FAILED!!!! <<<<<<<<<<<<<<<<"
fi

exit "${rc}"
