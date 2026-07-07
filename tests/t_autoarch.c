/*
 * Automatic architecture detection unit tests
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: a617ca28-5b10-11f1-a431-80ee73e9b8e7
 */

/******************************************************************************/

#define main lzpack_main_unused
#include "../lzpack.c"
#undef main

/******************************************************************************/

static int fails = 0;

static void
check (int got, int want, const char *what)
{
  if (got == want)
    (void)printf ("  [PASS] %s\n", what);
  else
    {
      (void)printf ("  [FAIL] %s (got %d, want %d)\n", what, got, want);
      fails++;
    }
}

/******************************************************************************/

static int
detect (const unsigned char *buf, long len, long n)
{
#ifdef LZPACK_STREAM
  FILE *f = tmpfile ();
  int r;

  if (!f)
    return -1;

  (void)fwrite (buf, 1, (size_t)len, f);
  rewind (f);
  r = is_z80_file (f, n);
  (void)fclose (f);

  return r;
#else
  (void)len;

  return is_z80_image (buf, n);
#endif
}

/******************************************************************************/

int
main (void)
{
  unsigned char b [64];
  long i;

#ifdef LZPACK_STREAM
  (void)printf ("\nautoarch unit test: is_z80_file:\n");
#else
  (void)printf ("\nautoarch unit test: is_z80_image:\n");
#endif

  for (i = 0; i < 64; i++)
    b [i] = 0x00;

  b [20] = 0xED;

  check (detect (b, 64, 20), 0, "ED at 20 ignored when n=20 (in padding)");
  check (detect (b, 64, 21), 1, "ED at 20 seen when n=21 (in range)");
  check (detect (b, 64, 64), 1, "ED at 20 seen when n=64");

  b [20] = 0x00;
  b [10] = 0xCB;

  check (detect (b, 64, 64), 1, "CB recognized");

  b [10] = 0x00;
  b [10] = 0xDD;

  check (detect (b, 64, 64), 1, "DD recognized");

  b [10] = 0x00;
  b [10] = 0xED;

  check (detect (b, 64, 64), 1, "ED recognized");

  b [10] = 0x00;
  b [10] = 0xFD;

  check (detect (b, 64, 64), 1, "FD recognized");

  b [10] = 0x00;
  b [0] = 0x21;
  b [1] = 0x00;
  b [2] = 0xED;

  check (detect (b, 64, 64), 0, "ED as an LXI operand is not a prefix");

  b [0] = b [1] = b [2] = 0x00;
  check (detect (b, 64, 64), 0, "all-8080 image stays 8080");

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
