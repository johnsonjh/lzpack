#!/bin/sh
# .common.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 47d94114-5aff-11f1-b51a-80ee73e9b8e7

################################################################################

if [ -z "${CPE1704TKS:-}" ]; then
  printf '%s\n' \
    "ERROR: This script must be sourced; it is not directly executable!" >&2
  exit 1
fi

################################################################################

find_command()
{
  missing=${missing-}
  missing=""
  sep=${sep-}
  sep=""

  for cmd; do
    if ! command -v "${cmd:?}" > /dev/null 2>&1; then
      missing="${missing:-}${sep:-}${cmd:?}"
      sep=" "
    fi
  done

  [ -z "${missing:-}" ] && return 0

  case ${FIND_COMMAND_FATAL:-0} in
  1)
    prefix="ERROR: "
    ;;
  *)
    prefix="WARNING: "
    ;;
  esac

  # shellcheck disable=SC2086
  set -- ${missing:-}
  count=$#

  if [ "${count:?}" -eq 1 ]; then
    printf '%sCommand '\''%s'\'' was not found!\n' "${prefix:?}" "${1:?}"
    return 1
  fi

  if [ "${count:?}" -eq 2 ]; then
    printf '%sCommands '\''%s'\'' and '\''%s'\'' were not found!\n' \
      "${prefix:?}" "${1:?}" "${2:?}"
    return 1
  fi

  last=${count:?}
  last_cmd=$(eval "printf '%s' \"\${${last:?}}\"")

  out=${out-}
  out=""
  i=${i-}
  i=1

  while [ "${i:?}" -lt "${last:?}" ]; do
    eval "c=\${${i:?}}"
    if [ -z "${out:-}" ]; then
      out="'${c:?}'"
    else
      out="${out:?}, '${c:?}'"
    fi
    i=$((i + 1))
  done

  printf '%sCommands %s, and '\''%s'\'' were not found!\n' \
    "${prefix:?}" "${out:?}" "${last_cmd:?}"

  return 1
}

################################################################################

detect_width()
{
  width=${width:-80}

  if cols=$(tput cols 2> /dev/null); then
    case ${cols:-} in
    *[!0-9]* | '') : ;;
    *)
      w=$((cols - 2))
      case ${w:-} in
      *[!0-9]* | '' | 0) : ;;
      *)
        width=${w:-}
        ;;
      esac
      ;;
    esac
  fi

  if [ "${width:?}" -eq 80 ]; then
    case ${COLUMNS:-} in
    *[!0-9]* | '') : ;;
    *)
      width=${COLUMNS:-}
      ;;
    esac
  fi

  printf '%s\n' "${width:?}"
}

################################################################################

wrap()
{
  awk -v width="${1:?}" '
    {
      for (i = 1; i <= NF; i++) {
        if (length(out) + length($i) + 1 > width) {
          print out
          out = $i
        } else {
          out = (out ? out " " $i : $i)
        }
      }
    }
    END {
      if (out) print out
    }
  '
}

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
