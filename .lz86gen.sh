#!/bin/sh
# .lz86gen.sh
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: e060d888-5d53-11f1-b2a0-80ee73e9b8e7

#   ia16 -> a GNU-as (.intel_syntax) .s assembled and linked by ia16-elf-gcc
# watcom -> a wasm .asm assembled by wasm and linked by owcc
#  aztec -> a C inline #asm Aztec inline assembly for Aztec aztec42_cc

set -eu

target="${1:-}"
body="${2:-lz86body.asm}"

if [ -z "${target}" ] || [ ! -f "${body}" ]; then
  printf '%s\n' "Usage: ./.lz86gen.sh <ia16|watcom|aztec> [bodyfile]" >&2
  exit 1
fi

# Bare instruction stream: drop ';' comments, trailing space, and blank lines.
strip_body()
{
  sed -e 's/;.*$//' -e 's/[[:space:]]*$//' -e '/^[[:space:]]*$/d' "${body}"
}

case "${target}" in
ia16)
  cat << 'EOF'
        .arch   i8086
        .code16
        .intel_syntax noprefix
        .text
        .global lz86_decode
lz86_decode:
        push    bp
        push    si
        push    di
        push    es
        mov     ax,ds
        mov     es,ax
        cld
        mov     si,[lz86_src]
        mov     di,[lz86_dst]
        mov     bp,[lz86_oend]
        mov     ah,128
EOF
  strip_body
  cat << 'EOF'
LZ86DONE:
        pop     es
        pop     di
        pop     si
        pop     bp
        ret
EOF
  ;;

watcom)
  cat << 'EOF'
        .8086
DGROUP  group   _DATA
_DATA   segment word public 'DATA'
        extrn   _lz86_src:word
        extrn   _lz86_dst:word
        extrn   _lz86_oend:word
_DATA   ends
_TEXT   segment byte public 'CODE'
        assume  cs:_TEXT, ds:DGROUP
        public  lz86_decode_
lz86_decode_ proc near
        push    bp
        push    si
        push    di
        push    es
        mov     ax,ds
        mov     es,ax
        cld
        mov     si,_lz86_src
        mov     di,_lz86_dst
        mov     bp,_lz86_oend
        mov     ah,128
EOF
  strip_body
  cat << 'EOF'
LZ86DONE:
        pop     es
        pop     di
        pop     si
        pop     bp
        ret
lz86_decode_ endp
_TEXT   ends
        end
EOF
  ;;

aztec)
  cat << 'EOF'
unsigned lz86_src, lz86_dst, lz86_oend;
lz86_decode()
{
#asm
        push    bp
        push    si
        push    di
        push    es
        mov     ax,ds
        mov     es,ax
        cld
        mov     si,lz86_src_
        mov     di,lz86_dst_
        mov     bp,lz86_oend_
        mov     ah,128
EOF
  strip_body
  cat << 'EOF'
LZ86DONE:
        pop     es
        pop     di
        pop     si
        pop     bp
#endasm
}
EOF
  ;;

*)
  printf 'FATAL: unknown target "%s"\n' "${target}" >&2
  exit 1
  ;;
esac
