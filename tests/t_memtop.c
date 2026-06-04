/*
 * MEMTOP (-m) value parser unit tests
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: 31398bae-6021-11f1-b302-80ee73e9b8e7
 */

/******************************************************************************/

#define main lzpack_main_unused
#include "../lzpack.c"
#undef main

/******************************************************************************/

static int fails = 0;

static void
check (const char *in, unsigned want)
{
  unsigned got = parse_memtop (in);

  if (got == want)
    (void)printf ("  [PASS] -m %-8s -> %u\n", in, got);
  else
    {
      (void)printf ("  [FAIL] -m %-8s (got %u, want %u)\n", in, got, want);
      fails++;
    }
}

/******************************************************************************/

int
main (void)
{
  char nb[16];

#ifdef LZPACK_STREAM
  (void)printf ("\nmemtop unit test (streaming):\n");
#else
  (void)printf ("\nmemtop unit test (in-RAM):\n");
#endif

  /* KB form: K*1024 - 0x201; 48 must equal the built-in default */
  check ("48", MEMTOP);
  check ("5", 0x11FFU);
  check ("16", 0x3DFFU);
  check ("32", 0x7DFFU);
  check ("60", 0xEDFFU);
  check ("64", 0xFDFFU);
  check ("05", 0x11FFU); /* leading zero is still decimal */

  /* a trailing K or k is accepted and equals the bare KB form */
  check ("32K", 0x7DFFU);
  check ("32k", 0x7DFFU);
  check ("64K", 0xFDFFU);
  check ("5k", 0x11FFU);

  /* 0x/0X hex address, either digit case (the CCP upper-cases the tail) */
  check ("0x7DFF", 0x7DFFU);
  check ("0X7DFF", 0x7DFFU);
  check ("0x7dff", 0x7DFFU);
  check ("0xbdff", MEMTOP);
  check ("0xFFFF", 0xFFFFU);

  /* a bare decimal above 64 is a literal address */
  check ("65023", 0xFDFFU);
  check ("65535", 0xFFFFU);

  /* the exact floor, decimal and hex, both sides (0 == rejected) */
  (void)sprintf (nb, "%u", (unsigned)MEMTOP_MIN);
  check (nb, (unsigned)MEMTOP_MIN);
  (void)sprintf (nb, "%u", (unsigned)MEMTOP_MIN - 1);
  check (nb, 0);
  (void)sprintf (nb, "0x%X", (unsigned)MEMTOP_MIN);
  check (nb, (unsigned)MEMTOP_MIN);
  (void)sprintf (nb, "0x%x", (unsigned)MEMTOP_MIN - 1);
  check (nb, 0);

  /* out of range */
  check ("0", 0);
  check ("4", 0); /* 4K - 0x201 is below the floor */
  check ("1K", 0);
  check ("65K", 0); /* 65K - 0x201 is past 0xFFFF */
  check ("65536", 0);
  check ("99999", 0);
  check ("999999", 0);
  check ("0x10000", 0);
  check ("0xFFFFF", 0);

  /* malformed */
  check ("", 0);
  check ("K", 0);
  check ("0x", 0);
  check ("12Q", 0);
  check ("0x7DFFK", 0);
  check ("-1", 0);
  check ("64KB", 0);

  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)printf (fails ? "\n  **** UNIT TEST FAILED ****\n"
                      : "\n  **** unit test ok ****\n");

  return (fails ? 1 : 0);
}

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
