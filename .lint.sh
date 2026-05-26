#!/bin/sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: be82fe80-58d5-11f1-8fcd-80ee73e9b8e7

set -eu
cd "$(dirname "$0")"

rc=0

printf '%s\n' ">> cppi --check"
for f in *.c; do
  if cppi --check "${f}"; then :; else rc=1; fi
done

printf '%s\n' ">> cppcheck"
if cppcheck --enable=warning,style,performance,portability,unusedFunction \
  --force --check-level=exhaustive --std=c89 --platform=unix64 \
  --inline-suppr --inconclusive --error-exitcode=2 \
  -D__CPPCHECK__ -D__LINT__ -j 1 ./*.c; then :; else rc=1; fi

printf '%s\n' ">> scan-build"
make distclean > /dev/null 2>&1 || :
if scan-build --status-bugs -o /tmp/lzpack-scan make all > /dev/null 2>&1; then
  :
else
  printf '%s\n' "scan-build reported issues (see /tmp/lzpack-scan)"
  rc=1
fi
make distclean > /dev/null 2>&1 || :

printf '%s\n' ">> pvs-studio"
rm -f compile_commands.json log.pvs 2> /dev/null
rm -f -r ./pvsreport 2> /dev/null 2>&1
env CC=clang bear -- scan-build make clean distclean all
pvs-studio-analyzer analyze --intermodular -j 72 -o log.pvs
if plog-converter -a "GA:1,2,3" -t fullhtml log.pvs \
  -o pvsreport --indicateWarnings; then
  :
else
  rc=1
fi

if [ "${rc}" = 0 ]; then printf '%s\n' ">> lint clean"; else printf '%s\n' ">> lint FAILED"; fi
exit "${rc}"
