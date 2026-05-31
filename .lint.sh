#!/bin/sh
# .lint.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: be82fe80-58d5-11f1-8fcd-80ee73e9b8e7

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

export FIND_COMMAND_FATAL=1
find_command "${AWK:-awk}" diff grep "${MAKE:-make}" mkdir paste rm rmdir \
  sed sleep uname

################################################################################

export FIND_COMMAND_FATAL=0

# shellcheck disable=SC2310
if out=$(
  find_command \
    "${BEAR_CMD:-bear}" "${BLACK_CMD:-black}" "${CH_CMD:-ch}" \
    "${CLANG_CMD:-clang}" "${CPPCHECK:-cppcheck}" codespell cppi flawfinder \
    git "${GCC_CMD:-gcc}" plog-converter pvs-studio-analyzer \
    "${REUSE_CMD:-reuse}" "${SCAN_BUILD_CMD:-scan-build}" \
    "${SHELLCHECK_CMD:-shellcheck}" "${SHFMT_CMD:-shfmt}" \
    "${HOME}/src/smatch/smatch" "${HOME}/src/smatch/cgcc" 2>&1
); then
  status=0
else
  status="$?"
fi

width="$(detect_width)"

# shellcheck disable=SC2310
printf '%s\n' "${out:-}" \
  | wrap "${width:?}"

unset NEED_PAUSE

if [ "${status:?}" -ne 0 ]; then
  NEED_PAUSE=1
fi

################################################################################

os="$(uname -s 2> /dev/null)"

unset CHECK_OLINT

case "${os:?}" in
Linux)
  CHECK_OLINT=1
  ;;
Solaris)
  CHECK_OLINT=1
  ;;
*) : ;;
esac

unset OLINT

if [ "${CHECK_OLINT:-0}" -eq 1 ]; then
  if command -v "/opt/solarisstudio12.6/bin/lint" \
    > /dev/null 2>&1; then
    OLINT="/opt/solarisstudio12.6/bin/lint"
  elif command -v "/opt/oracle/developerstudio12.6/bin/lint" \
    > /dev/null 2>&1; then
    OLINT="/opt/oracle/developerstudio12.6/bin/lint"
  fi

  if [ -z "${OLINT+x}" ]; then
    printf '%s\n' "WARNING: Oracle Developer Studio Lint 12.6 was not found!" \
      | wrap "${width:?}"
    NEED_PAUSE=1
  fi
fi

################################################################################

case ${OVERRIDE_PAUSE:-} in
'' | *[!0-9]*)
  unset OVERRIDE_PAUSE
  ;;
*) : ;;
esac

test "${NEED_PAUSE:-0}" -ne 1 || {
  printf '%s\n' \
    "Some checks will be skipped! [pausing ${OVERRIDE_PAUSE:-10}s]" \
    | wrap "${width:?}"
  sleep "${OVERRIDE_PAUSE:-10}"
}

################################################################################

rc=0

################################################################################

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> distclean <<<<<<<<<<<<<<<<"

(
  set -x
  "${MAKE:-make}" distclean > /dev/null
)

################################################################################

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> make <<<<<<<<<<<<<<<<"

