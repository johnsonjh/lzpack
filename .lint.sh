#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: be82fe80-58d5-11f1-8fcd-80ee73e9b8e7

# This script isn't quite for use by the general public just yet.
# The author doesn't expect every machine to have all these tools,
# so it's likely this script won't run successfully for anyone else.

set -eu
cd "$(dirname "$0")"

rc=0

printf '\n%s\n' ">>>>>>>>>>>>>>>> cppi <<<<<<<<<<<<<<<<"

for f in *.c; do
  if cppi -a --check "${f}"; then
    :
  else
    rc=1
  fi
done

printf '\n%s\n' ">>>>>>>>>>>>>>>> cppcheck <<<<<<<<<<<<<<<<"

if cppcheck --enable=warning,style,performance,portability,unusedFunction \
  --force --check-level=exhaustive --std=c89 --platform=unix64 \
  --inline-suppr --inconclusive --error-exitcode=2 \
  -D__CPPCHECK__ -D__LINT__ -j 1 ./*.c; then
  :
else
  rc=1
fi

printf '\n%s\n' ">>>>>>>>>>>>>>>> scan-build <<<<<<<<<<<<<<<<"

make distclean > /dev/null 2>&1 || :

if scan-build --status-bugs -o /tmp/lzpack-scan make all > /dev/null 2>&1; then
  :
else
  printf '\n%s\n' "scan-build reported issues (see /tmp/lzpack-scan)"
  rc=1
fi

printf '\n%s\n' ">>>>>>>>>>>>>>>> pvs-studio <<<<<<<<<<<<<<<<"

rm -f compile_commands.json log.pvs 2> /dev/null
rm -f -r ./pvsreport 2> /dev/null 2>&1

bear -- make clean all

pvs-studio-analyzer analyze --intermodular -j 1 -o log.pvs

if plog-converter -a "GA:1,2,3" -t fullhtml log.pvs \
  -o pvsreport --indicateWarnings; then
  :
else
  rc=1
fi

printf '\n%s\n' ">>>>>>>>>>>>>>>> ch <<<<<<<<<<<<<<<<"

if ch -n ./stubasm.c; then
  :
else
  rc=1
fi

if ch -n ./lzpack.c; then
  :
else
  rc=1
fi

printf '\n%s\n' ">>>>>>>>>>>>>>>> oracle lint <<<<<<<<<<<<<<<<"

if /opt/oracle/developerstudio12.6/bin/lint stubasm.c; then
  :
else
  rc=1
fi

if /opt/oracle/developerstudio12.6/bin/lint lzpack.c; then
  :
else
  rc=1
fi

printf '\n%s\n' ">>>>>>>>>>>>>>>> reuse <<<<<<<<<<<<<<<<"

if (reuse lint -q || reuse lint); then
  :
else
  rc=1
fi

if [ "${rc}" = 0 ]; then
  printf '\n%s\n' ">>>>>>>>>>>>>>>> lint clean <<<<<<<<<<<<<<<<"
else
  printf '\n%s\n' ">>>>>>>>>>>>>>>> lint FAILED <<<<<<<<<<<<<<<<"
fi

exit "${rc}"
