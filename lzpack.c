/*
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: a5653bbc-585c-11f1-954d-80ee73e9b8e7
 */

/******************************************************************************/

/* //-V::707 */

/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#ifdef LZPACK_VER
# undef LZPACK_VER
#endif

#define LZPACK_VER "v0.8-dev"

/******************************************************************************/

#ifndef MZXFILE
# define MZXFILE 262144L
#endif

/******************************************************************************/

#ifdef POPCOM_STREAM
# ifndef POPCOM_NO_OPT
#  define POPCOM_NO_OPT
# endif
#endif

/******************************************************************************/

#ifndef HSZ
# ifdef POPCOM_STREAM
#  define HSZ 1024
# else
#  define HSZ 32768
# endif
#endif

/******************************************************************************/

#ifdef POPCOM_STREAM
static const int never =0;
# define FREE(p) \
  do {          \
    free((p));  \
    (p) = NULL; \
  } while(never)
#endif

/******************************************************************************/

/*
 * BUFSZ sizes the in-RAM whole-file buffers used by the non-streaming build.
 * The streaming build does not use it: -R/-L allocate exactly what they need
 * and compression streams through the dynamically-sized window.
 */

#ifndef POPCOM_STREAM
# ifndef BUFSZ
#  define BUFSZ (MZXFILE + 512)
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef CPM
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef __CPM__
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef CPM80
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef __CPM80__
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef CPM86
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef __CPM86__
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef LZ_CPM
# ifdef __AZTEC_C_42T__
#  define LZ_CPM
# endif
#endif

/******************************************************************************/

#ifndef POPCOM_STREAM
static unsigned char g_a[BUFSZ];
static unsigned char g_b[BUFSZ];
#endif

/******************************************************************************/

/*
 * Default self-extracting stub architecture.  z88dk predefines __8080 when its
 * 8080 library is in use (-clib=8080) and __Z80 for a Z80 build; on an 8080
 * host we default to the 8080 stub.  POPCOM_8080 may also be set by the build
 * to force that default.  -8 always selects the 8080 stub and -Z the Z80 stub.
 */

#ifdef __8080
# ifndef POPCOM_8080
#  define POPCOM_8080
# endif
#endif

#ifdef POPCOM_8080
# define DEFAULT_USE8080 1
#else
# define DEFAULT_USE8080 0
#endif

/******************************************************************************/

#ifndef POPCOM_DECODE_ONLY
# ifndef POPCOM_STREAM
static unsigned char g_c[BUFSZ];

static void *
lxmalloc (size_t n)
{
  void *p = malloc (n);

  if (!p)
    {
      (void)fprintf (stderr, "FATAL: Out of memory!\n");

      exit (1);
    }

  return p;
}
# endif
#endif

/******************************************************************************/

#define TPA 0x100
#define LITCNT 16
#define STUBLEN 230
#define MAXDIST 8192
#define MAXLEN 256
#define MEMTOP 0xBDFF

/******************************************************************************/

#ifndef POPCOM_DECODE_ONLY

# include "csz80.h"
# include "cs8080.h"

/******************************************************************************/

static long ol, tagpos;
static int tagcnt;

/******************************************************************************/

# ifndef POPCOM_STREAM

static unsigned char *ob;

/******************************************************************************/

static void
e_init (unsigned char *buf)
{
  ob = buf;
  ol = 0;
  tagpos = -1;
  tagcnt = 8;
}

/******************************************************************************/

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
    ob[tagpos] |= (unsigned char)(1 << (7 - tagcnt));

  tagcnt++;
}

/******************************************************************************/

static void
e_byte (int x)
{
  ob[ol++] = (unsigned char)(x & 0xff);
}

# else

/******************************************************************************/

#  ifdef __AZTEC_C_42T__
static char *
xmemmove(char *dst, const char *src, unsigned int n)
{
  char *d = dst;
  const char *s = src;

  if ((unsigned long)d <= (unsigned long)s) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }

  return dst;
}
#   undef memmove
#   define memmove xmemmove /* //-V1059 */
#  endif

/******************************************************************************/

#  ifdef __AZTEC_C_42T__
#   ifndef NEED_REMOVE
#    define NEED_REMOVE
#   endif
#  endif

#  ifdef __IA16_SYS_ELKS
#  include <unistd.h>
#   ifndef NEED_REMOVE
#    define NEED_REMOVE
#   endif
#  endif

#  ifdef NEED_REMOVE
static int
xremove (const char *filename)
{
  return unlink (filename);
}
#   undef remove
#   define remove xremove /* //-V1059 */
#  endif

/******************************************************************************/

/*
 * Streaming encoder: the payload is written to a temp file as it is produced,
 * so only a small hold buffer is kept in RAM.  The single tag byte is the only
 * byte ever modified after being written; everything strictly before it is
 * final and may be flushed.  At most a handful of bytes accumulate between tag
 * boundaries, so OBSZ need only be small.
 */

#  ifndef OBSZ
#   define OBSZ 256
#  endif

static FILE *s_of;
static unsigned char s_obuf[OBSZ];
static long s_obase;

/******************************************************************************/

static void
obuf_flush (long upto)
{
  long cnt = upto - s_obase;

  if (cnt > 0)
    {
      unsigned int len;
      int off;

      (void)fwrite (s_obuf, 1, (size_t)cnt, s_of);

      off = (int)cnt;
      len = (unsigned int)(ol - upto);

      (void)memmove((char *)s_obuf, (const char *)(s_obuf + off), len);

      s_obase = upto;
    }
}

/******************************************************************************/

static void
e_init_stream (FILE *f)
{
  s_of = f;
  s_obase = 0;
  ol = 0;
  tagpos = -1;
  tagcnt = 8;
}

/******************************************************************************/

static void
e_bit (int b)
{
  if (tagcnt == 8)
    {
      obuf_flush (ol);
      tagpos = ol;
      s_obuf[ol - s_obase] = 0;
      ol++;
      tagcnt = 0;
    }

  if (b)
    s_obuf[tagpos - s_obase] |= (unsigned char)(1 << (7 - tagcnt));

  tagcnt++;
}

/******************************************************************************/

static void
e_byte (int x)
{
  s_obuf[ol - s_obase] = (unsigned char)(x & 0xff);
  ol++;
}

# endif

/******************************************************************************/

static void
e_bits (unsigned v, int n)
{
  int i;

  for (i = n - 1; i >= 0; i--)
    e_bit ((int)((v >> i) & 1));
}

/******************************************************************************/

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
    e_bit (1);

  if (B < 7)
    e_bit (0);

  e_bits ((unsigned)(v & ((1 << B) - 1)), B);
}

/******************************************************************************/

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

/******************************************************************************/

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

/******************************************************************************/

static void
e_lit (int byte)
{
  e_bit (0);
  e_byte (byte);
}

/******************************************************************************/

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
      int b0 = ((L == 3) ? 0 : 1);

      e_bit (1);
      e_byte (0xC0 | ((off >> 7) & 0x3f));
      e_byte (((off & 0x7f) << 1) | b0);

      if (L >= 4)
        e_len3 (L);
    }
}

/******************************************************************************/

static int head[HSZ];

/******************************************************************************/

# ifndef POPCOM_STREAM
static int *lnk;
static const unsigned char *D;
static long N;

/******************************************************************************/

static int
hash3 (long i)
{
  return (int)((((unsigned)D[i] << 10) ^ ((unsigned)D[i + 1] << 5) ^ D[i + 2])
               & (HSZ - 1));
}

