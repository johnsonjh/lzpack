/*
 * LZPACK - CP/M-80 (8080 and Z80) executable compressor
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: e7989fa4-7b77-11f1-b0b6-80ee73e9b8e7
 */

/******************************************************************************/

#ifndef BSDMALLOC_H_INCLUDED
# define BSDMALLOC_H_INCLUDED

/******************************************************************************/

# ifdef NDEBUG
#  ifdef DEBUG
#   undef DEBUG
#  endif
# endif

/******************************************************************************/

# ifdef __FreeBSD__
#  ifdef DEBUG
#   include <sys/param.h>
#   if !(__FreeBSD_version < 1000011)
const char * malloc_conf = "abort:true,confirm_conf:true,junk:true";
#   else
const char * malloc_conf = "JR";
#   endif
#  endif
# endif

/******************************************************************************/

# ifdef __NetBSD__
#  ifdef DEBUG
const char * malloc_conf = "abort:true,junk:true";
#  endif
# endif

/******************************************************************************/

# ifdef __OpenBSD__
#  ifdef DEBUG
const char * const malloc_options = "CFGJRU";
#  else
const char * const malloc_options = "j";
#  endif
# endif

/******************************************************************************/

#endif

/******************************************************************************/

/*
 * Local Variables:
 * mode: c
 * indent-tabs-mode: nil
 * tab-width: 2
 * c-basic-offset: 2
 * fill-column: 80
 * eval: (setq-local display-fill-column-indicator-column 80)
 * eval: (display-fill-column-indicator-mode 1)
 * End:
 */

/******************************************************************************/
/* vim: set ft=c ts=2 sw=2 tw=0 ai expandtab cc=80 : */
/******************************************************************************/