if (
  set -x
  "${MAKE:-make}"
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

################################################################################

command -v codespell > /dev/null 2>&1 && {
  command -v git > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> codespell <<<<<<<<<<<<<<<<"
    if (
      CODESPELL_EXCLUDE=$({
        git ls-files --ignored --exclude-standard --others \
          | sed 's/["\\]/\\&/g' \
          | paste -sd',' -
      } | sed 's/^/"/; s/$/"/')
      codespell --ignore-words-list \
        "expad,ACI,clen,DAA" --skip "${CODESPELL_EXCLUDE:-}" .
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  }
}

################################################################################

command -v editorconfig-checker > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> editorconfig <<<<<<<<<<<<<<<<"
  if (
    editorconfig-checker
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> dangling words <<<<<<<<<<<<<<<<"

if (
  set -x
  (grep -xi '[^[:space:]]\+' README.md | grep -Ev '(`|<|>|\[|\]|:)') && {
    : ERROR: Dangling words found
    exit 1
  } || exit 0
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

################################################################################

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> tag generation <<<<<<<<<<<<<<<<"

if (
  set -x
  "${MAKE:-make}" tags
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

################################################################################

printf '\n%s\n\n' ">>>>>>>>>>>>>>>> license diff <<<<<<<<<<<<<<<<"

if (
  set -x
  diff LICENSES/MIT-0.txt LICENSE
); then
  :
else
  printf '%s\n' "****** FAILURE DETECTED ******"
  rc=1
fi

################################################################################

command -v flawfinder > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> flawfinder <<<<<<<<<<<<<<<<"
  if (
    set -x
    flawfinder --quiet --dataonly --omittime --error-level=3 --context \
      --minlevel=3 stubasm.c lzpack.c tests/t_autoarch.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v cppi > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppi <<<<<<<<<<<<<<<<"
  for f in ./cs8080.h ./csz80.h ./csr8080.h ./csrz80.h ./stubasm.c \
    ./lzpack.c ./tests/t_autoarch.c; do
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
}

################################################################################

command -v "${CPPCHECK:-cppcheck}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> cppcheck <<<<<<<<<<<<<<<<"
  if (
    "${CPPCHECK:-cppcheck}" --check-level=exhaustive 2>&1 \
      | grep -q 'unrecognized command line option' \
      || {
        CHECK_LEVEL="--check-level=exhaustive"
      } || :
    set -x
    # shellcheck disable=SC2086
    "${CPPCHECK:-cppcheck}" \
      --enable=warning,style,performance,portability,unusedFunction \
      --force ${CHECK_LEVEL:-} --std=c89 --platform=unix64 \
      --inline-suppr --inconclusive --quiet --error-exitcode=2 \
      -D__CPPCHECK__ -D__LINT__ -j 1 \
      ./stubasm.c ./lzpack.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

aCFLAGS="-std=c89 -pedantic -ansi -Wall -Werror -Wpedantic -Wextra -O3"
aCFLAGS="${aCFLAGS:?} -U_FORTIFY_SOURCE"
aCFLAGS="${aCFLAGS:?} -D_FORTIFY_SOURCE=${FORTIFY_LEVEL:-3}"
aCFLAGS="${aCFLAGS:?} -DGCC_ANALYZER -fanalyzer"

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer standard <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer no-opt <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?} -DLZPACK_NO_OPT"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer no-progress <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?} -DLZPACK_NO_PROGRESS"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

aCFLAGS="${aCFLAGS:?} -DLZPACK_STREAM"

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> gcc analyzer streaming <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

aCFLAGS="${aCFLAGS:?} -DLZPACK_OPT"

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' \
    ">>>>>>>>>>>>>>>> gcc analyzer streaming opt <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${GCC_CMD:-gcc}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' \
    ">>>>>>>>>>>>>>>> gcc analyzer streaming opt no-progress <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${GCC_CMD:-gcc}" \
      CFLAGS="${aCFLAGS:?} -DLZPACK_NO_PROGRESS"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

lCFLAGS="-U_FORTIFY_SOURCE -O3 -Weverything -Wno-unsafe-buffer-usage"
lCFLAGS="${lCFLAGS:-} -Wno-padded -Wno-missing-noreturn"
lCFLAGS="${lCFLAGS:-} -Wno-disabled-macro-expansion"
lCFLAGS="${lCFLAGS:-} -Wno-used-but-marked-unused -Werror -ferror-limit=0"
lCFLAGS="${lCFLAGS:-} -std=c89 -Wno-padded -Wno-used-but-marked-unused"

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> clang strict standard <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> clang strict no-opt <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?} -DLZPACK_NO_OPT"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> clang strict no-progress <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?} -DLZPACK_NO_PROGRESS"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

lCFLAGS="${lCFLAGS:?} -DLZPACK_STREAM"

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> clang strict streaming <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

lCFLAGS="${lCFLAGS:?} -DLZPACK_OPT"

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' \
    ">>>>>>>>>>>>>>>> clang strict streaming opt <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?}"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' \
    ">>>>>>>>>>>>>>>> clang strict streaming opt no-progress <<<<<<<<<<<<<<<<"
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  if (
    set -x
    "${MAKE:-make}" \
      CC="${CLANG_CMD:-clang}" \
      CFLAGS="${lCFLAGS:?} -DLZPACK_NO_PROGRESS"
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build standard <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs \
        -o "${TMPFILE:?}" "${MAKE:-make}" all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf \
        '\n%s\n' "*** scan-build reported issues, see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build no-opt <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs \
        -o "${TMPFILE:?}" "${MAKE:-make}" \
        CFLAGS="-O -DLZPACK_NO_OPT" all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf '\n%s\n' "*** scan-build reported issues; see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build no-progress <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs \
        -o "${TMPFILE:?}" "${MAKE:-make}" \
        CFLAGS="-O -DLZPACK_NO_PROGRESS" all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf '\n%s\n' "*** scan-build reported issues; see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
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
    }
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
      printf '\n%s\n\n' ">>>>>>>>>>>>>>>> pvs-studio no-opt <<<<<<<<<<<<<<<<"
      rm -f compile_commands.json log.pvs 2> /dev/null
      rm -f -r ./pvsreport 2> /dev/null 2>&1
      "${MAKE:-make}" distclean > /dev/null 2>&1 || :
      (
        set -x
        "${BEAR_CMD:-bear}" -- "${MAKE:-make}" \
          CFLAGS="-O3 -DLZPACK_NO_OPT" > /dev/null
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
    }
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
      printf '\n%s\n\n' \
        ">>>>>>>>>>>>>>>> pvs-studio no-progress <<<<<<<<<<<<<<<<"
      rm -f compile_commands.json log.pvs 2> /dev/null
      rm -f -r ./pvsreport 2> /dev/null 2>&1
      "${MAKE:-make}" distclean > /dev/null 2>&1 || :
      (
        set -x
        "${BEAR_CMD:-bear}" -- "${MAKE:-make}" \
          CFLAGS="-O3 -DLZPACK_NO_PROGRESS" > /dev/null
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
    }
  }
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build stream <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs \
        -o "${TMPFILE:?}" "${MAKE:-make}" \
        CFLAGS="-O -DLZPACK_STREAM" all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf '\n%s\n' "*** scan-build reported issues; see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> scan-build stream opt <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs \
        -o "${TMPFILE:?}" "${MAKE:-make}" \
        CFLAGS="-O -DLZPACK_STREAM -DLZPACK_OPT" all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf '\n%s\n' "*** scan-build reported issues; see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${SCAN_BUILD_CMD:-scan-build}" > /dev/null 2>&1 && {
  command -v "${CLANG_CMD:-clang}" > /dev/null 2>&1 && {
    printf '\n%s\n\n' \
      ">>>>>>>>>>>>>>>> scan-build stream opt no-progress <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" distclean > /dev/null 2>&1 || :
    # shellcheck disable=SC2119
    TMPFILE="$(mktemp 2> /dev/null || mktemp_lzpack)"
    rm -f "${TMPFILE}"
    if (
      set -x
      "${SCAN_BUILD_CMD:-scan-build}" \
        --status-bugs -o "${TMPFILE:?}" \
        "${MAKE:-make}" \
        CFLAGS="-O -DLZPACK_STREAM -DLZPACK_OPT -DLZPACK_NO_PROGRESS" \
        all > /dev/null 2>&1
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      printf '\n%s\n' "*** scan-build reported issues; see '${TMPFILE:?}'"
      rc=1
    fi
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
      printf '\n%s\n\n' ">>>>>>>>>>>>>>>> pvs-studio stream <<<<<<<<<<<<<<<<"
      rm -f compile_commands.json log.pvs 2> /dev/null
      rm -f -r ./pvsreport 2> /dev/null 2>&1
      "${MAKE:-make}" distclean > /dev/null 2>&1 || :
      (
        set -x
        "${BEAR_CMD:-bear}" -- "${MAKE:-make}" \
          CFLAGS="-O3 -DLZPACK_STREAM" > /dev/null
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
    }
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
      printf '\n%s\n\n' \
        ">>>>>>>>>>>>>>>> pvs-studio stream opt <<<<<<<<<<<<<<<<"
      rm -f compile_commands.json log.pvs 2> /dev/null
      rm -f -r ./pvsreport 2> /dev/null 2>&1
      "${MAKE:-make}" distclean > /dev/null 2>&1 || :
      (
        set -x
        "${BEAR_CMD:-bear}" -- "${MAKE:-make}" \
          CFLAGS="-O3 -DLZPACK_STREAM -DLZPACK_OPT" > /dev/null
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
    }
  }
}

################################################################################

command -v "${BEAR_CMD:-bear}" > /dev/null 2>&1 && {
  command -v pvs-studio-analyzer > /dev/null 2>&1 && {
    command -v plog-converter > /dev/null 2>&1 && {
      printf '\n%s\n\n' \
        ">>>>>>>>>>>>>>>> pvs-studio stream opt no-progress <<<<<<<<<<<<<<<<"
      rm -f compile_commands.json log.pvs 2> /dev/null
      rm -f -r ./pvsreport 2> /dev/null 2>&1
      "${MAKE:-make}" distclean > /dev/null 2>&1 || :
      (
        set -x
        "${BEAR_CMD:-bear}" -- "${MAKE:-make}" \
          CFLAGS="-O3 -DLZPACK_STREAM -DLZPACK_OPT -DLZPACK_NO_PROGRESS" \
          > /dev/null
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
    }
  }
}

################################################################################

command -v "${CH_CMD:-ch}" > /dev/null 2>&1 && {
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
  if (
    set -x
    (cd tests && "${CH_CMD:-ch}" -n ./t_autoarch.c)
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${OLINT:-}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> oracle lint <<<<<<<<<<<<<<<<"
  if (
    set -x
    "${OLINT:?}" \
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
    "${OLINT:?}" \
      -DLZPACK_STREAM -DLZPACK_OPT -O -fd -std=c89 -err=warn -XCC=no \
      -errchk=structarg,parentheses,locfmtchk lzpack.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
  if (
    set -x
    "${OLINT:?}" \
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
    "${OLINT:?}" \
      -DLZPACK_NO_OPT -O -fd -std=c89 -err=warn -XCC=no \
      -errchk=structarg,parentheses,locfmtchk lzpack.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
  if (
    set -x
    "${OLINT:?}" \
      -DLZPACK_NO_PROGRESS -O -fd -std=c89 -err=warn -XCC=no \
      -errchk=structarg,parentheses,locfmtchk lzpack.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
  if (
    set -x
    "${OLINT:?}" \
      -DLZPACK_STREAM -DLZPACK_OPT -DLZPACK_NO_PROGRESS \
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
    "${OLINT:?}" \
      -O -fd -std=c89 -err=warn -XCC=no \
      -errchk=structarg,parentheses,locfmtchk stubasm.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
  if (
    cd tests
    set -x
    "${OLINT:?}" \
      -O -fd -std=c89 -err=warn -XCC=no \
      -errchk=structarg,parentheses,locfmtchk -erroff=E_NAME_DEF_NOT_USED2 \
      t_autoarch.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
  if (
    cd tests
    set -x
    "${OLINT:?}" \
      -O -fd -std=c89 -err=warn -XCC=no -DLZPACK_STREAM \
      -errchk=structarg,parentheses,locfmtchk -erroff=E_NAME_DEF_NOT_USED2 \
      t_autoarch.c
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${REUSE_CMD:-reuse}" > /dev/null 2>&1 && {
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
}

################################################################################

command -v "${SHELLCHECK_CMD:-shellcheck}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shellcheck <<<<<<<<<<<<<<<<"
  if (
    set -x
    "${SHELLCHECK_CMD:-shellcheck}" -o any,all \
      ./.common.sh \
      ./.build-cpm.sh \
      ./.lint.sh \
      ./.updatedocs.sh \
      ./tests/run.sh
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${SHFMT_CMD:-shfmt}" > /dev/null 2>&1 && {
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> shfmt <<<<<<<<<<<<<<<<"
  if (
    set -x
    "${SHFMT_CMD:-shfmt}" -bn -sr -fn -i 2 -s -d \
      ./.common.sh \
      ./.build-cpm.sh \
      ./.lint.sh \
      ./.updatedocs.sh \
      ./tests/run.sh
  ); then
    :
  else
    printf '%s\n' "****** FAILURE DETECTED ******"
    rc=1
  fi
}

################################################################################

command -v "${BLACK_CMD:-black}" > /dev/null 2>&1 && {
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
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
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
  }
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch no-opt <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" clean > /dev/null 2>&1 || :
    if (
      set -x
      "${MAKE:-make}" \
        CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
        CFLAGS="-O -DLZPACK_NO_OPT" CC="${HOME}/src/smatch/cgcc"
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  }
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch no-progress <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" clean > /dev/null 2>&1 || :
    if (
      set -x
      "${MAKE:-make}" \
        CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
        CFLAGS="-O -DLZPACK_NO_PROGRESS" CC="${HOME}/src/smatch/cgcc"
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  }
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
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
  }
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> smatch stream opt <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" clean > /dev/null 2>&1 || :
    if (
      set -x
      "${MAKE:-make}" \
        CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
        CFLAGS="-O -DLZPACK_STREAM -DLZPACK_OPT" CC="${HOME}/src/smatch/cgcc"
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  }
}

################################################################################

command -v "${HOME}/src/smatch/smatch" > /dev/null 2>&1 && {
  command -v "${HOME}/src/smatch/cgcc" > /dev/null 2>&1 && {
    printf '\n%s\n\n' \
      ">>>>>>>>>>>>>>>> smatch stream opt no-progress <<<<<<<<<<<<<<<<"
    "${MAKE:-make}" clean > /dev/null 2>&1 || :
    if (
      set -x
      "${MAKE:-make}" \
        CHECK="${HOME}/src/smatch/smatch --two-pass --full-path" \
        CFLAGS="-O -DLZPACK_STREAM -DLZPACK_OPT -DLZPACK_NO_PROGRESS" \
        CC="${HOME}/src/smatch/cgcc"
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  }
}

################################################################################

case "$(uname -s 2> /dev/null || :)" in
NetBSD)
  if command -p -v lint > /dev/null 2>&1; then
    printf '\n%s\n\n' ">>>>>>>>>>>>>>>> NetBSD lint <<<<<<<<<<<<<<<<"
    if (
      set -x
      lint -a -aa -b -c -e -g -h -P -r -u -w -z lzpack.c
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
    if (
      set -x
      lint -a -aa -b -c -e -g -h -P -r -u -w -z -DLZPACK_STREAM lzpack.c
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
    if (
      set -x
      lint -a -aa -b -c -e -g -h -P -r -u -w -z \
        -DLZPACK_STREAM -DLZPACK_OPT lzpack.c
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
    if (
      set -x
      lint -a -aa -b -c -e -g -h -P -r -u -w -z stubasm.c
    ); then
      :
    else
      printf '%s\n' "****** FAILURE DETECTED ******"
      rc=1
    fi
  fi
  ;;
*) : ;;
esac

################################################################################

if [ "${rc}" = 0 ]; then
  "${MAKE:-make}" distclean > /dev/null 2>&1 || :
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> lint SUCCESSFUL <<<<<<<<<<<<<<<<"
else
  printf '\n%s\n\n' ">>>>>>>>>>>>>>>> lint FAILED!!!! <<<<<<<<<<<<<<<<"
fi

exit "${rc}"

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
