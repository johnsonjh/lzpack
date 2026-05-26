/*
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: a5653bbc-585c-11f1-954d-80ee73e9b8e7
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* //-V::707 */

/*
 * MZXFILE - input/output file (bytes).
 *           lower it for small-memory targets (e.g. -DMZXFILE=49152L).
 * HSZ - compressor hash table size (power of two).
 * POPCOM_DECODE_ONLY - build a restore/list-only utility
 */

#ifndef MZXFILE
# define MZXFILE 262144L
#endif

#ifndef HSZ
# define HSZ 32768
#endif

#ifndef BUFSZ
# define BUFSZ (MZXFILE + 512)
#endif

static unsigned char g_a[BUFSZ];
static unsigned char g_b[BUFSZ];

#if 0
# ifdef __Z88DK
#  pragma output CLIB_MALLOC_HEAP_SIZE = 2048
#  include <malloc.h>
# endif
#endif

#ifndef POPCOM_DECODE_ONLY
static unsigned char g_c[BUFSZ];

static void *
lxmalloc (size_t n)
{
  void *p = malloc (n);

  if (!p)
    {
      fprintf (stderr, "FATAL: Out of memory!\n");
      exit (1);
    }

  return p;
}
#endif

#define TPA 0x100
#define LITCNT 16
#define STUBLEN 230
#define MAXDIST 8192
#define MAXLEN 256
#define MEMTOP 0xBDFF

#ifndef POPCOM_DECODE_ONLY

static const unsigned char z80_stub[STUBLEN] = {
  0x21, 0x12, 0x0f, 0x11, 0x00, 0x01, 0x01, 0x10, 0x00, 0xed, 0xb0, 0x21, 0x07,
  0x10, 0x11, 0x75, 0x17, 0x01, 0xc4, 0x00, 0xed, 0xb8, 0x21, 0x11, 0x0f, 0x11,
  0x7f, 0x16, 0x01, 0x02, 0x0e, 0xc3, 0xb2, 0x16, 0xed, 0xb8, 0xeb, 0x23, 0x1e,
  0x80, 0xd9, 0x21, 0x10, 0x01, 0x7c, 0xfe, 0x16, 0x20, 0x09, 0x7d, 0xfe, 0x80,
  0x38, 0x04, 0xca, 0x00, 0x01, 0xc7, 0xd9, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23,
  0xcb, 0x02, 0x7e, 0x23, 0x38, 0x05, 0xd9, 0x77, 0x23, 0x18, 0xe0, 0xcb, 0x7f,
  0x20, 0x08, 0xd9, 0x16, 0x00, 0x5f, 0xd9, 0xaf, 0x18, 0x3a, 0xcb, 0x77, 0x20,
  0x21, 0xcb, 0xbf, 0x01, 0x00, 0x04, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb,
  0x02, 0x17, 0xcb, 0x11, 0x10, 0xf3, 0xc6, 0x80, 0xd9, 0x5f, 0xd9, 0x79, 0xce,
  0x00, 0xd9, 0x57, 0xd9, 0x3e, 0x01, 0x18, 0x15, 0xe6, 0x3f, 0xcb, 0x3f, 0xd9,
  0x57, 0xd9, 0x7e, 0x23, 0x1f, 0xd9, 0x5f, 0xd9, 0x3e, 0x02, 0x30, 0x46, 0x0e,
  0x01, 0x18, 0x0c, 0x4f, 0x3c, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb, 0x02,
  0x30, 0x36, 0x3c, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb, 0x02, 0x30, 0x2b,
  0x3c, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb, 0x02, 0x30, 0x20, 0x3e, 0x02,
  0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb, 0x02, 0x30, 0x05, 0x3c, 0xfe, 0x07,
  0x20, 0xf1, 0x47, 0x3e, 0x01, 0xcb, 0x03, 0x30, 0x02, 0x56, 0x23, 0xcb, 0x02,
  0x17, 0x10, 0xf5, 0x81, 0xd9, 0x44, 0x4d, 0x37, 0xed, 0x52, 0x50, 0x59, 0x06,
  0x00, 0x4f, 0x03, 0xed, 0xb0, 0xeb, 0xc3, 0xbc, 0x16
};