/******************************************************************************/

static void
hinsert (long i)
{
  int h;

  if (i + 2 >= N)
    return;

  h = hash3 (i);
  lnk[i] = head[h];
  head[h] = (int)i;
}
# endif

/******************************************************************************/

static int
mlen_min (int dist)
{
  return ((dist <= 128) ? 2 : 3);
}

/******************************************************************************/

# ifndef POPCOM_STREAM

static int
findmatch (long i, int *bestdist, int maxdepth)
{
  int h, bl = 0, bd = 0, depth = maxdepth;
  long p;

  if (i + 2 >= N)
    return 0;

  h = hash3 (i);

  for (p = head[h]; p >= 0 && depth-- > 0; p = lnk[p])
    {
      long d = i - p;
      int ml, mx;

      if (d > MAXDIST)
        break;

      mx = MAXLEN;

      if (mx > (int)(N - i))
        mx = (int)(N - i);

      if (bl > 0 && bl < mx && D[p + bl] != D[i + bl])
        continue;

      ml = 0;

      while (ml < mx && D[p + ml] == D[i + ml])
        ml++;

      if (ml < mlen_min ((int)d))
        continue;

      if (ml > bl || (ml == bl && d < bd))
        {
          bl = ml;
          bd = (int)d;

          if (bl >= mx)
            break;
        }
    }

  *bestdist = bd;

  return bl;
}

/******************************************************************************/

static long
compress (const unsigned char *data, long n, int start, unsigned char *out,
          int depth)
{
  long i;
  int d, d2, L2;

  D = data;
  N = n;
  lnk = (int *)lxmalloc (sizeof (int) * (size_t)(n > 0 ? n : 1));

  {
    int j;

    for (j = 0; j < HSZ; j++)
      head[j] = -1;
  }

  e_init (out);

  for (i = 0; i < start && i + 2 < n; i++)
    hinsert (i);

  i = start;

  while (i < n)
    {
      int L;

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
              hinsert (i);
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

# endif

/******************************************************************************/

# ifndef POPCOM_NO_OPT
#  ifndef POPCOM_STREAM

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

/******************************************************************************/

static int
len_bits (int L, int c)
{
  int b2 = c + 2;

  if (L == b2)
    return 1;

  if (L == b2 + 1)
    return 2;

  if (L == b2 + 2)
    return 3;

  return 3 + extlen_bits (L - c - 1);
}

/******************************************************************************/

static int
len3_bits (int L)
{
  if (L == 3)
    return 0;

  if (L == 4)
    return 1;

  if (L == 5)
    return 2;

  return 2 + extlen_bits (L - 2);
}

/******************************************************************************/

static int
match_bits (int dist, int L)
{
  if (dist <= 128)
    return 1 + 8 + len_bits (L, 0);

  if (dist <= 1152)
    return 1 + 8 + 4 + len_bits (L, 1);

  return 1 + 16 + len3_bits (L);
}

/******************************************************************************/

static long
compress_opt (const unsigned char *data, long n, int start, unsigned char *out,
              int depth)
{
  long i;
  long *cost;
  int *tlen, *tdist;
  static int l2d[MAXLEN + 1];

  D = data;
  N = n;

  lnk = (int *)lxmalloc (sizeof (int) * (size_t)(n > 0 ? n : 1));
  cost = (long *)lxmalloc (sizeof (long) * (size_t)(n + 1));
  tlen = (int *)lxmalloc (sizeof (int) * (size_t)(n + 1));
  tdist = (int *)lxmalloc (sizeof (int) * (size_t)(n + 1));

  {
    long j;

    for (j = 0; j < HSZ; j++)
      head[j] = -1;
  }

  for (i = 0; i < start && i + 2 < n; i++)
    hinsert (i);

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
                cap = (int)(n - i);

              for (p = head[h]; p >= 0 && dep-- > 0; p = lnk[p])
                {
                  long d = i - p;
                  int ml;

                  if (d > MAXDIST)
                    break;

                  if (maxml > 0 && maxml < cap && D[p + maxml] != D[i + maxml])
                    continue;

                  ml = 0;

                  while (ml < cap && D[p + ml] == D[i + ml])
                    ml++;

                  if (ml > maxml)
                    {
                      int L;

                      for (L = maxml + 1; L <= ml; L++)
                        l2d[L] = (int)d;

                      maxml = ml;

                      if (maxml >= cap)
                        break;
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
              long lo = (i > 128) ? (i - 128) : 0;
              long p;

              for (p = i - 1; p >= lo; p--)
                if (data[p] == data[i] && data[p + 1] == data[i + 1])
                  {
                    long c2 = cost[i] + match_bits ((int)(i - p), 2);

                    if (c2 < cost[i + 2])
                      {
                        cost[i + 2] = c2;
                        tlen[i + 2] = 2;
                        tdist[i + 2] = (int)(i - p);
                      }

                    break;
                  }
            }
        }

      hinsert (i);
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
          e_match (tdist[e], L);
        else
          e_lit (data[e - 1]);
      }

    free (st);
  }

  free (lnk);
  free (cost);
  free (tlen);
  free (tdist);

  return ol;
}

#  endif
# endif
#endif

/******************************************************************************/

#ifndef POPCOM_COMPRESS_ONLY

static const unsigned char *ip, *ip_end;
static int dbc;
static unsigned dbv;
static int
g_bit (void)
{
  int b;

  if (dbc == 0)
    {
      if (ip >= ip_end)
        {
          (void)fprintf (stderr, "FATAL: unexpected end of data\n");

          exit (1);
        }

      dbv = *ip++;
      dbc = 8;
    }

  b = (dbv >> 7) & 1;
  dbv = (dbv << 1) & 0xff;
  dbc--;

  return b;
}


/******************************************************************************/

