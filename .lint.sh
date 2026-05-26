#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: be82fe80-58d5-11f1-8fcd-80ee73e9b8e7

# This script isn't ready for use by the general public just yet.
# The author doesn't expect every machine to have all these tools,
# so it's likely this script won't run successfully for anyone else.

set -eu
cd "$(dirname "$0")"

rc=0

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> distclean <<<<<<<<<<<<<<<<"

(
  set -x
  make distclean > /dev/null 2>&1 || :
)

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppi <<<<<<<<<<<<<<<<"

for f in ./stubasm.c ./lzpack.c; do
  if (
    set -x
    cppi -a --check "${f}"
  ); then
    :
  else
    rc=1
  fi
done

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppcheck <<<<<<<<<<<<<<<<"

if (
  set -x
  cppcheck \
    --enable=warning,style,performance,portability,unusedFunction \
    --force --check-level=exhaustive --std=c89 --platform=unix64 \
    --inline-suppr --inconclusive --quiet --error-exitcode=2 \
    -D__CPPCHECK__ -D__LINT__ -j 1 ./stubasm.c ./lzpack.c
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer <<<<<<<<<<<<<<<<"

make distclean > /dev/null 2>&1 || :

if (
  set -x
  make \
    CC="gcc" \
    CFLAGS="-std=c89 -pedantic -ansi -Wall -Werror -Wpedantic -Wextra -O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fanalyzer"
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build <<<<<<<<<<<<<<<<"

make distclean > /dev/null 2>&1 || :
TMPID=$$$$

if (
  set -x
  scan-build \
    --status-bugs \
    -o /tmp/"lzpack-scan.${TMPID}" make all > /dev/null 2>&1
); then
  :
else
  printf '\n%s\n' "*** scan-build reported issues (see /tmp/lzpack-scan.${TMPID})"
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> pvs-studio <<<<<<<<<<<<<<<<"

rm -f compile_commands.json log.pvs 2> /dev/null
rm -f -r ./pvsreport 2> /dev/null 2>&1
make clean > /dev/null 2>&1 || :

(
  set -x
  bear -- make
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
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> ch <<<<<<<<<<<<<<<<"

if (
  set -x
  ch -n ./stubasm.c
); then
  :
else
  rc=1
fi

if (
  set -x
  ch -n ./lzpack.c
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> oracle lint <<<<<<<<<<<<<<<<"

if (
  set -x
  /opt/oracle/developerstudio12.6/bin/lint \
    -O -fd -std=c89 -err=warn -XCC=no \
    -errchk=structarg,parentheses,locfmtchk stubasm.c
); then
  :
else
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
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> reuse <<<<<<<<<<<<<<<<"

if (
  set -x
  reuse lint -q || reuse lint
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shellcheck <<<<<<<<<<<<<<<<"

if (
  set -x
  shellcheck -o any,all \
    ./.build-cpm.sh \
    ./.lint.sh \
    ./tests/run.sh
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shfmt <<<<<<<<<<<<<<<<"

if (
  set -x
  shfmt -bn -sr -fn -i 2 -s -d \
    ./.build-cpm.sh \
    ./.lint.sh \
    ./tests/run.sh
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> black <<<<<<<<<<<<<<<<"

if (
  set -x
  black --quiet --check \
    ./tests/gen.py \
    ./tests/harness.py
); then
  :
else
  rc=1
fi

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch <<<<<<<<<<<<<<<<"

make clean > /dev/null 2>&1 || :
if (
  set -x
  make \
    CHECK="~/src/smatch/smatch --two-pass --full-path" \
    CC="${HOME}/src/smatch/cgcc"
); then
  :
else
  rc=1
fi

if [ "${rc}" = 0 ]; then
  make distclean > /dev/null 2>&1 || :
  printf '\n%s\n' ">>>>>>>>>>>>>>>> lint SUCCESSFUL <<<<<<<<<<<<<<<<"
else
  printf '\n%s\n' ">>>>>>>>>>>>>>>> lint FAILED!!!! <<<<<<<<<<<<<<<<"
fi

exit "${rc}"