# define P_LIT_SRC 0x01
# define P_STUB_SRCTOP 0x0c
# define P_STUB_DSTTOP 0x0f
# define P_PL_SRCTOP 0x17
# define P_PL_DSTTOP 0x1a
# define P_PL_LEN 0x1d
# define P_JP_RELOC 0x20
# define P_CP_HI 0x2e
# define P_CP_LO 0x33
# define P_JP_LOOP 0xe4

# include "cs8080.h"

static unsigned char *ob;
static long ol, tagpos;
static int tagcnt;
static void
e_init (unsigned char *buf)
{
  ob = buf;
  ol = 0;
  tagpos = -1;
  tagcnt = 8;
}
static void
e_bit (int b)
{
  if (tagcnt == 8)
    {
      tagpos = ol;
      ob[ol++] = 0;
      tagcnt = 0;
    }

  if (b)
    {
      ob[tagpos] |= (unsigned char)(1 << (7 - tagcnt));
    }

  tagcnt++;
}
static void
e_byte (int x)
{
  ob[ol++] = (unsigned char)(x & 0xff);
}

static void
e_bits (unsigned v, int n)
{
  int i;

  for (i = n - 1; i >= 0; i--)
    {
      e_bit ((int)((v >> i) & 1));
    }
}

static void
e_extlen (int v)
{
  int B = 0, t = v, ones, i;

  while (t > 1)
    {
      t >>= 1;
      B++;
    }

  ones = B - 2;

  for (i = 0; i < ones; i++)
    {
      e_bit (1);
    }

  if (B < 7)
    {
      e_bit (0);
    }

  e_bits ((unsigned)(v & ((1 << B) - 1)), B);
}

static void
e_len (int L, int c)
{
  int b2 = c + 2;

  if (L == b2)
    {
      e_bit (0);
      return;
    }

  if (L == b2 + 1)
    {
      e_bit (1);
      e_bit (0);
      return;
    }

  if (L == b2 + 2)
    {
      e_bit (1);
      e_bit (1);
      e_bit (0);
      return;
    }

  e_bit (1);
  e_bit (1);
  e_bit (1);
  e_extlen (L - c - 1);
}

static void
e_len3 (int L)
{
  if (L == 4)
    {
      e_bit (0);
      return;
    }

  if (L == 5)
    {
      e_bit (1);
      e_bit (0);
      return;
    }

  e_bit (1);
  e_bit (1);
  e_extlen (L - 2);
}
static void
e_lit (int byte)
{
  e_bit (0);
  e_byte (byte);
}
static void
e_match (int dist, int L)
{
  int off = dist - 1;

  if (dist <= 128)
    {
      e_bit (1);
      e_byte (off & 0x7f);
      e_len (L, 0);
    }
  else if (dist <= 1152)
    {
      int val = off - 0x80;
      e_bit (1);
      e_byte (0x80 | ((val >> 4) & 0x3f));
      e_bits ((unsigned)(val & 0xf), 4);
      e_len (L, 1);
    }
  else
    {
      int b0 = (L == 3) ? 0 : 1;

      e_bit (1);
      e_byte (0xC0 | ((off >> 7) & 0x3f));
      e_byte (((off & 0x7f) << 1) | b0);

      if (L >= 4)
        {
          e_len3 (L);
        }
    }
}

static int head[HSZ];
static int *lnk;
static const unsigned char *D;
static long N;

static int
hash3 (long i)
{
  return (int)((((unsigned)D[i] << 10) ^ ((unsigned)D[i + 1] << 5) ^ D[i + 2])
               & (HSZ - 1));
}

static void
hinsert (long i)
{
  int h;

  if (i + 2 >= N)
    {
      return;
    }

  h = hash3 (i);
  lnk[i] = head[h];
  head[h] = (int)i;
}
static int
mlen_min (int dist)
{
  return (dist <= 128) ? 2 : 3;
}