static long
decode (const unsigned char *pl, long pllen, unsigned char *out, long outlen,
        int litcnt, const unsigned char *litsrc)
{
  long pos = litcnt;
  long mpos;
  int ctrl, a, b, c, bit, i;
  unsigned off, ml;

  (void)memcpy (out, litsrc, (size_t)litcnt); /* seed the literal prefix */

  ip = pl;
  ip_end = pl + (size_t)pllen;
  dbc = 0;

  while (pos < outlen)
    {
      ctrl = g_bit ();

      if (ip >= ip_end)
        {
          (void)fprintf (stderr, "FATAL: unexpected end of data\n");

          exit (1);
        }

      a = *ip++;

      if (!ctrl)
        {
          out[pos++] = (unsigned char)a;

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

          if (ip >= ip_end)
            {
              (void)fprintf (stderr, "FATAL: unexpected end of data\n");

              exit (1);
            }

          a = *ip++;
          nc = a & 1;
          a = ((cy << 7) | (a >> 1)) & 0xff;
          cy = nc;
          ol2 = a;
          off = ((unsigned)oh << 8) | (unsigned)ol2;
          a = 2;

          if (!cy)
            {
              ml = (unsigned int)(a + 1);

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
          ml = (unsigned int)(a + 1);

          goto cp;
        }

    lc:
      a++;
      bit = g_bit ();

      if (!bit)
        {
          ml = (unsigned int)(a + 1);

          goto cp;
        }

      a++;
      bit = g_bit ();

      if (!bit)
        {
          ml = (unsigned int)(a + 1);

          goto cp;
        }

      a = 2;
      for (;;)
        {
          bit = g_bit ();

          if (!bit)
            break;

          a++;

          if (a == 7)
            break;
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
      ml = (unsigned)a + 1;

    cp:
      if (off >= (unsigned)pos)
        {
          (void)fprintf (stderr,
                         "FATAL: invalid compressed data (underflow)\n");

          exit (1);
        }

      mpos = pos - ((long)off + 1);

      for (i = 0; i < (int)ml; i++)
        if (pos < outlen)
          out[pos++] = out[mpos++];
    }

  return pos;
}

#endif

/******************************************************************************/

#ifndef POPCOM_DECODE_ONLY
# ifndef POPCOM_STREAM
static long
min_gap (const unsigned char *pl, long pl_len, long outlen, int litcnt,
         long pl_dst_top)
{
  long src_base = pl_dst_top + 1 - pl_len;
  long dst_base = (long)TPA + litcnt;
  long pi = 0;
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
      consumed = pi;
      gap = (src_base + consumed) - (dst_base + produced);

      if (first || gap < ming)
        {
          ming = gap;
          first = 0;
        }

      if (bc == 0)
        {
          bv = pl[pi++];
          bc = 8;
        }

      ctrl = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;
      a = pl[pi++];

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
                  bv = pl[pi++];
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

          /* original 8080 stub masks a & 0x3f here; length comes from b0 below */
          b0 = (pl[pi++]) & 1;
          a = 2;

          if (!b0)
            {
              ml = (unsigned int)(a + 1);

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
          bv = pl[pi++];
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned int)(a + 1);

          goto cpx;
        }

    lc:
      a++;

      if (bc == 0)
        {
          bv = pl[pi++];
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned int)(a + 1);

          goto cpx;
        }

      a++;

      if (bc == 0)
        {
          bv = pl[pi++];
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned int)(a + 1);

          goto cpx;
        }

      a = 2;

      for (;;)
        {
          if (bc == 0)
            {
              bv = pl[pi++];
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;

          if (!bit)
            break;

          a++;

          if (a == 7)
            break;
        }

      b = a;
      a = 1;

      do
        {
          if (bc == 0)
            {
              bv = pl[pi++];
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;
          a = ((a << 1) | bit) & 0xff;
        }
      while (--b);

      a = (a + c) & 0xff;
      ml = (unsigned int)(a + 1);

    cpx:
      for (k = 0; k < (long)ml; k++)
        produced++;
    }

  (void)bit;

  return ming;
}
# endif
#endif

/******************************************************************************/

#ifdef LZ_CPM
# ifdef __Z88DK
#  include <cpm.h>
# else
extern int bdos ();
# endif
#endif

/******************************************************************************/

#ifdef LZ_CPM

static void
cpm_setfcb (unsigned char *fcb, const char *fn)
{
  const char *p = fn;
  int i;

  for (i = 0; i < 36; i++)
    fcb[i] = 0;

  for (i = 1; i <= 11; i++)
    fcb[i] = ' ';

  if (p[0] && p[1] == ':')
    p += 2;

  i = 1;

  while (*p && *p != '.' && i <= 8)
    {
      char c = *p++;

      if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);

      fcb[i++] = (unsigned char)c;
    }

  while (*p && *p != '.')
    p++;

  if (*p == '.')
    p++;

  i = 9;

  while (*p && i <= 11)
    {
      char c = *p++;

      if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);

      fcb[i++] = (unsigned char)c;
    }
}

/******************************************************************************/

static long
cpm_file_size (const char *fn)
{
  unsigned char fcb[36];
  long records, last_ext;
  int lrbc;

  if ((bdos (12, 0) & 0x00ff) < 0x30)
    return -1;

  cpm_setfcb (fcb, fn);
  (void)bdos (35, (int)fcb);
  records = ((long)fcb[33])
          | ((long)fcb[34] << 8)
          | ((long)fcb[35] << 16);

  if (records <= 0)
    return -1;

  last_ext = (records - 1L) / 128L;

  cpm_setfcb (fcb, fn);
  fcb[12] = (unsigned char)(last_ext & 0x1f);
  fcb[14] = (unsigned char)((last_ext >> 5) & 0x3f);

  if ((bdos (15, (int)fcb) & 0x00ff) == 0xff)
    return -1;

  lrbc = fcb[13] & 0xff;
  (void)bdos (16, (int)fcb);

  if (lrbc > 0 && lrbc <= 128)
    return (records - 1L) * 128L + lrbc;

  return records * 128L;
}

/******************************************************************************/

static int
cpm_set_byte_count (const char *fn, long nbytes)
{
  unsigned char fcb[36];
  long records, last_ext;
  int lrbc;

  if (nbytes <= 0)
    return -1;

  if ((bdos (12, 0) & 0x00ff) < 0x30)
    return -1;

  records = (nbytes + 127L) / 128L;
  last_ext = (records - 1L) / 128L;
  lrbc = (int)(nbytes - (records - 1L) * 128L);

  cpm_setfcb (fcb, fn);
  fcb[12] = (unsigned char)(last_ext & 0x1f);
  fcb[14] = (unsigned char)((last_ext >> 5) & 0x3f);

  if ((bdos (15, (int)fcb) & 0x00ff) == 0xff)
    return -1;

  fcb[13] = (unsigned char)(lrbc & 0x7f);
  (void)bdos (16, (int)fcb);

  return 0;
}

#endif

/******************************************************************************/

#ifndef POPCOM_STREAM
static long
readfile (const char *fn, unsigned char *buf, size_t max)
{
  FILE *f = fopen (fn, "rb");
  size_t n;

  if (!f)
    f = fopen (fn, "r");

  if (!f)
    return -1;

  n = fread (buf, 1, max, f);

  if (n == max)
    {
      unsigned char c;

      if (fread (&c, 1, 1, f) > 0)
        {
          (void)fclose (f);

          return (long)max + 1;
        }
    }

  (void)fclose (f);

# ifdef LZ_CPM
  {
    long exact = cpm_file_size (fn);

    if (exact > 0 && exact <= (long)n && (long)n - exact < 128)
      return exact;
  }
# endif

  return (long)n;
}
#endif

/******************************************************************************/

static int
writefile (const char *fn, const unsigned char *buf, long n)
{
  FILE *f = fopen (fn, "wb");

  if (!f)
    f = fopen (fn, "w");

  if (!f)
    return -1;

  (void)fwrite (buf, 1, (size_t)n, f);
  (void)fclose (f);

#ifdef LZ_CPM
  (void)cpm_set_byte_count (fn, n);
#endif

  return 0;
}

/******************************************************************************/

#ifndef POPCOM_DECODE_ONLY
static void
put16 (unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)(v & 0xff);
  p[1] = (unsigned char)((v >> 8) & 0xff);
}
#endif

/******************************************************************************/

