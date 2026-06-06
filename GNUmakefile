# LZPACK - GNUmakefile
# Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
# SPDX-License-Identifier: MIT-0
# scspell-id: 646dd6b0-61c5-11f1-821b-80ee73e9b8e7

################################################################################

# Compatible defaults.

CC=$(shell echo $$(command -v cc 2> /dev/null || \
	command -v gcc 2> /dev/null || \
	command -v clang 2> /dev/null || echo cc))
CFLAGS=-O

################################################################################

include platform.mk

################################################################################

export CC
export CFLAGS
export LDFLAGS

################################################################################

include Makefile

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