static int
findmatch (long i, int *bestdist, int maxdepth)
{
  int h, bl = 0, bd = 0, depth = maxdepth;
  long p;

  if (i + 2 >= N)
    {
      return 0;
    }

  h = hash3 (i);

  for (p = head[h]; p >= 0 && depth-- > 0; p = lnk[p])
    {
      long d = i - p;
      int ml, mx;

      if (d > MAXDIST)
        {
          break;
        }

      mx = MAXLEN;

      if (mx > (int)(N - i))
        {
          mx = (int)(N - i);
        }

      if (bl > 0 && bl < mx && D[p + bl] != D[i + bl])
        {
          continue;
        }

      ml = 0;

      while (ml < mx && D[p + ml] == D[i + ml])
        {
          ml++;
        }

      if (ml < mlen_min ((int)d))
        {
          continue;
        }

      if (ml > bl || (ml == bl && d < bd))
        {
          bl = ml;
          bd = (int)d;

          if (bl >= mx)
            {
              break;
            }
        }
    }

  *bestdist = bd;
  return bl;
}

static long
compress (const unsigned char *data, long n, int start, unsigned char *out,
          int depth)
{
  long i;
  int d, L, d2, L2;

  D = data;
  N = n;
  lnk = (int *)lxmalloc (sizeof (int) * (size_t)(n > 0 ? n : 1));
  {
    int j;

    for (j = 0; j < HSZ; j++)
      {
        head[j] = -1;
      }
  }
  e_init (out);

  for (i = 0; i < start && i + 2 < n; i++)
    {
      hinsert (i);
    }

  i = start;

  while (i < n)
    {
      d = 0;
      L = findmatch (i, &d, depth);

      if (L >= mlen_min (d))
        {
          d2 = 0;
          L2 = 0;

          if (i + 1 < n)
            {
              hinsert (i);
              L2 = findmatch (i + 1, &d2, depth);
            }

          if (L2 > L)
            {
              e_lit (data[i]);
              i++;
              continue;
            }

          e_match (d, L);
          {
            long e = i + L;

            i++;

            for (; i < e; i++)
              {
                hinsert (i);
              }
          }
        }
      else
        {
          e_lit (data[i]);
          hinsert (i);
          i++;
        }
    }
  free (lnk);
  return ol;
}

# ifndef POPCOM_NO_OPT
static int
extlen_bits (int v)
{
  int B = 0, t = v;

  while (t > 1)
    {
      t >>= 1;
      B++;
    }
  return (B < 7 ? (B - 2) + 1 : 5) + B;
}
static int
len_bits (int L, int c)
{
  int b2 = c + 2;

  if (L == b2)
    {
      return 1;
    }

  if (L == b2 + 1)
    {
      return 2;
    }

  if (L == b2 + 2)
    {
      return 3;
    }

  return 3 + extlen_bits (L - c - 1);
}
static int
len3_bits (int L)
{
  if (L == 3)
    {
      return 0;
    }

  if (L == 4)
    {
      return 1;
    }

  if (L == 5)
    {
      return 2;
    }

  return 2 + extlen_bits (L - 2);
}
static int
match_bits (int dist, int L)
{
  if (dist <= 128)
    {
      return 1 + 8 + len_bits (L, 0);
    }

  if (dist <= 1152)
    {
      return 1 + 8 + 4 + len_bits (L, 1);
    }

  return 1 + 16 + len3_bits (L);
}

