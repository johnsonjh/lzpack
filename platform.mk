# LZPACK - platform.mk
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 691c4d86-61c5-11f1-92c0-80ee73e9b8e7

################################################################################

# NOTE: It is currently safe that we don't copy all CFLAGS into LDFLAGS,
# because no part of LZPACK currently builds with multiple compilation units,
# but if that changes it might be a necessary change when targeting older
# compilers.  We also will detect and enable LTO to allow possible link-time
# inlining of system library functions that might available with LTO enabled.

################################################################################

# Detect OS with `uname`
UNAME_S:=$(shell uname -s 2> /dev/null)
ifneq "$(findstring AIX,$(UNAME_S))" ""
 UNAME_M:=$(shell uname -p 2> /dev/null)
else
 UNAME_M:=$(shell uname -m 2> /dev/null)
endif
$(info [MAKE] Platform = $(UNAME_S) $(UNAME_M))

################################################################################

# Detect if CC is GCC with `-v`
GCC_COMP=$(shell $(CC) -v 2>&1 | \
 grep -q -i "gcc version" 2> /dev/null && \
  printf '%s\n' "1")

################################################################################

# Detect if CC is Clang with `-v`
CLANG_COMP=$(shell $(CC) -v 2>&1 | \
 grep -q -i "clang version" 2> /dev/null && \
  printf '%s\n' "1")

################################################################################

# Set GCC_CLANG is CC is GCC or Clang
ifeq ($(GCC_COMP),1)
 GCC_CLANG=1
 COMP_NAME=(GCC)
endif
ifeq ($(CLANG_COMP),1)
 GCC_CLANG=1
 COMP_NAME=(Clang)
endif

################################################################################

# Detect if CC is Sun CC with `-V`
ifneq ($(GCC_CLANG),1)
 SUNCC_CMP=$(shell $(CC) -V 2>&1 | \
  grep -q -e "Sun C" 2> /dev/null && printf '%s\n' "1")
 ifeq ($(SUNCC_CMP),1)
  $(info [MAKE] Compiler = $(CC) (Sun/Oracle))
 endif
endif

################################################################################

# Sun CC: Disable LTO
ifeq ($(SUNCC_CMP),1)
 NO_LTO:=1
endif

################################################################################

# Detect if CC is GCC by name (we'll trust it acts like real GCC)
ifneq "$(findstring gcc,$(CC))" ""
 GCC_CLANG=1
endif

################################################################################

# Detect if CC is Clang by name (we'll trust it acts like real Clang)
ifneq "$(findstring clang,$(CC))" ""
 GCC_CLANG=1
endif

################################################################################

# Show compiler details if we haven't already
ifeq ($(GCC_CLANG),1)
 $(info [MAKE] Compiler = $(CC) $(COMP_NAME))
endif
ifneq ($(GCC_CLANG),1)
 ifneq ($(SUNCC_CMP),1)
  $(info [MAKE] Compiler = $(CC))
 endif
endif

################################################################################