static unsigned
get16 (const unsigned char *p)
{
  return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

/******************************************************************************/

static void
mkname (const char *in, const char *ext, char *out, size_t outsz)
{
  const char *dot = strrchr (in, '.');
  const char *slash = strrchr (in, '/');
  size_t base;

  if (dot && (!slash || dot > slash))
    base = strlen (in) - strlen (dot); /* offset of dot within in */
  else
    base = strlen (in);

  if (base >= outsz)
    base = outsz - 1;

  (void)memcpy (out, in, base);
  out[base] = '\0';
  (void)strncat (out, ext, outsz - base - 1);
}

/******************************************************************************/

#ifndef POPCOM_DECODE_ONLY
# ifndef POPCOM_STREAM

static void
put_header (unsigned char *outf, const unsigned char *data, long stub_v,
            long outlen)
{
  (void)memcpy (outf, data, LITCNT);
  outf[0] = 0xc3;
  put16 (outf + 1, (unsigned)stub_v);
  (void)memcpy (outf + 5, "-pc1-", 5);
  put16 (outf + 10, (unsigned)outlen);
  outf[12] = outf[13] = outf[14] = outf[15] = 0;
}

/******************************************************************************/

static long
build_z80 (unsigned char *outf, const unsigned char *data, long pllen,
           const unsigned char *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long stub_dst_top = pl_dst_top + 246;
  unsigned char *stub;

  if (stub_dst_top > MEMTOP)
    return -1;

  put_header (outf, data, stub_v, outlen);

  (void)memcpy (outf + LITCNT, pl, (size_t)pllen);
  (void)memcpy (outf + LITCNT + pllen, data, LITCNT);

  stub = outf + LITCNT + pllen + LITCNT;

  (void)memcpy (stub, z80_stub, STUBLEN);

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

/******************************************************************************/

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
    return -1;

  put_header (outf, data, stub_v, outlen);

  (void)memcpy (outf + LITCNT, pl, (size_t)pllen);
  (void)memcpy (outf + LITCNT + pllen, data, LITCNT);

  su = outf + LITCNT + pllen + LITCNT;
  de = su + S8_SLEN;

  (void)memcpy (su, setup8080, S8_SLEN);
  (void)memcpy (de, decomp8080, S8_DLEN);

  for (i = 0; i < SETUP8080_FIX_N; i++)
    put16 (su + setup8080_fix[i][0],
           (unsigned)(stub_v + setup8080_fix[i][1]));

  put16 (su + S8S_LIT_SRC, (unsigned)lit_src);
  put16 (su + S8S_DCMP_SRCTOP, (unsigned)(decomp_file_v + S8_DLEN - 1));
  put16 (su + S8S_DCMP_DSTTOP, (unsigned)dcmp_dsttop);
  put16 (su + S8S_DCMP_LEN, (unsigned)S8_DLEN);
  put16 (su + S8S_DCMP_RUN, (unsigned)stub_run);

  for (i = 0; i < DECOMP8080_FIX_N; i++)
    put16 (de + decomp8080_fix[i][0],
           (unsigned)(stub_run + decomp8080_fix[i][1]));

  de[S8D_OUT_END_HI] = (unsigned char)((out_end >> 8) & 0xff);
  de[S8D_OUT_END_LO] = (unsigned char)(out_end & 0xff);

  put16 (de + S8D_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (de + S8D_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (de + S8D_PL_LEN, (unsigned)pllen);
  put16 (de + S8D_PL_DSTBOT, (unsigned)pl_dstbot);

  return LITCNT + pllen + LITCNT + S8_SLEN + S8_DLEN;
}

/******************************************************************************/

static int parse_header (const unsigned char *data, long n, unsigned *stubv,
                         unsigned *lit_src, long *outlen);

/******************************************************************************/

static int
do_compress (const char *fn, const char *oname, int verbose, int use8080,
             int optimal)
{
  unsigned char *data = g_a, *pl = g_b, *outf = g_c;
  long n, pllen, outlen, pl_dst_top, ming, total, body;
  char nb[1024];

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

#  ifndef POPCOM_COMPRESS_ONLY
  {
    unsigned r_stubv, r_litsrc;
    long r_outlen;

    if (parse_header (data, n, &r_stubv, &r_litsrc, &r_outlen) == 0
        && r_outlen >= LITCNT && r_outlen <= MZXFILE
        && (long)r_litsrc - TPA >= LITCNT
        && (long)r_litsrc - TPA + LITCNT <= n)
      {
        long lit_off = (long)r_litsrc - TPA; /* in [LITCNT, n - LITCNT] */

        (void)decode (data + LITCNT, lit_off - LITCNT, g_c, r_outlen, LITCNT,
                      data + lit_off);
        (void)memcpy (data, g_c, (size_t)r_outlen);
        n = r_outlen;

        if (verbose)
          (void)fprintf (stderr,
                         "  %-12s already packed; recompressing original\n",
                         fn);
      }
  }
#  endif

  if (n > MZXFILE)
    {
      (void)fprintf (stderr,
                     "FATAL: %s exceeds MZXFILE=%ld (build constraint)\n",
                     fn, (long)MZXFILE);

      return 1;
    }

  if (n > 65535L)
    {
      (void)fprintf (stderr,
                     "FATAL: %s is too large for header (max 65535 bytes)\n",
                     fn);

      return 1;
    }

  if (n <= LITCNT + 32)
    {
      (void)fprintf (stderr, "FATAL: %s too small\n", fn);

      return 1;
    }

#  ifdef POPCOM_NO_OPT
  if (optimal && verbose)
    (void)fprintf (stderr, "  (note: -e is not available in this build)\n");

  pllen = compress (data, n, LITCNT, pl, 1024);
#  else
  pllen = (optimal ? compress_opt (data, n, LITCNT, pl, 4096)
                   : compress (data, n, LITCNT, pl, 1024));
#  endif
  outlen = n;
  pl_dst_top = (long)(TPA + outlen) - 1;
  ming = min_gap (pl, pllen, outlen - LITCNT, LITCNT, pl_dst_top);

  if (ming < 1)
    pl_dst_top += (1 - ming);

  body = (use8080 ? build_8080 (outf, data, pllen, pl, outlen, pl_dst_top)
                  : build_z80 (outf, data, pllen, pl, outlen, pl_dst_top));

  if (body < 0)
    {
      (void)fprintf (stderr, "FATAL: %s would not fit in memory\n", fn);

      return 1;
    }

  total = body;

  if (total >= n)
    {
      if (verbose)
        (void)fprintf (stderr,
                       "  %-12s -- inefficient (%ld => %ld), skipped\n",
                       fn, n, total);

      return 2;
    }

  if (!oname)
    {
      mkname (fn, ".pop", nb, sizeof (nb));
      oname = nb;
    }

  if (writefile (oname, outf, total))
    {
      (void)fprintf (stderr, "FATAL: cannot write %s\n", oname);

      return 1;
    }

  if (verbose)
    {
      long p10 = (total * 1000L + n / 2) / n; /* n > LITCNT + 32 here */

      (void)fprintf (stderr,
                     "  %-12s %6ld => %6ld  (%ld.%ld%%)  [%s]  -> %s\n",
                     fn, n, total, p10 / 10, p10 % 10,
                     (use8080 ? "8080" : "Z80"), oname);
    }

  return 0;
}

/******************************************************************************/

# else

/*
 * Streaming compressor for tiny (CP/M-80) hosts.  The input is read from disk
 * through a fixed sliding window and the payload is written to a temp file, so
 * working memory does not depend on file size and large executables can be
 * packed on a 64K machine.  Match choices may differ from the in-RAM path, but
 * the emitted format is identical and self-extracts the same way.
 *
 * The sliding window (window bytes + a 2-byte link per slot, = 3*WINSZ of RAM)
 * is allocated dynamically: at startup the largest power-of-two window that
 * fits in the available heap is chosen, from WIN_MAX down to WINMIN, so the
 * compressor uses as big a window -- and packs as tightly -- as the host's TPA
 * allows.  The effective match distance is min(WINSZ - MAXLEN - 1, MAXDIST).
 *
 * WIN_MAX is not a tunable: a window larger than 2*MAXDIST cannot help, since
 * the format caps match distance at MAXDIST, so that is where the probe starts.
 */

#  define LOOKAHEAD MAXLEN
#  define WIN_MAX (MAXDIST * 2)

#  ifndef WINMIN
#   define WINMIN 1024
#  endif

#  ifndef LZTMP
#   define LZTMP "LZTMP.$$$"
#  endif

/******************************************************************************/

static int parse_header (const unsigned char *data, long n, unsigned *stubv,
                         unsigned *lit_src, long *outlen);

/******************************************************************************/

static unsigned char *s_win;
static int *s_lnk;
static long s_winsz, s_wmask, s_maxback;
static FILE *s_in;
static long s_N, s_loaded;

/******************************************************************************/

static int
win_alloc (void)
{
  for (s_winsz = WIN_MAX; s_winsz >= WINMIN; s_winsz >>= 1)
    {
      s_win = (unsigned char *)malloc ((size_t)s_winsz);
      s_lnk = (int *)malloc ((size_t)s_winsz * sizeof (int));

      if (s_win && s_lnk)
        break;

      FREE (s_win);
      FREE (s_lnk);
      s_win = 0;
      s_lnk = 0;
    }

  if (!s_win)
    return -1;

  s_wmask = s_winsz - 1;
  s_maxback = s_winsz - LOOKAHEAD - 1;

  if (s_maxback > MAXDIST)
    s_maxback = MAXDIST;

  return 0;
}

/******************************************************************************/

static void
win_free (void)
{
  FREE (s_win);
  FREE (s_lnk);
}

/******************************************************************************/

static void
win_load (long upto)
{
  if (upto > s_N)
    upto = s_N;

  while (s_loaded < upto)
    {
      int c = getc (s_in);

      if (c == EOF)
        {
          s_N = s_loaded;

          break;
        }

      s_win[s_loaded & s_wmask] = (unsigned char)c;
      s_loaded++;
    }
}

/******************************************************************************/

static int
s_hash3 (long i)
{
  return (int)((((unsigned)s_win[i & s_wmask] << 10)
                ^ ((unsigned)s_win[(i + 1) & s_wmask] << 5)
                ^ s_win[(i + 2) & s_wmask]) & (HSZ - 1));
}

/******************************************************************************/

static void
s_hinsert (long i)
{
  int h;

  if (i + 2 >= s_N)
    return;

  h = s_hash3 (i);
  s_lnk[i & s_wmask] = head[h];
  head[h] = (int)(i & s_wmask);
}

/******************************************************************************/

/*
 * Hash chains store positions as window-relative indices (0..s_winsz-1);
 * the absolute position is reconstructed from the current position i,
 * which is always within s_winsz of any live chain entry.  Entries older
 * than s_maxback have been overwritten in the window and are pruned by the
 * distance test.
 */

static int
findmatch_stream (long i, int *bestdist, int maxdepth)
{
  int bl = 0, bd = 0, stored, ml, depth = maxdepth;
  long base;

  if (i + 2 >= s_N)
    return 0;

  base = i & ~s_wmask;
  stored = head[s_hash3 (i)];

  while (stored >= 0 && depth-- > 0)
    {
      long p = base | (long)stored;
      long d;
      int mx;

      if (p > i)
        p -= s_winsz;

      d = i - p;

      if (d <= 0 || d > s_maxback)
        break;

      mx = MAXLEN;

      if ((long)mx > s_N - i)
        mx = (int)(s_N - i);

      if (bl > 0 && bl < mx
          && s_win[(p + bl) & s_wmask] != s_win[(i + bl) & s_wmask])
        {
          stored = s_lnk[p & s_wmask];

          continue;
        }

      ml = 0;

      while (ml < mx && s_win[(p + ml) & s_wmask] == s_win[(i + ml) & s_wmask])
        ml++;

      if (ml >= mlen_min ((int)d) && (ml > bl || (ml == bl && d < bd)))
        {
          bl = ml;
          bd = (int)d;

          if (bl >= mx)
            break;
        }

      stored = s_lnk[p & s_wmask];
    }

  *bestdist = bd;

  return bl;
}

/******************************************************************************/

static long
compress_stream (FILE *in, long n, int start, FILE *out, int depth,
                 unsigned char *first16)
{
  long i;
  int k;

  s_in = in;
  s_N = n;
  s_loaded = 0;

  for (k = 0; k < HSZ; k++)
    head[k] = -1;

  e_init_stream (out);

  win_load ((long)LOOKAHEAD + 1);

  for (k = 0; k < LITCNT && (long)k < n; k++)
    first16[k] = s_win[k & s_wmask];

  for (i = 0; i < start && i + 2 < n; i++)
    {
      win_load (i + LOOKAHEAD + 1);
      s_hinsert (i);
    }

  i = start;

  while (i < n)
    {
      int d, L;

      win_load (i + LOOKAHEAD + 1);
      d = 0;
      L = findmatch_stream (i, &d, depth);

      if (L >= mlen_min (d))
        {
          int d2 = 0, L2 = 0;

          if (i + 1 < n)
            {
              s_hinsert (i);
              win_load (i + 1 + LOOKAHEAD + 1);
              L2 = findmatch_stream (i + 1, &d2, depth);
            }

          if (L2 > L)
            {
              e_lit (s_win[i & s_wmask]);
              i++;

              continue;
            }

          e_match (d, L);
          {
            long e = i + L;

            i++;

            for (; i < e; i++)
              {
                win_load (i + LOOKAHEAD + 1);
                s_hinsert (i);
              }
          }
        }
      else
        {
          e_lit (s_win[i & s_wmask]);
          s_hinsert (i);
          i++;
        }
    }

  obuf_flush (ol);

  return ol;
}

/******************************************************************************/

static FILE *s_mg_f;
static long s_mg_rd;

static int
mg_byte (void)
{
  s_mg_rd++;

  return getc (s_mg_f);
}

/******************************************************************************/

/*
 * Worst-case overlap scan, identical to min_gap () but reading the payload
 * sequentially from the temp file instead of a RAM buffer.
 */

static long
min_gap_stream (FILE *f, long pl_len, long outlen, int litcnt, long pl_dst_top)
{
  long src_base = pl_dst_top + 1 - pl_len;
  long dst_base = TPA + litcnt;
  int bc = 0;
  unsigned bv = 0;
  long produced = 0;
  long consumed;
  long gap, ming = 0x7fffffffL;
  int first = 1;
  int ctrl, a, b, c, bit;
  unsigned ml;
  long k;

  s_mg_f = f;
  s_mg_rd = 0;

  while (produced < outlen)
    {
      consumed = s_mg_rd;
      gap = (src_base + consumed) - (dst_base + produced);

      if (first || gap < ming)
        {
          ming = gap;
          first = 0;
        }

      if (bc == 0)
        {
          bv = (unsigned)mg_byte ();
          bc = 8;
        }

      ctrl = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;
      a = mg_byte ();

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
                  bv = (unsigned)mg_byte ();
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

          b0 = mg_byte () & 1;
          a = 2;

          if (!b0)
            {
              ml = (unsigned)a + 1U;

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
          bv = (unsigned)mg_byte ();
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned)a + 1U;

          goto cpx;
        }

    lc:
      a++;

      if (bc == 0)
        {
          bv = (unsigned)mg_byte ();
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned)a + 1U;

          goto cpx;
        }

      a++;

      if (bc == 0)
        {
          bv = (unsigned)mg_byte ();
          bc = 8;
        }

      bit = (bv >> 7) & 1;
      bv = (bv << 1) & 0xff;
      bc--;

      if (!bit)
        {
          ml = (unsigned)a + 1U;

          goto cpx;
        }

      a = 2;

      for (;;)
        {
          if (bc == 0)
            {
              bv = (unsigned)mg_byte ();
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;

          if (!bit)
            break;

          a++;

          if (a == 7)
            break;
        }

      b = a;
      a = 1;

      do
        {
          if (bc == 0)
            {
              bv = (unsigned)mg_byte ();
              bc = 8;
            }

          bit = (bv >> 7) & 1;
          bv = (bv << 1) & 0xff;
          bc--;
          a = ((a << 1) | bit) & 0xff;
        }
      while (--b);

      a = (a + c) & 0xff;
      ml = (unsigned)a + 1U;

    cpx:
      for (k = 0; k < (long)ml; k++)
        produced++;
    }

  (void)bit;

  return ming;
}

/******************************************************************************/

static long
assemble_z80_stream (FILE *outf, const unsigned char *first16, long pllen,
                     FILE *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long stub_dst_top = pl_dst_top + 246;
  unsigned char hdr[LITCNT], stub[STUBLEN];
  long k;

  (void)memcpy (hdr, first16, LITCNT);
  hdr[0] = 0xc3;
  put16 (hdr + 1, (unsigned)stub_v);
  (void)memcpy (hdr + 5, "-pc1-", 5);
  put16 (hdr + 10, (unsigned)outlen);
  hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0;
  (void)fwrite (hdr, 1, LITCNT, outf);

  for (k = 0; k < pllen; k++)
    {
      int c = getc (pl);

      if (c == EOF)
        break;

      (void)putc (c, outf);
    }

  (void)fwrite (first16, 1, LITCNT, outf);

  (void)memcpy (stub, z80_stub, STUBLEN);
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
  (void)fwrite (stub, 1, STUBLEN, outf);

  return LITCNT + pllen + LITCNT + STUBLEN;
}

/******************************************************************************/

static long
assemble_8080_stream (FILE *outf, const unsigned char *first16, long pllen,
                      FILE *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long decomp_file_v = stub_v + S8_SLEN;
  long stub_run = pl_dst_top + 51;
  long dcmp_dsttop = stub_run + S8_DLEN - 1;
  long pl_dstbot = pl_dst_top + 1 - pllen;
  unsigned char hdr[LITCNT], su[S8_SLEN], de[S8_DLEN];
  long k;
  int i;

  (void)memcpy (hdr, first16, LITCNT);
  hdr[0] = 0xc3;
  put16 (hdr + 1, (unsigned)stub_v);
  (void)memcpy (hdr + 5, "-pc1-", 5);
  put16 (hdr + 10, (unsigned)outlen);
  hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0;
  (void)fwrite (hdr, 1, LITCNT, outf);

  for (k = 0; k < pllen; k++)
    {
      int c = getc (pl);

      if (c == EOF)
        break;

      (void)putc (c, outf);
    }

  (void)fwrite (first16, 1, LITCNT, outf);

  (void)memcpy (su, setup8080, S8_SLEN);
  (void)memcpy (de, decomp8080, S8_DLEN);

  for (i = 0; i < SETUP8080_FIX_N; i++)
    put16 (su + setup8080_fix[i][0], (unsigned)(stub_v + setup8080_fix[i][1]));

  put16 (su + S8S_LIT_SRC, (unsigned)lit_src);
  put16 (su + S8S_DCMP_SRCTOP, (unsigned)(decomp_file_v + S8_DLEN - 1));
  put16 (su + S8S_DCMP_DSTTOP, (unsigned)dcmp_dsttop);
  put16 (su + S8S_DCMP_LEN, (unsigned)S8_DLEN);
  put16 (su + S8S_DCMP_RUN, (unsigned)stub_run);

  for (i = 0; i < DECOMP8080_FIX_N; i++)
    put16 (de + decomp8080_fix[i][0],
           (unsigned)(stub_run + decomp8080_fix[i][1]));

  de[S8D_OUT_END_HI] = (unsigned char)((out_end >> 8) & 0xff);
  de[S8D_OUT_END_LO] = (unsigned char)(out_end & 0xff);
  put16 (de + S8D_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (de + S8D_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (de + S8D_PL_LEN, (unsigned)pllen);
  put16 (de + S8D_PL_DSTBOT, (unsigned)pl_dstbot);

  (void)fwrite (su, 1, S8_SLEN, outf);
  (void)fwrite (de, 1, S8_DLEN, outf);

  return LITCNT + pllen + LITCNT + S8_SLEN + S8_DLEN;
}

/******************************************************************************/

static long
count_file (const char *fn)
{
  FILE *f = fopen (fn, "rb");
  long n = 0;
  unsigned char buf[128];

  if (!f)
    f = fopen (fn, "r");

  if (!f)
    return -1;


  for (;;)
    {
      size_t r = fread (buf, 1, sizeof (buf), f);

      n += (long)r;

      if (r < sizeof (buf))
        break;
    }

  if (ferror (f))
    {
      (void)fclose (f);

      return -1;
    }

  (void)fclose (f);

#  ifdef LZ_CPM
  {
    long exact = cpm_file_size (fn);

    if (exact > 0 && exact <= n && n - exact < 128)
      n = exact;
  }
#  endif

  return n;
}

/******************************************************************************/

static int
do_compress_stream (const char *fn, const char *oname, int verbose,
                    int use8080, int optimal)
{
  FILE *in, *tmp, *outf;
  long n, pllen, outlen, pl_dst_top, ming, total, body;
  long stub_dst_top, dcmp_dsttop;
  unsigned char first16[LITCNT];
  char nb[64];

  n = count_file (fn);

  if (n < 0)
    {
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  if (n >= 16)
    {
      unsigned rsv, rls;
      long rol;

      in = fopen (fn, "rb");

      if (!in)
        in = fopen (fn, "r");

      if (in)
        {
          unsigned char hdr[LITCNT];
          long k;

          k = (long)fread (hdr, 1, LITCNT, in);
          (void)fclose (in);

          if (k == LITCNT && parse_header (hdr, n, &rsv, &rls, &rol) == 0)
            {
              (void)fprintf (stderr,
                             "FATAL: %s is already packed; restore it first\n",
                             fn);

              return 1;
            }
        }
    }

  if (n > MZXFILE)
    {
      (void)fprintf (stderr,
                     "FATAL: %s exceeds MZXFILE=%ld (build constraint)\n",
                     fn, (long)MZXFILE);

      return 1;
    }

  if (n > 65535L)
    {
      (void)fprintf (stderr,
                     "FATAL: %s is too large for header (max 65535 bytes)\n",
                     fn);

      return 1;
    }

  if (n <= LITCNT + 32)
    {
      (void)fprintf (stderr, "FATAL: %s too small\n", fn);

      return 1;
    }

  if (optimal && verbose)
    (void)fprintf (stderr, "  (note: -e is not available in this build)\n");

  in = fopen (fn, "rb");

  if (!in)
    in = fopen (fn, "r");

  if (!in)
    {
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  tmp = fopen (LZTMP, "wb");

  if (!tmp) {
    /* cppcheck-suppress incompatibleFileOpen */
    tmp = fopen (LZTMP, "w");
  }

  if (!tmp)
    {
      (void)fclose (in);
      (void)fprintf (stderr, "FATAL: cannot create temp file %s\n", LZTMP);

      return 1;
    }

  /* Grab the largest window the heap allows, after the file buffers exist. */

  if (win_alloc ())
    {
      (void)fclose (in);
      (void)fclose (tmp);
      (void)remove (LZTMP);
      (void)fprintf (stderr, "FATAL: out of memory for compression window\n");

      return 1;
    }

  if (verbose)
    (void)fprintf (stderr, "  %-12s window %ld bytes (max distance %ld)\n",
                   fn, s_winsz, s_maxback);

  pllen = compress_stream (in, n, LITCNT, tmp, 1024, first16);
  win_free (); /* release the window before reopening files */
  (void)fclose (in);
  (void)fclose (tmp);

  outlen = n;
  pl_dst_top = (long)(TPA + outlen) - 1;

  /* CP/M stdio cannot reliably read a file back through "w+b"; reopen "rb". */

  tmp = fopen (LZTMP, "rb");

  if (!tmp)
    tmp = fopen (LZTMP, "r");

  if (!tmp)
    {
      (void)remove (LZTMP);
      (void)fprintf (stderr, "FATAL: cannot reopen temp file %s\n", LZTMP);

      return 1;
    }

  ming = min_gap_stream (tmp, pllen, outlen - LITCNT, LITCNT, pl_dst_top);
  (void)fclose (tmp);

  if (ming < 1)
    pl_dst_top += (1 - ming);

  stub_dst_top = pl_dst_top + 246;
  dcmp_dsttop = (pl_dst_top + 51) + S8_DLEN - 1;

  if (use8080 ? (dcmp_dsttop > MEMTOP) : (stub_dst_top > MEMTOP))
    {
      (void)remove (LZTMP);
      (void)fprintf (stderr, "FATAL: %s would not fit in memory\n", fn);

      return 1;
    }

  body = (use8080 ? (LITCNT + pllen + LITCNT + S8_SLEN + S8_DLEN)
                  : (LITCNT + pllen + LITCNT + STUBLEN));

  total = body;

  if (total >= n)
    {
      if (verbose)
        (void)fprintf (stderr,
                       "  %-12s -- inefficient (%ld => %ld), skipped\n",
                       fn, n, total);

      (void)remove (LZTMP);

      return 2;
    }

  if (!oname)
    {
      mkname (fn, ".pop", nb, sizeof (nb));
      oname = nb;
    }

  outf = fopen (oname, "wb");

  if (!outf)
    outf = fopen (oname, "w");

  if (!outf)
    {
      (void)remove (LZTMP);
      (void)fprintf (stderr, "FATAL: cannot write %s\n", oname);

      return 1;
    }

  tmp = fopen (LZTMP, "rb");

  if (!tmp)
    tmp = fopen (LZTMP, "r");

  if (!tmp)
    {
      (void)fclose (outf);
      (void)remove (LZTMP);
      (void)fprintf (stderr, "FATAL: cannot reopen temp file %s\n", LZTMP);

      return 1;
    }

  if (use8080)
    (void)assemble_8080_stream (outf, first16, pllen, tmp, outlen, pl_dst_top);
  else
    (void)assemble_z80_stream (outf, first16, pllen, tmp, outlen, pl_dst_top);

  (void)fclose (outf);
  (void)fclose (tmp);
  (void)remove (LZTMP);

#  ifdef LZ_CPM
  (void)cpm_set_byte_count (oname, total);
#  endif

  if (verbose)
    {
      long p10 = (total * 1000L + n / 2) / n; /* n > LITCNT + 32 here */

      (void)fprintf (stderr,
                     "  %-12s %6ld => %6ld  (%ld.%ld%%)  [%s]  -> %s\n", fn, n,
                     total, p10 / 10, p10 % 10, (use8080 ? "8080" : "Z80"),
                     oname);
    }

  return 0;
}

# endif
#endif

/******************************************************************************/

static int
parse_header (const unsigned char *data, long n, unsigned *stubv,
              unsigned *lit_src, long *outlen)
{
  unsigned sv;

  if (n < 16)
    return 1;

  if (data[0] == 0xc3)
    sv = get16 (data + 1);
  else if (data[0] == 0x18 && data[2] == 0xc3)
    sv = get16 (data + 3);
  else
    return 1;

  if (memcmp (data + 5, "-pc1-", 5) != 0)
    return 1;

  *stubv = sv;
  *lit_src = sv - LITCNT;
  *outlen = (long)get16 (data + 10);

  if (sv < 0x120 || *outlen <= 0 || (long)(sv - TPA) > n)
    return 1;

  return 0;
}

/******************************************************************************/

#ifndef POPCOM_COMPRESS_ONLY
# ifndef POPCOM_STREAM
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
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  if (n > BUFSZ)
    {
      (void)fprintf (stderr,
                     "FATAL: %s is too large to restore in this build\n", fn);

      return 1;
    }

  if (parse_header (data, n, &stubv, &lit_src, &outlen))
    {
      (void)fprintf (stderr, "FATAL: %s is not a POPCOM/LZPACK file\n", fn);

      return 1;
    }

  pstart = TPA + LITCNT - TPA;

  if (outlen > MZXFILE)
    {
      (void)fprintf (stderr, "FATAL: %s expands beyond MZXFILE=%ld\n", fn,
               (long)MZXFILE);

      return 1;
    }

  if ((long)lit_src - TPA < 0 ||
      (long)lit_src - TPA + LITCNT > n || outlen < LITCNT)
    {
      (void)fprintf (stderr, "FATAL: %s has invalid header data\n", fn);

      return 1;
    }

  (void)decode (data + pstart, (long)lit_src - TPA - pstart, out, outlen,
                LITCNT, data + ((long)lit_src - TPA));

  if (!oname)
    {
      mkname (fn, ".unp", nb, sizeof (nb));
      oname = nb;
    }

  if (writefile (oname, out, outlen))
    {
      (void)fprintf (stderr, "FATAL: cannot write %s\n", oname);

      return 1;
    }

  if (verbose)
    (void)fprintf (stderr, "  %-12s %6ld => %6ld  -> %s\n",
                   fn, n, outlen, oname);

  return 0;
}
# else

/*
 * Streaming restore: the compressed file and the decompressed output are
 * malloc'd to their exact sizes, so -R is limited only by the heap (not a
 * fixed buffer) and the compression path keeps all of RAM for its window.
 * Files too large for the heap are rejected cleanly.
 */

static int
do_restore (const char *fn, const char *oname, int verbose)
{
  unsigned char *data, *out;
  long n, outlen, pstart;
  unsigned stubv, lit_src;
  char nb[64];
  FILE *f;
  size_t r;

  n = count_file (fn);

  if (n < 0)
    {
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  data = (unsigned char *)malloc ((size_t)(n > 0 ? n : 1));

  if (!data)
    {
      (void)fprintf (stderr,
                     "FATAL: %s too large to restore (out of memory)\n", fn);

      return 1;
    }

  f = fopen (fn, "rb");

  if (!f)
    f = fopen (fn, "r");

  if (!f)
    {
      FREE (data);
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  r = fread (data, 1, (size_t)n, f);
  (void)fclose (f);

  if ((long)r != n || parse_header (data, n, &stubv, &lit_src, &outlen))
    {
      FREE (data);
      (void)fprintf (stderr, "FATAL: %s is not a POPCOM/LZPACK file\n", fn);

      return 1;
    }

  pstart = TPA + LITCNT - TPA;

  if (outlen > MZXFILE)
    {
      FREE (data);
      (void)fprintf (stderr, "FATAL: %s expands beyond MZXFILE=%ld\n", fn,
               (long)MZXFILE);

      return 1;
    }

  if ((long)lit_src - TPA < 0 ||
      (long)lit_src - TPA + LITCNT > n || outlen < LITCNT)
    {
      FREE (data);
      (void)fprintf (stderr, "FATAL: %s has invalid header data\n", fn);

      return 1;
    }

  out = (unsigned char *)malloc ((size_t)outlen);

  if (!out)
    {
      FREE (data);
      (void)fprintf (stderr,
                     "FATAL: %s too large to restore (out of memory)\n", fn);

      return 1;
    }

  (void)decode (data + pstart, (long)lit_src - TPA - pstart, out, outlen,
                LITCNT, data + ((long)lit_src - TPA));

  if (!oname)
    {
      mkname (fn, ".unp", nb, sizeof (nb));
      oname = nb;
    }

  if (writefile (oname, out, outlen))
    {
      FREE (data);
      FREE (out);
      (void)fprintf (stderr, "FATAL: cannot write %s\n", oname);

      return 1;
    }

  if (verbose)
    (void)fprintf (stderr,
                   "  %-12s %6ld => %6ld  -> %s\n", fn, n, outlen, oname);

  FREE (data);
  FREE (out);

  return 0;
}
# endif
#endif

/******************************************************************************/

static int
do_list (const char *fn)
{
  unsigned char hdr[LITCNT];
  long n, outlen;
  unsigned stubv, lit_src;
  size_t got;
  FILE *f = fopen (fn, "rb");

  if (!f)
    f = fopen (fn, "r");

  if (!f)
    {
      (void)fprintf (stderr, "FATAL: cannot read %s\n", fn);

      return 1;
    }

  /*
   * Only the 16-byte header is parsed; the rest is read solely to size the
   * file, so listing never needs a whole-file buffer.  Count only after a
   * full header was read, and stop on the first short read, so fread is
   * never issued on a stream already at EOF or in error.
   */

  got = fread (hdr, 1, (size_t)LITCNT, f);
  n = (long)got;

  if (got == (size_t)LITCNT)
    {
      unsigned char buf[128];

      for (;;)
        {
          size_t r = fread (buf, 1, sizeof (buf), f);

          n += (long)r;

          if (r < sizeof (buf))
            break;
        }
    }

  (void)fclose (f);

  if (got < (size_t)LITCNT || parse_header (hdr, n, &stubv, &lit_src, &outlen))
    {
      (void)printf ("  %-16s (not a LZPACK/POPCOM file)\n", fn);

      return 0;
    }

  {
    long p10 = (outlen ? (n * 1000L + outlen / 2) / outlen : 0);

    (void)printf ("  %-16s compressed %6ld   original %6ld   (%ld.%ld%%)\n",
                  fn, n, outlen, p10 / 10, p10 % 10);
  }

  return 0;
}

/******************************************************************************/

static void
herald (FILE *f)
{
  (void)fprintf (f,
    "LZPACK %s - PopCom!-compatible 48K CP/M-80 executable compressor\n"
    "Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>\n",
    LZPACK_VER);
}

/******************************************************************************/

static void
usage (void)
{
#ifdef POPCOM_NO_OPT
  static const char* exopt = "";
  static const char* expad = "     ";
  static const char* extra = "";
#else
# ifndef POPCOM_DECODE_ONLY
  static const char* exopt = "[-e] ";
  static const char* expad = "";
  static const char* extra = "-e: extra, ";
# endif
#endif

  herald (stderr);

  (void)fprintf (stderr,
    "\n"
    "Usage:\n"
#ifndef POPCOM_DECODE_ONLY
# ifdef POPCOM_8080
    "  lzpack %s[-Z] <file>%s  compress (%s-Z: use Z80 stub)\n"
# else
    "  lzpack %s[-8] <file>%s  compress (%s-8: use 8080 stub)\n"
# endif
#endif
#ifndef POPCOM_COMPRESS_ONLY
    "  lzpack -R <file>         restore (decompress)\n"
#endif
    "  lzpack -L <file>         list stored sizes\n"
    "  lzpack -O <name>         set output name\n"
    "  lzpack -V                show LZPACK information\n"
#ifndef POPCOM_DECODE_ONLY
    , exopt, expad, extra
#endif
    );
}

/******************************************************************************/

static void
version (void)
{
  herald (stdout);

#ifdef LZ_CPM
  {
    unsigned w = (unsigned)bdos (12, 0);
    int sys = (int)((w >> 8) & 0xff);
    int ver = (int)(w & 0xff);
    const char *os = (sys >= 2) ? "CP/NET" : (sys == 1) ? "MP/M" : "CP/M";

    (void)printf ("%s %d.%d (BDOS %02Xh, system %02Xh)%s\n",
      os, (ver >> 4) & 0xf, ver & 0xf, ver, sys,
      ((ver >= 0x30) ? "; Last Record Byte Count supported."
                     : "; no Last Record Byte Count support."));
  }
#endif

  (void)printf (
    "The LZPACK canonical homepage is https://github.com/johnsonjh/lzpack\n"
    "This program is distributed under the terms of the MIT-0 license.\n");
}

/******************************************************************************/

int
main (int argc, char **argv)
{
  int mode = 0;
  int i, rc = 0, nfiles = 0;
  const char *oname = 0;
  int use8080 = DEFAULT_USE8080, optimal = 0;
  int showver = 0;

  /*
   * First pass: gather options (which may appear anywhere on the line) and
   * count the input files.  CP/M's CCP upper-cases the command tail, so each
   * flag is accepted in either case.
   */

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1])
        {
          char c = argv[i][1];

          if (c == 'R' || c == 'r')
            mode = 1;
          else if (c == 'L' || c == 'l')
            mode = 2;
          else if (c == '8')
            use8080 = 1;
          else if (c == 'Z' || c == 'z')
            use8080 = 0;
          else if (c == 'e' || c == 'E')
            optimal = 1;
          else if (c == 'o' || c == 'O')
            {
              if (i + 1 >= argc)
                {
                  (void)fprintf (stderr, "FATAL: -o requires an argument\n");

                  return 2;
                }

              oname = argv[++i];
            }
          else if (c == 'V' || c == 'v')
            showver = 1;
          else if (c == 'h' || c == 'H')
            {
              usage ();

              return 0;
            }
          else
            {
              (void)fprintf (stderr, "FATAL: unknown option %s\n", argv[i]);

              return 2;
            }
        }
      else
        nfiles++;
    }

  if (showver)
    {
      version ();

      return 0;
    }

  if (!nfiles)
    {
      usage ();

      return 2;
    }

  if (oname && nfiles > 1)
    {
      (void)fprintf (stderr, "FATAL: -o cannot be used with multiple files\n");

      return 2;
    }

  /* Second pass: process each input file (skipping options). */

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1])
        {
          char c = argv[i][1];

          if (c == 'o' || c == 'O')
            i++;

          continue;
        }

      if (mode == 0)
        {
#ifdef POPCOM_DECODE_ONLY
          (void)fprintf (stderr, "FATAL: this build cannot compress\n");
          rc |= 1;
#else
# ifdef POPCOM_STREAM
          rc |= do_compress_stream (argv[i], oname, 1, use8080, optimal);
# else
          rc |= do_compress (argv[i], oname, 1, use8080, optimal);
# endif
#endif
        }
      else if (mode == 1)
        {
#ifdef POPCOM_COMPRESS_ONLY
          (void)fprintf (stderr, "FATAL: this build cannot restore\n");
          rc |= 1;
#else
          rc |= do_restore (argv[i], oname, 1);
#endif
        }
      else
        rc |= do_list (argv[i]);
    }

  (void)use8080;
  (void)optimal;

  return (rc ? 1 : 0);
}

/******************************************************************************/