static long
compress_opt (const unsigned char *data, long n, int start, unsigned char *out,
              int depth)
{
  long i;
  long *cost;
  int *tlen, *tdist, *head2;
  static int l2d[MAXLEN + 1];

  D = data;
  N = n;
  lnk = (int *)lxmalloc (sizeof (int) * (size_t)(n > 0 ? n : 1));
  cost = (long *)lxmalloc (sizeof (long) * (size_t)(n + 1));
  tlen = (int *)lxmalloc (sizeof (int) * (size_t)(n + 1));
  tdist = (int *)lxmalloc (sizeof (int) * (size_t)(n + 1));
  head2 = (int *)lxmalloc (sizeof (int) * (size_t)65536U);
  {
    long j;

    for (j = 0; j < HSZ; j++)
      {
        head[j] = -1;
      }

    for (j = 0; j < 65536; j++)
      {
        head2[j] = -1;
      }
  }

  for (i = 0; i < start && i + 2 < n; i++)
    {
      hinsert (i);
      head2[data[i] | (data[i + 1] << 8)] = (int)i;
    }

  for (i = start; i <= n; i++)
    {
      cost[i] = 0x3fffffffL;
      tlen[i] = 0;
      tdist[i] = 0;
    }

  cost[start] = 0;

  for (i = start; i < n; i++)
    {
      if (cost[i] != 0x3fffffffL)
        {
          if (cost[i] + 9 < cost[i + 1])
            {
              cost[i + 1] = cost[i] + 9;
              tlen[i + 1] = 1;
              tdist[i + 1] = 0;
            }

          if (i + 2 < n)
            {
              int h = hash3 (i), dep = depth, maxml = 0, cap = MAXLEN;
              long p;

              if (cap > (int)(n - i))
                {
                  cap = (int)(n - i);
                }

              for (p = head[h]; p >= 0 && dep-- > 0; p = lnk[p])
                {
                  long d = i - p;
                  int ml;

                  if (d > MAXDIST)
                    {
                      break;
                    }

                  if (maxml > 0 && maxml < cap && D[p + maxml] != D[i + maxml])
                    {
                      continue;
                    }

                  ml = 0;

                  while (ml < cap && D[p + ml] == D[i + ml])
                    {
                      ml++;
                    }

                  if (ml > maxml)
                    {
                      int L;

                      for (L = maxml + 1; L <= ml; L++)
                        {
                          l2d[L] = (int)d;
                        }

                      maxml = ml;

                      if (maxml >= cap)
                        {
                          break;
                        }
                    }
                }

              {
                int L;

                for (L = 3; L <= maxml; L++)
                  {
                    int d = l2d[L];
                    long c2 = cost[i] + match_bits (d, L);

                    if (c2 < cost[i + L])
                      {
                        cost[i + L] = c2;
                        tlen[i + L] = L;
                        tdist[i + L] = d;
                      }
                  }
              }
            }

          if (i + 1 < n)
            {
              int p2 = head2[data[i] | (data[i + 1] << 8)];

              if (p2 >= 0 && (i - p2) <= 128)
                {
                  long c2 = cost[i] + match_bits ((int)(i - p2), 2);

                  if (c2 < cost[i + 2])
                    {
                      cost[i + 2] = c2;
                      tlen[i + 2] = 2;
                      tdist[i + 2] = (int)(i - p2);
                    }
                }
            }
        }

      hinsert (i);

      if (i + 1 < n)
        {
          head2[data[i] | (data[i + 1] << 8)] = (int)i;
        }
    }

  {
    long *st = (long *)lxmalloc (sizeof (long) * (size_t)(n + 1));
    long sp = 0, k;

    for (k = n; k > start;)
      {
        st[sp++] = k;
        k -= (tlen[k] > 1 ? tlen[k] : 1);
      }

    e_init (out);

    for (k = sp - 1; k >= 0; k--)
      {
        long e = st[k];
        int L = tlen[e];

        if (L > 1)
          {
            e_match (tdist[e], L);
          }
        else
          {
            e_lit (data[e - 1]);
          }
      }

    free (st);
  }
  free (lnk);
  free (cost);
  free (tlen);
  free (tdist);
  free (head2);
  return ol;
}
# endif
#endif

#ifndef POPCOM_COMPRESS_ONLY
static const unsigned char *ip;
static int dbc;
static unsigned dbv;
static int
g_bit (void)
{
  int b;

  if (dbc == 0)
    {
      dbv = *ip++;
      dbc = 8;
    }

  b = (dbv >> 7) & 1;
  dbv = (dbv << 1) & 0xff;
  dbc--;
  return b;
}