# Detect support for `-std=c90` or `-std=c89` (or nothing)
C90_OK:=$(shell printf '%s\n' \
 "int main(void){return 0;}" > .test.c; \
  $(CC) -std=c90 .test.c -o .test.out > /dev/null 2>&1; \
   echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
ifeq ($(C90_OK),1)
 C89_OK:=$(shell printf '%s\n' \
  "int main(void){return 0;}" > .test.c; \
   $(CC) -std=c89 .test.c -o .test.out > /dev/null 2>&1; \
    echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
endif
ifeq ($(C90_OK),0)
 CFLAGS+=-std=c90
endif
ifeq ($(C89_OK),0)
 CFLAGS+=-std=c89
endif

################################################################################

# Extra CFLAGS for GCC or Clang
EXTRA_CFLAGS?=-Wall -Wextra -Wpedantic
ifeq ($(GCC_CLANG),1)
 CFLAGS+=$(EXTRA_CFLAGS)
endif

################################################################################

# Upgrade GCC/Clang plain -O to -O3 (if not overridden with another level)
ifeq ($(GCC_CLANG),1)
 CFLAGS := $(patsubst -O,-O3,$(CFLAGS))
endif

################################################################################

# Same as above for Sun/Oracle compilers, but use -xO5
ifeq ($(SUNCC_CMP),1)
 CFLAGS := $(patsubst -O,-xO5,$(CFLAGS))
endif

################################################################################

# Detect if CC with current CFLAGS supports `-flto`
ifndef NO_LTO
 FLTO_WR:=$(shell printf '%s\n' "int main(void){return 0;}" > .test.c; \
  $(CC) $(CFLAGS) -flto .test.c -o .test.out > /dev/null 2>&1; \
   echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
 ifeq ($(FLTO_WR),0)
  FLTO_OK:=$(shell printf '%s\n' "int main(void){return 0;}" > .test.c; \
   $(CC) $(CFLAGS) -Werror -flto .test.c -o .test.out > /dev/null 2>&1; \
    echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
  ifeq ($(FLTO_OK),0)
   LTO_FLAGS:=-flto
   # Detect if CC supports `-flto=auto`
   AUTO_WR:=$(shell printf '%s\n' "int main(void){return 0;}" > .test.c; \
    $(CC) $(CFLAGS) -flto=auto .test.c -o .test.out > /dev/null 2>&1; \
     echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
   ifeq ($(AUTO_WR),0)
    AUTO_OK:=$(shell printf '%s\n' "int main(void){return 0;}" > .test.c; \
     $(CC) $(CFLAGS) -Werror -flto=auto .test.c -o .test.out > /dev/null 2>&1; \
      echo $$?; rm -f .test.c .test.out > /dev/null 2>&1)
    ifeq ($(AUTO_OK),0)
     LTO_FLAGS:=-flto=auto
    endif
   endif
  endif
 endif
 ifeq ($(findstring -flto,$(CFLAGS)),)
  CFLAGS+=$(LTO_FLAGS)
 endif
 ifeq ($(findstring -flto,$(LDFLAGS)),)
  LDFLAGS+=$(LTO_FLAGS)
 endif
endif

################################################################################

# Solaris or illumos: Force `-flto=auto` to `-flto`
ifneq "$(findstring SunOS,$(UNAME_S))" ""
 CFLAGS := $(subst -flto=auto,-flto,$(CFLAGS))
 LDFLAGS := $(subst -flto=auto,-flto,$(LDFLAGS))
endif

################################################################################

# AIX: Default to 64-bit (OBJECT_MODE=64)
ifneq "$(findstring AIX,$(UNAME_S))" ""
 export OBJECT_MODE=64
 # AIX with GCC: Disable LTO, use 64-bit
 ifneq "$(findstring gcc,$(CC))" ""
  CFLAGS+=-maix64
  LDFLAGS+=-maix64
  CFLAGS:=$(subst -flto=auto,-flto,$(CFLAGS))
  CFLAGS:=$(subst -flto,,$(CFLAGS))
  LDFLAGS:=$(subst -flto=auto,-flto,$(LDFLAGS))
  LDFLAGS:=$(subst -flto,,$(LDFLAGS))
 endif
endif

################################################################################

# OS/400 with GCC: Disable LTO
ifneq "$(findstring OS400,$(UNAME_S))" ""
 ifneq "$(findstring gcc,$(CC))" ""
  CFLAGS:=$(subst -flto=auto,-flto,$(CFLAGS))
  CFLAGS:=$(subst -flto,,$(CFLAGS))
  LDFLAGS:=$(subst -flto=auto,-flto,$(LDFLAGS))
  LDFLAGS:=$(subst -flto,,$(LDFLAGS))
 endif
endif

################################################################################

# Display report
$(info [MAKE] CFLAGS   = $(CFLAGS))
$(info [MAKE] LDFLAGS  = $(LDFLAGS))

################################################################################

# Local Variables:
# mode: makefile
# indent-tabs-mode: t
# tab-width: 8
# whitespace-style: (tabs tab-mark)
# whitespace-display-mappings: ((tab-mark 9 [45] [45]))
# fill-column: 80
# eval: (setq-local whitespace-display-mappings
#                   '((tab-mark 9
#                               [45 45 45 45 45 45 62]
#                               [45 45 45 45 45 45 62])))
# eval: (whitespace-mode 1)
# eval: (setq-local display-fill-column-indicator-column 80)
# eval: (display-fill-column-indicator-mode 1)
# End:

################################################################################
# vim: set ft=make ts=8 ai noexpandtab list listchars=tab\:\>\- cc=80 :
################################################################################