static long
decode (const unsigned char *pl, unsigned char *out, long outlen, int litcnt)
{
  unsigned char *op = out + litcnt;
  int ctrl, a, b, c, bit, i;
  unsigned off, ml;
  unsigned char *mp;

  ip = pl;
  dbc = 0;

  while ((long)(op - out) < outlen)
    {
      ctrl = g_bit ();
      a = *ip++;

      if (!ctrl)
        {
          *op++ = (unsigned char)a;
          continue;
        }

      if (!(a & 0x80))
        {
          off = (unsigned)a;
          a = 0;
          goto lf;
        }
      else if (!(a & 0x40))
        {
          a &= 0x7f;
          b = 4;
          c = 0;

          do
            {
              int cy;
              bit = g_bit ();
              cy = (a >> 7) & 1;
              a = ((a << 1) | bit) & 0xff;
              c = ((c << 1) | cy) & 0xff;
            }

          while (--b);
          {
            int t = a + 0x80;
            a = t & 0xff;
            off = ((unsigned)(c + (t >> 8)) << 8) | (unsigned)a;
          }

          a = 1;
          goto lf;
        }
      else
        {
          int cy, nc, oh, ol2;

          a &= 0x3f;
          cy = a & 1;
          a >>= 1;
          oh = a;
          a = *ip++;
          nc = a & 1;
          a = ((cy << 7) | (a >> 1)) & 0xff;
          cy = nc;
          ol2 = a;
          off = ((unsigned)oh << 8) | (unsigned)ol2;
          a = 2;

          if (!cy)
            {
              ml = a + 1;
              goto cp;
            }

          c = 1;
          goto lc;
        }

    lf:
      c = a;
      a++;
      bit = g_bit ();

      if (!bit)
        {
          ml = a + 1;
          goto cp;
        }

    lc:
      a++;
      bit = g_bit ();

      if (!bit)
        {
          ml = a + 1;
          goto cp;
        }

      a++;
      bit = g_bit ();

      if (!bit)
        {
          ml = a + 1;
          goto cp;
        }

      a = 2;
      for (;;)
        {
          bit = g_bit ();

          if (!bit)
            {
              break;
            }

          a++;

          if (a == 7)
            {
              break;
            }
        }

      b = a;
      a = 1;

      do
        {
          bit = g_bit ();
          a = ((a << 1) | bit) & 0xff;
        }

      while (--b);

      a = (a + c) & 0xff;
      ml = a + 1;

    cp:
      mp = op - (off + 1);

      for (i = 0; i < (int)ml; i++)
        {
          *op++ = *mp++;
        }
    }
  return (long)(op - out);
}
#endif

#ifndef POPCOM_DECODE_ONLY
static long
min_gap (const unsigned char *pl, long pl_len, long outlen, int litcnt,
         long pl_dst_top)
{
  long src_base = pl_dst_top + 1 - pl_len;
  long dst_base = TPA + litcnt;
  const unsigned char *p = pl;
  int bc = 0;
  unsigned bv = 0;
  long produced = 0;
  long consumed;
  long gap, ming = 0x7fffffffL;
  int first = 1;
  int ctrl, a, b, c, bit;
  unsigned ml;
  long k;

  (void)pl_len;

  while (produced < outlen)
    {
      consumed = (long)(p - pl);
      gap = (src_base + consumed) - (dst_base + produced);

      if (first || gap < ming)
        {
          ming = gap;
          first = 0;
        }

      if (bc == 0)
        {
          bv = *p++;
          bc = 8;
        }

      ctrl = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;
      a = *p++;

      if (!ctrl)
        {
          produced++;
          continue;
        }

      if (!(a & 0x80))
        {
          a = 0;
          goto lf;
        }
      else if (!(a & 0x40))
        {
          b = 4;

          do
            {
              if (bc == 0)
                {
                  bv = *p++;
                  bc = 8;
                }

              bv = (bv << 1) & 0xff;
              bc--;
            }

          while (--b);

          a = 1;
          goto lf;
        }
      else
        {
          int b0;

          (void)(a & 0x3f);
          b0 = (*p++) & 1;
          a = 2;

          if (!b0)
            {
              ml = a + 1;
              goto cpx;
            }

          c = 1;
          goto lc;
        }

    lf:
      c = a;
      a++;

      if (bc == 0)
        {
          bv = *p++;
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = a + 1;
          goto cpx;
        }

    lc:
      a++;

      if (bc == 0)
        {
          bv = *p++;
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = a + 1;
          goto cpx;
        }

      a++;

      if (bc == 0)
        {
          bv = *p++;
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = a + 1;
          goto cpx;
        }

      a = 2;

      for (;;)
        {
          if (bc == 0)
            {
              bv = *p++;
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;

          if (!bit)
            {
              break;
            }

          a++;

          if (a == 7)
            {
              break;
            }
        }

      b = a;
      a = 1;

      do
        {
          if (bc == 0)
            {
              bv = *p++;
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;
          a = ((a << 1) | bit) & 0xff;
        }

      while (--b);

      a = (a + c) & 0xff;
      ml = a + 1;

    cpx:
      for (k = 0; k < (long)ml; k++)
        {
          produced++;
        }
    }
  (void)bit;
  return ming;
}
#endif

static long
readfile (const char *fn, unsigned char *buf, size_t max)
{
  FILE *f = fopen (fn, "rb");
  long n;

  if (!f)
    {
      return -1;
    }

  n = (long)fread (buf, 1, max, f);
  fclose (f);
  return n;
}

static int
writefile (const char *fn, const unsigned char *buf, long n)
{
  FILE *f = fopen (fn, "wb");

  if (!f)
    {
      return -1;
    }

  fwrite (buf, 1, (size_t)n, f);
  fclose (f);
  return 0;
}

#ifndef POPCOM_DECODE_ONLY
static void
put16 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)(v & 0xff);
  p[1] = (unsigned char)((v >> 8) & 0xff);
}
#endif

static unsigned
get16 (const unsigned char *p)
{
  return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static void
mkname (const char *in, const char *ext, char *out)
{
  const char *dot = strrchr (in, '.');
  const char *slash = strrchr (in, '/');
  size_t base;

  if (dot && (!slash || dot > slash))
    {
      base = (size_t)(dot - in);
    }
  else
    {
      base = strlen (in);
    }

  memcpy (out, in, base);
  strcpy (out + base, ext);
}

#ifndef POPCOM_DECODE_ONLY
static void
put_header (unsigned char *outf, const unsigned char *data, long stub_v,
            long outlen)
{
  memcpy (outf, data, LITCNT);
  outf[0] = 0xc3;
  put16 (outf + 1, (unsigned)stub_v);
  memcpy (outf + 5, "-pc1-", 5);
  put16 (outf + 10, (unsigned)outlen);
  outf[12] = outf[13] = outf[14] = outf[15] = 0;
}

static long
build_z80 (unsigned char *outf, const unsigned char *data, long pllen,
           const unsigned char *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long stub_dst_top = pl_dst_top + 246;
  unsigned char *stub;

  if (stub_dst_top > MEMTOP)
    {
      return -1;
    }

  put_header (outf, data, stub_v, outlen);
  memcpy (outf + LITCNT, pl, (size_t)pllen);
  memcpy (outf + LITCNT + pllen, data, LITCNT);
  stub = outf + LITCNT + pllen + LITCNT;
  memcpy (stub, z80_stub, STUBLEN);
  put16 (stub + P_LIT_SRC, (unsigned)lit_src);
  put16 (stub + P_STUB_SRCTOP, (unsigned)(stub_v + 0xe5));
  put16 (stub + P_STUB_DSTTOP, (unsigned)stub_dst_top);
  put16 (stub + P_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (stub + P_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (stub + P_PL_LEN, (unsigned)pllen);
  put16 (stub + P_JP_RELOC, (unsigned)(stub_dst_top - 195));
  stub[P_CP_HI] = (unsigned char)((out_end >> 8) & 0xff);
  stub[P_CP_LO] = (unsigned char)(out_end & 0xff);
  put16 (stub + P_JP_LOOP, (unsigned)(stub_dst_top - 195 + 0x0a));
  return LITCNT + pllen + LITCNT + STUBLEN;
}

static long
build_8080 (unsigned char *outf, const unsigned char *data, long pllen,
            const unsigned char *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long decomp_file_v = stub_v + S8_SLEN;
  long stub_run = pl_dst_top + 51;
  long dcmp_dsttop = stub_run + S8_DLEN - 1;
  long pl_dstbot = pl_dst_top + 1 - pllen;
  unsigned char *su, *de;
  int i;

  if (dcmp_dsttop > MEMTOP)
    {
      return -1;
    }

  put_header (outf, data, stub_v, outlen);

  memcpy (outf + LITCNT, pl, (size_t)pllen);
  memcpy (outf + LITCNT + pllen, data, LITCNT);

  su = outf + LITCNT + pllen + LITCNT;
  de = su + S8_SLEN;

  memcpy (su, setup8080, S8_SLEN);
  memcpy (de, decomp8080, S8_DLEN);

  for (i = 0; i < SETUP8080_FIX_N; i++)
    {
      put16 (su + setup8080_fix[i][0],
             (unsigned)(stub_v + setup8080_fix[i][1]));
    }

  put16 (su + S8S_LIT_SRC, (unsigned)lit_src);
  put16 (su + S8S_DCMP_SRCTOP, (unsigned)(decomp_file_v + S8_DLEN - 1));
  put16 (su + S8S_DCMP_DSTTOP, (unsigned)dcmp_dsttop);
  put16 (su + S8S_DCMP_LEN, (unsigned)S8_DLEN);
  put16 (su + S8S_DCMP_RUN, (unsigned)stub_run);

  for (i = 0; i < DECOMP8080_FIX_N; i++)
    {
      put16 (de + decomp8080_fix[i][0],
             (unsigned)(stub_run + decomp8080_fix[i][1]));
    }

  de[S8D_OUT_END_HI] = (unsigned char)((out_end >> 8) & 0xff);
  de[S8D_OUT_END_LO] = (unsigned char)(out_end & 0xff);
  put16 (de + S8D_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (de + S8D_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (de + S8D_PL_LEN, (unsigned)pllen);
  put16 (de + S8D_PL_DSTBOT, (unsigned)pl_dstbot);
  return LITCNT + pllen + LITCNT + S8_SLEN + S8_DLEN;
}

static int
do_compress (const char *fn, const char *oname, int verbose, int use8080,
             int optimal)
{
  unsigned char *data = g_a, *pl = g_b, *outf = g_c;
  long n, pllen, outlen, pl_dst_top, ming, pad, total, body;
  char nb[1024];

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      fprintf (stderr, "FATAL: cannot read %s\n", fn);
      return 1;
    }

  if (n > MZXFILE)
    {
      fprintf (stderr, "FATAL: %s exceeds MZXFILE=%ld (build constraint)\n", fn,
               (long)MZXFILE);
      return 1;
    }

  if (n <= LITCNT + 32)
    {
      fprintf (stderr, "FATAL: %s too small\n", fn);
      return 1;
    }

# ifdef POPCOM_NO_OPT
  if (optimal && verbose)
    {
      fprintf (stderr, "  (note: -e is not available in this build)\n");
    }

  pllen = compress (data, n, LITCNT, pl, 1024);
# else
  pllen = optimal ? compress_opt (data, n, LITCNT, pl, 4096)
                  : compress (data, n, LITCNT, pl, 1024);
# endif
  outlen = n;
  pl_dst_top = (long)(TPA + outlen) - 1;
  ming = min_gap (pl, pllen, outlen - LITCNT, LITCNT, pl_dst_top);

  if (ming < 1)
    {
      pl_dst_top += (1 - ming);
    }

  body = use8080 ? build_8080 (outf, data, pllen, pl, outlen, pl_dst_top)
                 : build_z80 (outf, data, pllen, pl, outlen, pl_dst_top);

  if (body < 0)
    {
      fprintf (stderr, "FATAL: %s would not fit in memory\n", fn);
      return 1;
    }

  total = body;
  pad = (128 - (total % 128)) % 128;

  if (pad)
    {
      memset (outf + total, 0, (size_t)pad);
    }

  total += pad;

  if (total >= n)
    {
      if (verbose)
        {
          fprintf (stderr, "  %-12s -- inefficient (%ld => %ld), skipped\n",
                   fn, n, total);
        }

      return 2;
    }

  if (!oname)
    {
      mkname (fn, ".pop", nb);
      oname = nb;
    }

  if (writefile (oname, outf, total))
    {
      fprintf (stderr, "FATAL: cannot write %s\n", oname);
      return 1;
    }

  if (verbose)
    {
      fprintf (stderr, "  %-12s %6ld => %6ld  (%.1f%%)  [%s]  -> %s\n", fn, n,
               total, 100.0 * total / n, use8080 ? "8080" : "Z80", oname);
    }

  return 0;
}
#endif

static int
parse_header (const unsigned char *data, long n, unsigned *stubv,
              unsigned *lit_src, long *outlen)
{
  unsigned sv;

  if (data[0] == 0xc3)
    {
      sv = get16 (data + 1);
    }
  else if (data[0] == 0x18 && data[2] == 0xc3)
    {
      sv = get16 (data + 3);
    }
  else
    {
      return 1;
    }

  if (memcmp (data + 5, "-pc1-", 5) != 0)
    {
      return 1;
    }

  *stubv = sv;
  *lit_src = sv - LITCNT;
  *outlen = (long)get16 (data + 10);

  if (sv < 0x120 || *outlen <= 0 || (long)(sv - TPA) > n)
    {
      return 1;
    }

  return 0;
}

#ifndef POPCOM_COMPRESS_ONLY
static int
do_restore (const char *fn, const char *oname, int verbose)
{
  unsigned char *data = g_a, *out = g_b;
  long n, outlen, pstart;
  unsigned stubv, lit_src;
  char nb[1024];

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      fprintf (stderr, "FATAL: cannot read %s\n", fn);
      return 1;
    }

  if (parse_header (data, n, &stubv, &lit_src, &outlen))
    {
      fprintf (stderr, "FATAL: %s is not a POPCOM/LZPACK file\n", fn);
      return 1;
    }

  pstart = TPA + LITCNT - TPA;

  if (outlen > MZXFILE)
    {
      fprintf (stderr, "FATAL: %s expands beyond MZXFILE=%ld\n", fn,
               (long)MZXFILE);
      return 1;
    }

  memcpy (out, data + ((long)lit_src - TPA), LITCNT);
  decode (data + pstart, out, outlen - LITCNT, LITCNT);

  if (!oname)
    {
      mkname (fn, ".unp", nb);
      oname = nb;
    }

  if (writefile (oname, out, outlen))
    {
      fprintf (stderr, "FATAL: cannot write %s\n", oname);
      return 1;
    }

  if (verbose)
    {
      fprintf (stderr, "  %-12s %6ld => %6ld  -> %s\n", fn, n, outlen, oname);
    }

  return 0;
}
#endif

static int
do_list (const char *fn)
{
  unsigned char *data = g_a;
  long n, outlen;
  unsigned stubv, lit_src;

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      fprintf (stderr, "FATAL: cannot read %s\n", fn);
      return 1;
    }

  if (parse_header (data, n, &stubv, &lit_src, &outlen))
    {
      printf ("  %-16s (not a POPCOM file)\n", fn);
      return 0;
    }

  printf ("  %-16s compressed %6ld   original %6ld   (%.1f%%)\n", fn, n,
          outlen, 100.0 * n / (double)outlen);
  return 0;
}

static void
usage (void)
{
  fprintf (stderr, "LZPACK - PopCom!-compatible CP/M-80 executable compressor\n"
                   "Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>\n"
                   "\nUsage:\n"
#ifndef POPCOM_DECODE_ONLY
                   "  lzpack [-e] [-8] <file>  compress (-e: extra, -8: 8080 compatible)\n"
#endif
#ifndef POPCOM_COMPRESS_ONLY
                   "  lzpack -R <file>         restore (decompress)\n"
#endif
                   "  lzpack -L <file>         list stored sizes\n"
                   "  lzpack -o <name>         set output name\n");
}

int
main (int argc, char **argv)
{
  int mode = 0;
  int i, rc = 0, any = 0;
  const char *oname = 0;
  int use8080 = 0, optimal = 0;

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1] && !oname)
        {
          char c = argv[i][1];

          if (c == 'R' || c == 'r')
            {
              mode = 1;
            }
          else if (c == 'L' || c == 'l')
            {
              mode = 2;
            }
          else if (c == '8')
            {
              use8080 = 1;
            }
          else if (c == 'e' || c == 'E')
            {
              optimal = 1;
            }
          else if (c == 'o')
            {
              if (i + 1 < argc)
                {
                  oname = argv[++i];
                }
            }
          else if (c == 'h')
            {
              usage ();
              return 0;
            }
          else
            {
              fprintf (stderr, "FATAL: unknown option %s\n", argv[i]);
              return 2;
            }

          continue;
        }

      any = 1;

      if (mode == 0)
        {
#ifdef POPCOM_DECODE_ONLY
          fprintf (stderr, "FATAL: this build cannot compress\n");
          rc |= 1;
#else
          rc |= do_compress (argv[i], oname, 1, use8080, optimal);
#endif
        }
      else if (mode == 1)
        {
#ifdef POPCOM_COMPRESS_ONLY
          fprintf (stderr, "FATAL: this build cannot restore\n");
          rc |= 1;
#else
          rc |= do_restore (argv[i], oname, 1);
#endif
        }
      else
        {
          rc |= do_list (argv[i]);
        }

      oname = 0;
    }

  (void)use8080;
  (void)optimal;

  if (!any)
    {
      usage ();
      return 2;
    }

  return rc ? 1 : 0;
}
