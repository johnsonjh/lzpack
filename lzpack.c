/*
 * LZPACK - CP/M-80 (8080 and Z80) executable compressor
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: a5653bbc-585c-11f1-954d-80ee73e9b8e7
 * //-V::707
 */

/******************************************************************************/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#ifdef LZPACK_VER
# undef LZPACK_VER
#endif

#define LZPACK_VER "v1.04"

/******************************************************************************/

#ifdef LZPACK_DECODE_ONLY
# ifdef LZPACK_COMPRESS_ONLY
#  error "LZPACK_DECODE_ONLY and LZPACK_COMPRESS_ONLY are mutually exclusive"
# endif
# define LZ_PROG "LZUNPACK"
# define LZ_KIND "decompressor"
#else
# define LZ_PROG "LZPACK"
# define LZ_KIND "compressor"
#endif

/******************************************************************************/

#ifndef MZXFILE
# define MZXFILE 262144L
#endif

/******************************************************************************/

#ifdef LZPACK_STREAM
# ifndef LZPACK_OPT
#  ifndef LZPACK_NO_OPT
#   define LZPACK_NO_OPT
#  endif
# endif
#endif

/******************************************************************************/

#ifndef HSZ
# ifdef LZPACK_STREAM
#  define HSZ 1024
# else
#  define HSZ 32768
# endif
#endif

/******************************************************************************/

#ifdef LZPACK_STREAM
static const int never =0;
# define FREE(p) \
  do {           \
    free((p));   \
    (p) = NULL;  \
  } while(never)
#endif

/******************************************************************************/

/*
 * BUFSZ sizes the in-RAM whole-file buffers used by the non-streaming build.
 * The streaming build does not use it: -R/-L allocate exactly what they need
 * and compression streams through the dynamically-sized window.
 */

#ifndef LZPACK_STREAM
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

#ifdef LZ_CPM
static int opt_lrbc_isx = 0;
#endif

/******************************************************************************/

#if defined(LZPACK_STREAM) && defined(LZ_CPM) && defined(__Z88DK) && \
  !defined(LZPACK_NO_BDOS_IO)
# define LZ_BDOS_IO
#endif

/******************************************************************************/

#ifndef LZPACK_WILDCARD
# if defined(__AZTEC_C_42T__) && !defined(LZPACK_NO_WILDCARD)
#  define LZPACK_WILDCARD 1
# else
#  define LZPACK_WILDCARD 0
# endif
#endif

#if LZPACK_WILDCARD && (!defined(LZ_CPM) || defined(LZPACK_NO_WILDCARD))
# undef LZPACK_WILDCARD
# define LZPACK_WILDCARD 0 /* off: needs CP/M BDOS or NO_WILDCARD */
#endif

#if defined(_WIN32) && !defined(LZPACK_NO_WILDCARD)
# define LZ_WIN_WILDCARD 1
#else
# define LZ_WIN_WILDCARD 0
#endif

#if (defined(LZ_CPM) && LZPACK_WILDCARD) || LZ_WIN_WILDCARD
# define LZ_INT_WILDCARD 1
#else
# define LZ_INT_WILDCARD 0
#endif

/******************************************************************************/

#ifdef LZ_BDOS_IO

# define LZ_NFILES 2

typedef struct lzf
{
  unsigned char fcb[36]; /* CP/M file control block */
  unsigned char buf[128]; /* one-record DMA buffer */
  unsigned char idx; /* next byte position in buf (0..128) */
  unsigned char mode; /* 0 = free, 1 = reading, 2 = writing */
  unsigned char ateof; /* reading: BDOS already reported end of file */
} LZF;

/******************************************************************************/

/* Implementations follow the CP/M FCB helpers they are built on */
static LZF *lzopen (const char *fn, int wr);
static int lzgetc (LZF *f);
static int lzputc (int c, LZF *f);
static size_t lzread (void *p, size_t n, LZF *f);
static size_t lzwrite (const void *p, size_t n, LZF *f);
static int lzclose (LZF *f);
static int lzerror (LZF *f);
static void lzunlink (const char *fn);

/******************************************************************************/

/* Console output using BDOS function 2 behind the usual printf names */
static int lz_fprintf (FILE *f, const char *fmt, ...);
static int lz_printf (const char *fmt, ...);
static void lz_cput (int c);

/******************************************************************************/

static void lz_mputs (const char *m);
static const char *lz_mload (const char *m, char *out);

# define LZ_MSG_PACKED 1
# define LZ_MPUTS(f, s) ((void)(f), lz_mputs (s))

/******************************************************************************/

/* stdio streams are never opened (the CRT built with CRT_ENABLE_STDIO=0) */
# undef stdout
# undef stderr
# define stdout ((FILE *)0)
# define stderr ((FILE *)0)

/* Flawfinder: ignore */ /* False positive CWE-134 */
# define fprintf lz_fprintf /* //-V1059 */
/* Flawfinder: ignore */ /* False positive CWE-134 */
# define printf  lz_printf  /* //-V1059 */
# ifndef LZPACK_NO_PROGRESS
#  define LZ_CPUT(c) lz_cput (c)
# endif

#else

/******************************************************************************/

typedef FILE LZF;

# define lzread(p, n, f)  fread ((p), 1, (n), (f))
# define lzwrite(p, n, f) fwrite ((p), 1, (n), (f))
# define lzclose(f)       fclose (f)

# ifdef LZPACK_STREAM
#  define lzgetc(f)       getc (f)
#  define lzputc(c, f)    putc ((c), (f))
#  define lzerror(f)      ferror (f)
# endif
# ifndef LZPACK_NO_PROGRESS
#  define LZ_CPUT(c)      ((void)putc ((c), stderr))
# endif

# ifdef LZPACK_PACKED_MSGS

#  define LZ_MSG_PACKED 1

static void lz_mfputs (const char *m, FILE *f);
static const char *lz_mload (const char *m, char *out);
static int lz_fprintf (FILE *f, const char *fmt, ...);
static int lz_printf (const char *fmt, ...);

#  define LZ_MPUTS(f, s) lz_mfputs ((s), (f))

/* Flawfinder: ignore */ /* False positive CWE-134 */
#  define fprintf lz_fprintf /* //-V1059 */
/* Flawfinder: ignore */ /* False positive CWE-134 */
#  define printf  lz_printf  /* //-V1059 */

# else

#  define LZ_MPUTS(f, s) ((void)fputs ((s), (f)))

# endif

/******************************************************************************/

static LZF *
lzopen (const char *fn, int wr)
{
  FILE *f = fopen (fn, wr ? "wb" : "rb");

  if (!f)
    f = fopen (fn, wr ? "w" : "r");

  return f;
}

#endif

/******************************************************************************/

#if defined(__8080)
# include "csr8080.h"
# define LZ_ASM_RESTORE
# define LZ_RCORE decompr8080
# define LZ_RCORE_LEN S8R_DLEN
# define LZ_RCORE_FIX decompr8080_fix
# define LZ_RCORE_FIX_N DECOMPR8080_FIX_N
# define LZ_RCORE_SRCV S8R_SRCV_INIT
# define LZ_RCORE_DSTV S8R_DSTV_INIT
# define LZ_RCORE_OEHI S8R_OUT_END_HI
# define LZ_RCORE_OELO S8R_OUT_END_LO
#elif defined(__Z80)
# include "csrz80.h"
# define LZ_ASM_RESTORE
# define LZ_RCORE decomprz80
# define LZ_RCORE_LEN SRZ_DLEN
# define LZ_RCORE_FIX decomprz80_fix
# define LZ_RCORE_FIX_N DECOMPRZ80_FIX_N
# define LZ_RCORE_SRCV SRZ_SRCV_INIT
# define LZ_RCORE_DSTV SRZ_DSTV_INIT
# define LZ_RCORE_OEHI SRZ_OUT_END_HI
# define LZ_RCORE_OELO SRZ_OUT_END_LO
#endif

/******************************************************************************/

#if !defined(LZ_ASM_RESTORE) && !defined(LZPACK_COMPRESS_ONLY)
# if defined(__ELKS__) || (defined(__WATCOMC__) && defined(__I86__)) || \
     defined(__AZTEC_C_42T__) && !defined(LZPACK_NO_ASM_RESTORE_86)
#  define LZ_ASM_RESTORE_86
# endif
#endif

/******************************************************************************/

#include "csmsg.h"

/******************************************************************************/

#include "cschk.h"

#define CHK_SP_SLACK 24

/******************************************************************************/

#ifndef LZPACK_STREAM
static unsigned char g_a[BUFSZ];
static unsigned char g_b[BUFSZ];
#endif

/******************************************************************************/

/*
 * Self-extracting stub architecture.  By default lzpack autodetects, per input
 * file, whether the program needs a Z80 (see op8080_len[] and is_z80_*()) and
 * picks the Z80 or 8080 self-extractor to match, so the packed file runs
 * wherever the original would.  -8 forces the 8080 stub and -Z the Z80 stub.
 *
 * DEFAULT_USE8080 is the fallback used only when autodetection is compiled out
 * (-DLZPACK_NO_AUTOARCH) and neither -8 nor -Z is given.  z88dk predefines
 * __8080 when its 8080 library is in use (-clib=8080), so an 8080 host does a
 * fallback to the 8080 stub; a set LZPACK_8080 may also be set to force that.
 */

#ifdef __8080
# ifndef LZPACK_8080
#  define LZPACK_8080
# endif
#endif

#ifdef LZPACK_8080
# define DEFAULT_USE8080 1
#else
# define DEFAULT_USE8080 0
#endif

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY
# ifndef LZPACK_STREAM
static unsigned char g_c[BUFSZ];

static void *
lxmalloc (size_t n)
{
  void *p = malloc (n);

  if (!p)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_OOM);

      exit (1);
    }

  return p;
}
# endif
#endif

/******************************************************************************/

#define TPA 0x100
#define LITCNT 16

/******************************************************************************/

#define MAXDIST 8192
#define MAXLEN 256
#define MEMTOP 0xBDFF

/******************************************************************************/

#ifndef LZ_STDBLK
# ifdef LZPACK_STREAM
#  define LZ_STDBLK 512L
# else
#  define LZ_STDBLK 2048L
# endif
#endif

/******************************************************************************/

#if MZXFILE <= 65535L
# define LZ_POS16 1 /* positions fit 16 bits; lzpos/lzcost may narrow */
typedef unsigned int lzpos;
typedef unsigned int lzcost;
#else
typedef long lzpos;
typedef long lzcost;
#endif

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY

# include "csz80.h"
# include "cs8080.h"

# define Z80_HEADROOM 51
# define STUBLEN (Z80_SETUP_LEN + Z80_DCMP_LEN)

# if S8_DLEN > 256
#  error "8080 decompressor exceeds 256 bytes; widen SL3 counter in s8080s.asm"
# endif

/******************************************************************************/

/*
 * Runtime MEMTOP (-M) override.  memtop is only the accept/reject ceiling
 * for the relocated stub: it never changes the bytes a fitting file gets.
 * The floor is 4K plus the worst-case (8080) relocated stub tail plus a
 * 128-byte record of stack slop, and tracks the stubs if they ever grow.
 */

# define MEMTOP_MIN (0x1000 + 51 + S8_DLEN + 128)

/* Set to MEMTOP at startup (-M overrides): zero-initialized so it stays in
 * BSS, where the CP/M-80 build's trailing-zero .COM trim can elide it. */
static unsigned memtop;

/*
 * -C prefixes the stub with the runtime memory check (chk.asm): it refuses
 * to unpack when the highest write would reach the BDOS base (word at
 * 0x0006) or come within CHK_SP_SLACK bytes of the live inherited stack.
 */

static int opt_checked = 0;

# ifndef LZPACK_COMPRESS_ONLY

static unsigned opt_chk_floor = 0;

# endif

/******************************************************************************/

# ifndef LZPACK_STREAM
static long ol, tagpos;
# endif
static int tagcnt;

/******************************************************************************/

# ifndef LZPACK_NO_PROGRESS

static const char *pg_name;
static long pg_total, pg_next, pg_step;
static int pg_pct, pg_on, pg_w;

static void
prog_begin (int on, long total, const char *name)
{
  pg_on = on;
  pg_name = name;
  pg_total = total;
  pg_step = total / 100;

  if (pg_step < 1)
    pg_step = 1;

  pg_next = pg_step;
  pg_pct = -1;

  pg_w = (int)strlen (name);

  if (pg_w < 12)
    pg_w = 12;

  pg_w += 11;
}

/******************************************************************************/

static void
prog_show (const char *tag, long done)
{
  int pct;

  if (!pg_on || pg_total < 1 || done < pg_next)
    return;

  pg_next = done + pg_step;

  pct =
    (int)((done * 1000L / pg_total) * (HSZ + done) / (HSZ + pg_total) / 10);

  if (pct == pg_pct)
    return;

  pg_pct = pct;
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)fprintf (stderr, MSG_P_PROG, pg_name, tag, pct);
}

/******************************************************************************/

static void
prog_done (void)
{
  int i;

  if (!pg_on)
    return;

  pg_on = 0;

  LZ_CPUT ('\r');

  for (i = 0; i < pg_w; i++)
    LZ_CPUT (' ');

  LZ_CPUT ('\r');
}

# else
#  define prog_begin(on, total, name) ((void)0)
#  define prog_show(tag, done)        ((void)0)
#  define prog_done()                 ((void)0)
# endif

/******************************************************************************/

# ifndef LZPACK_STREAM

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
#   ifndef NEED_REMOVE
#    define NEED_REMOVE
#   endif
#  endif

#  ifdef __IA16_SYS_ELKS
#   include <unistd.h>
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

#  ifndef LZ_BDOS_IO
#   define lzunlink(fn) ((void)remove (fn))
#  endif

/******************************************************************************/

/*
 * Streaming encoder: the payload is written to a temp file as it is
 * produced, so only a small hold buffer is kept in RAM.  The single tag
 * byte is the only byte ever modified after being written, and the buffer
 * is flushed whole exactly when each new tag byte starts, so the live tag
 * byte is ALWAYS s_obuf[0] and the bytes flushed are always final.  At
 * most 1 + 8 * 2 bytes accumulate between tag boundaries (eight tokens of
 * at most two payload bytes each), so OBSZ has ample slack.  s_ototal
 * counts flushed bytes; it is the only wide accumulator on this path (an
 * incompressible input's payload can pass 65535) but is touched only once
 * per flush, so all per-bit and per-byte arithmetic stays 16-bit native.
 */

#  ifndef OBSZ
#   define OBSZ 128
#  endif

static LZF *s_of;
static unsigned char s_obuf[OBSZ];
static unsigned s_on;
static long s_ototal;

/******************************************************************************/

static void
obuf_flush (void)
{
  if (s_on)
    {
      (void)lzwrite (s_obuf, (size_t)s_on, s_of);

      s_ototal += (long)s_on;
      s_on = 0;
    }
}

/******************************************************************************/

static void
e_init_stream (LZF *f)
{
  s_of = f;
  s_on = 0;
  s_ototal = 0;
  tagcnt = 8;
}

/******************************************************************************/

static void
e_bit (int b)
{
  if (tagcnt == 8)
    {
      obuf_flush ();
      s_obuf[0] = 0; /* the new tag byte; see the flush-whole invariant */
      s_on = 1;
      tagcnt = 0;
    }

  if (b)
    s_obuf[0] |= (unsigned char)(1 << (7 - tagcnt));

  tagcnt++;
}

/******************************************************************************/

static void
e_byte (int x)
{
  s_obuf[s_on++] = (unsigned char)(x & 0xff);
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

# ifndef LZPACK_STREAM
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

# ifndef LZPACK_STREAM

/*
 * The standard engine is a bounded-block cost-optimal parser.  It runs the
 * same shortest-path DP as the -E engine, but only over a small sliding block
 * at a time (LZ_STDBLK positions), so its working set is a few KB regardless
 * of file size, leaving room for a large match window even on a 48K TPA.
 * Matches are found against the full preceding dictionary (hash chains span
 * back up to MAXDIST); only the parse DP is blocked.  Because matches never
 * exceed MAXLEN and the block is many times larger, the loss from not letting
 * a match cross a block boundary is negligible, yet the parse is dramatically
 * better than the old greedy+lazy heuristic.  The -E engine raises the block
 * size (and so the memory) to remove even that residual boundary loss.
 */

static long
compress (const unsigned char *data, long n, int start, unsigned char *out,
          int depth, long blk)
{
  long seg_start, ins;
  long *cost;
  int *tlen, *tdist, *stk;
  static int l2d[MAXLEN + 1];

  D = data;
  N = n;

  if (blk < 1)
    blk = 1;

  lnk = (int *)lxmalloc (sizeof (int) * (size_t)(n > 0 ? n : 1));
  cost = (long *)lxmalloc (sizeof (long) * (size_t)(blk + 1));
  tlen = (int *)lxmalloc (sizeof (int) * (size_t)(blk + 1));
  tdist = (int *)lxmalloc (sizeof (int) * (size_t)(blk + 1));
  stk = (int *)lxmalloc (sizeof (int) * (size_t)(blk + 1));

  {
    long j;

    for (j = 0; j < HSZ; j++)
      head[j] = -1;
  }

  e_init (out);

  for (ins = 0; ins < start && ins + 2 < n; ins++)
    hinsert (ins);

  ins = start;

  for (seg_start = start; seg_start < n;)
    {
      long seg_end = seg_start + blk;
      long span, j, apos;

      if (seg_end > n)
        seg_end = n;

      span = seg_end - seg_start;

      for (j = 0; j <= span; j++)
        {
          cost[j] = 0x3fffffffL;
          tlen[j] = 0;
          tdist[j] = 0;
        }

      cost[0] = 0;

      for (apos = seg_start; apos < seg_end; apos++)
        {
          long jc = apos - seg_start;
          int cap;

          while (ins < apos)
            {
              hinsert (ins);
              ins++;
            }

          if (cost[jc] + 9 < cost[jc + 1])
            {
              cost[jc + 1] = cost[jc] + 9;
              tlen[jc + 1] = 1;
              tdist[jc + 1] = 0;
            }

          cap = MAXLEN;

          if ((long)cap > seg_end - apos)
            cap = (int)(seg_end - apos);

          if ((long)cap > n - apos)
            cap = (int)(n - apos);

          if (cap >= 3 && apos + 2 < n)
            {
              int h = hash3 (apos), dep = depth, maxml = 0;
              long p;

              for (p = head[h]; p >= 0 && dep-- > 0; p = lnk[p])
                {
                  long d = apos - p;
                  int ml;

                  if (d > MAXDIST)
                    break;

                  if (maxml > 0 && maxml < cap
                      && D[p + maxml] != D[apos + maxml])
                    continue;

                  ml = 0;

                  while (ml < cap && D[p + ml] == D[apos + ml])
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
                    int dd = l2d[L];
                    long c2 = cost[jc] + match_bits (dd, L);

                    if (c2 < cost[jc + L])
                      {
                        cost[jc + L] = c2;
                        tlen[jc + L] = L;
                        tdist[jc + L] = dd;
                      }
                  }
              }
            }

          if (cap >= 2 && apos + 1 < n)
            {
              long lo = (apos > 128) ? (apos - 128) : 0;
              long p;

              for (p = apos - 1; p >= lo; p--)
                if (data[p] == data[apos] && data[p + 1] == data[apos + 1])
                  {
                    long c2 = cost[jc] + match_bits ((int)(apos - p), 2);

                    if (c2 < cost[jc + 2])
                      {
                        cost[jc + 2] = c2;
                        tlen[jc + 2] = 2;
                        tdist[jc + 2] = (int)(apos - p);
                      }

                    break;
                  }
            }
        }

      {
        long sp = 0, kk;

        for (kk = span; kk > 0;)
          {
            stk[sp++] = (int)kk;
            kk -= (tlen[kk] > 1 ? tlen[kk] : 1);
          }

        for (kk = sp - 1; kk >= 0; kk--)
          {
            int e = stk[kk];
            int L = tlen[e];

            if (L > 1)
              e_match (tdist[e], L);
            else
              e_lit (data[seg_start + e - 1]);
          }
      }

      while (ins < seg_end)
        {
          hinsert (ins);
          ins++;
        }

      seg_start = seg_end;

      prog_show ("", seg_start - start);
    }

  free (lnk);
  free (cost);
  free (tlen);
  free (tdist);
  free (stk);

  return ol;
}

# endif
#endif

/******************************************************************************/

#ifndef LZPACK_COMPRESS_ONLY

# if !defined(LZ_ASM_RESTORE) && !defined(LZ_ASM_RESTORE_86)
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
          /* Flawfinder: ignore */ /* False positive CWE-134 */
          (void)fprintf (stderr, MSG_E_EOD);

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
          /* Flawfinder: ignore */ /* False positive CWE-134 */
          (void)fprintf (stderr, MSG_E_EOD);

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
              /* Flawfinder: ignore */ /* False positive CWE-134 */
              (void)fprintf (stderr, MSG_E_EOD);

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
          /* Flawfinder: ignore */ /* False positive CWE-134 */
          (void)fprintf (stderr, MSG_E_UNDERFLOW);

          exit (1);
        }

      mpos = pos - ((long)off + 1);

      for (i = 0; i < (int)ml; i++)
        if (pos < outlen)
          out[pos++] = out[mpos++];
    }

  return pos;
}

# endif

# ifdef LZ_ASM_RESTORE_86

/******************************************************************************/

extern void lz86_decode (void);
#  ifdef __AZTEC_C_42T__
extern unsigned lz86_src, lz86_dst, lz86_oend;
#  else
unsigned lz86_src, lz86_dst, lz86_oend;
#  endif

/******************************************************************************/

static long
decode (const unsigned char *pl, long pllen, unsigned char *out, long outlen,
        int litcnt, const unsigned char *litsrc)
{
  (void)pllen;
  (void)memcpy (out, litsrc, (size_t)litcnt); /* seed the literal prefix */

  lz86_src = (unsigned)pl;
  lz86_dst = (unsigned)(out + litcnt);
  lz86_oend = (unsigned)(out + outlen);
  lz86_decode ();

  return outlen;
}

# endif
#endif

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY
# ifndef LZPACK_STREAM
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

#  ifndef __VBCC__
  (void)pl_len;
#  endif

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

#  ifndef __VBCC__
  (void)bit;
#  endif

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
# ifndef BDOS_FCB
#  ifdef __Z88DK
#   define BDOS_FCB(p) ((int)(p))
#  else
#   define BDOS_FCB(p) (p)
#  endif
# endif
#endif

/******************************************************************************/

#ifdef __AZTEC_C_42T__
# if LZPACK_WILDCARD
#  define LZ_STACK_RESERVE 1792
# else
#  define LZ_STACK_RESERVE 2048
# endif
extern void rsvstk ();
# define LZ_GUARD_STACK() rsvstk (LZ_STACK_RESERVE)
#else
# define LZ_GUARD_STACK() ((void)0)
#endif

/******************************************************************************/

#ifdef LZ_CPM

static int
cpm_has_lrbc (void)
{
  return (bdos (12, 0) & 0x00ff) >= 0x30;
}

/******************************************************************************/

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
    {
      if (p[0] >= 'A' && p[0] <= 'Z')
        fcb[0] = (unsigned char)(p[0] - 'A' + 1);
      else if (p[0] >= 'a' && p[0] <= 'z')
        fcb[0] = (unsigned char)(p[0] - 'a' + 1);

      p += 2;
    }

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

  if (!cpm_has_lrbc ())
    return -1;

  cpm_setfcb (fcb, fn);
  (void)bdos (35, BDOS_FCB (fcb));
  records = ((long)fcb[33])
          | ((long)fcb[34] << 8)
          | ((long)fcb[35] << 16);

  if (records <= 0)
    return -1;

  last_ext = (records - 1L) / 128L;

  cpm_setfcb (fcb, fn);
  fcb[12] = (unsigned char)(last_ext & 0x1f);
  fcb[14] = (unsigned char)((last_ext >> 5) & 0x3f);

  if ((bdos (15, BDOS_FCB (fcb)) & 0x00ff) == 0xff)
    return -1;

  lrbc = fcb[13] & 0xff;
  (void)bdos (16, BDOS_FCB (fcb));

  if (lrbc <= 0 || lrbc >= 128)
    return records * 128L;

  if (opt_lrbc_isx)
    return records * 128L - (long)lrbc;

  return (records - 1L) * 128L + lrbc;
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

  if (!cpm_has_lrbc ())
    return -1;

  records = (nbytes + 127L) / 128L;
  last_ext = (records - 1L) / 128L;

  if (opt_lrbc_isx)
    lrbc = (int)(records * 128L - nbytes);
  else
    lrbc = (int)(nbytes - (records - 1L) * 128L);

  cpm_setfcb (fcb, fn);
  fcb[12] = (unsigned char)(last_ext & 0x1f);
  fcb[14] = (unsigned char)((last_ext >> 5) & 0x3f);

  if ((bdos (15, BDOS_FCB (fcb)) & 0x00ff) == 0xff)
    return -1;

  fcb[13] = (unsigned char)(lrbc & 0x7f);
  (void)bdos (16, BDOS_FCB (fcb));

  return 0;
}

#endif

/******************************************************************************/

#if defined(LZ_MSG_PACKED) && !(defined(LZ_BDOS_IO) && defined(__SCCZ80))

static const unsigned char *
lz_ment (int idx)
{
  const unsigned char *p = lz_msgbook;

  while (0 < idx--)
    {
      while (0 == (0x80 & *p))
        p++;

      p++;
    }

  return p;
}

/******************************************************************************/

static const char *
lz_mload (const char *m, char *out)
{
  const unsigned char *s = (const unsigned char *)m;
  char *o = out;

  while (*s)
    {
      int c = *s++;

      if (0x80 > c)
        *o++ = (char)c;
      else
        {
          const unsigned char *e = lz_ment (c - 0x80);
          int ch;

          do
            {
              ch = *e++;
              *o++ = (char)(0x7f & ch);
            }
          while (0 == (0x80 & ch));
        }
    }

  *o = 0;

  return out;
}

#endif

/******************************************************************************/

#ifdef LZ_BDOS_IO

# include <stdarg.h>

static LZF lz_files[LZ_NFILES];

static LZF *
lzopen (const char *fn, int wr)
{
  LZF *f;
  int i;

  for (i = 0; i < LZ_NFILES; i++)
    if (!lz_files[i].mode)
      break;

  if (i == LZ_NFILES)
    return NULL;

  f = &lz_files[i];
  cpm_setfcb (f->fcb, fn);

  if (wr)
    {
      (void)bdos (19, BDOS_FCB (f->fcb)); /* delete any old file */
      cpm_setfcb (f->fcb, fn); /* fn 19 searches: rebuild */

      if ((bdos (22, BDOS_FCB (f->fcb)) & 0xff) == 0xff) /* make */
        return NULL;
    }
  else if ((bdos (15, BDOS_FCB (f->fcb)) & 0xff) == 0xff) /* open */
    return NULL;

  f->fcb[32] = 0; /* current record: start of file */
  f->mode = (unsigned char)(wr ? 2 : 1);
  f->idx = (unsigned char)(wr ? 0 : 128); /* reading: force first fill */
  f->ateof = 0;

  return f;
}

/******************************************************************************/

static int
lzgetc (LZF *f)
{
  if (f->idx >= 128)
    {
      if (f->ateof)
        return EOF;

      (void)bdos (26, BDOS_FCB (f->buf)); /* DMA to this slot */

      if ((bdos (20, BDOS_FCB (f->fcb)) & 0xff) != 0) /* read seq */
        {
          f->ateof = 1;

          return EOF;
        }

      f->idx = 0;
    }

  return f->buf[f->idx++];
}

/******************************************************************************/

static int
lz_flushrec (LZF *f)
{
  (void)bdos (26, BDOS_FCB (f->buf)); /* DMA to this slot */

  if ((bdos (21, BDOS_FCB (f->fcb)) & 0xff) != 0) /* write seq */
    return -1; /* disk full */

  f->idx = 0;

  return 0;
}

/******************************************************************************/

static int
lzputc (int c, LZF *f)
{
  f->buf[f->idx++] = (unsigned char)c;

  if (f->idx >= 128 && lz_flushrec (f))
    return EOF;

  return (unsigned char)c;
}

/******************************************************************************/

static size_t
lzread (void *p, size_t n, LZF *f)
{
  unsigned char *d = (unsigned char *)p;
  size_t got = 0;
  int c;

  while (got < n && (c = lzgetc (f)) != EOF)
    d[got++] = (unsigned char)c;

  return got;
}

/******************************************************************************/

static size_t
lzwrite (const void *p, size_t n, LZF *f)
{
  const unsigned char *s = (const unsigned char *)p;
  size_t i;

  for (i = 0; i < n; i++)
    if (lzputc (s[i], f) == EOF)
      break;

  return i;
}

/******************************************************************************/

static int
lzclose (LZF *f)
{
  int rc = 0;

  if (f->mode == 2 && f->idx)
    {
      while (f->idx < 128)
        f->buf[f->idx++] = 0x1a; /* ^Z pad */

      if (lz_flushrec (f))
        rc = -1;
    }

  if ((bdos (16, BDOS_FCB (f->fcb)) & 0xff) == 0xff) /* close */
    rc = -1;

  f->mode = 0;

  return rc;
}

/******************************************************************************/

static int
lzerror (LZF *f)
{
  (void)f;

  return 0;
}

/******************************************************************************/

static void
lzunlink (const char *fn)
{
  unsigned char fcb[36];

  cpm_setfcb (fcb, fn);
  (void)bdos (19, BDOS_FCB (fcb));
}

/******************************************************************************/

# ifdef __SCCZ80

static void
lz_cput (int c)
{
#  asm
        ld      hl,2
        add     hl,sp
        ld      a,(hl)
.lzmd_pra                       ; print A, mapping LF to CR LF
        push    hl              ; preserved for the decode loops
        push    bc
        cp      10
        jp      nz,lzmd_pr1
        ld      a,13            ; recurse: 13 is not 10, prints the CR
        call    lzmd_pra
        ld      a,10
.lzmd_pr1
        ld      e,a
        ld      c,2             ; BDOS console output
        call    5
        pop     bc
        pop     hl              ; falls onto the RET sccz80 emits below
#  endasm
}

/******************************************************************************/

static void
lz_cputs (const char *s)
{
#  asm
        pop     bc              ; return address
        pop     hl              ; s
        push    hl
        push    bc
.lzmd_cs1
        ld      a,(hl)
        or      a
        ret     z
        inc     hl
        call    lzmd_pra
        jp      lzmd_cs1
#  endasm
}

/******************************************************************************/

static void
lz_mputs (const char *m)
{
#  asm
        pop     bc              ; return address
        pop     hl              ; m
        push    hl
        push    bc
.lzmd_mp1
        ld      a,(hl)          ; next coded byte
        inc     hl
        add     a,a             ; CY = book bit; Z only for 0 or 128
        jp      c,lzmd_mp2      ; high bytes index the book
        ret     z               ; CY clear too: NUL ends the message
        rra                     ; CY still clear: restore the literal
        call    lzmd_pra
        jp      lzmd_mp1
.lzmd_mp2
        push    hl
        call    lzmd_walk       ; HL = entry text
.lzmd_mp3
        ld      a,(hl)          ; entry byte; bit 7 marks the last one
        inc     hl
        or      a
        jp      m,lzmd_mp4
        call    lzmd_pra
        jp      lzmd_mp3
.lzmd_mp4
        and     127             ; strip the terminator bit
        call    lzmd_pra
        pop     hl              ; resume the coded string
        jp      lzmd_mp1
.lzmd_walk                      ; Z + A = doubled index -> HL
        ld      hl,_lz_msgbook  ; bit-7-terminated entries, no lengths
.lzmd_w1
        jp      z,lzmd_w2       ; Z: add a,a on entry, dec a after
        ld      c,a             ; park the doubled index
.lzmd_w3
        ld      a,(hl)          ; skip one entry: scan just past its
        inc     hl              ;   bit-7 terminator byte
        or      a
        jp      p,lzmd_w3
        ld      a,c
        dec     a               ; the index rides doubled, so count
        dec     a               ;   it down by two per entry
        jp      lzmd_w1
.lzmd_w2                        ; falls onto the RET sccz80 emits below
#  endasm
}

/******************************************************************************/

static const char *
lz_mload (const char *m, char *out)
{
#  asm
        pop     bc              ; return address
        pop     de              ; out
        pop     hl              ; m
        push    hl
        push    de
        push    bc
        push    de              ; out again: the return value
.lzmd_ml1
        ld      a,(hl)          ; next coded byte
        inc     hl
        add     a,a             ; CY = book bit; Z only for 0 or 128
        jp      c,lzmd_ml2      ; high bytes index the book
        rra                     ; CY clear: restore literal or NUL
        ld      (de),a          ; store the literal or the final NUL
        inc     de
        or      a
        jp      nz,lzmd_ml1     ; NUL ends the message
        pop     hl              ; return out
        ret
.lzmd_ml2
        push    hl
        call    lzmd_walk       ; HL = entry text
.lzmd_ml3
        ld      a,(hl)          ; entry byte; bit 7 marks the last one
        inc     hl
        or      a
        jp      m,lzmd_ml4
        ld      (de),a
        inc     de
        jp      lzmd_ml3
.lzmd_ml4
        and     127             ; strip the terminator bit
        ld      (de),a
        inc     de
        pop     hl              ; resume the coded string
        jp      lzmd_ml1
#  endasm
}

/******************************************************************************/

# else

static void
lz_cput (int c)
{
  if (c == '\n')
    (void)bdos (2, '\r');

  (void)bdos (2, c);
}

/******************************************************************************/

static void
lz_cputs (const char *s)
{
  while (*s)
    lz_cput (*s++);
}

/******************************************************************************/

static void
lz_mputs (const char *m)
{
  const unsigned char *s = (const unsigned char *)m;

  while (*s)
    {
      int c = *s++;

      if (0x80 > c)
        lz_cput (c);
      else
        {
          const unsigned char *e = lz_ment (c - 0x80);
          int ch;

          do
            {
              ch = *e++;
              lz_cput (0x7f & ch);
            }
          while (0 == (0x80 & ch));
        }
    }
}

# endif

/******************************************************************************/

static char lz_lbuf[88];
static char lz_fbuf[80];

static int
lz_fprintf (FILE *f, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  (void)f;
# ifdef __SCCZ80
  (void)fmt;
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)vsnprintf (lz_lbuf, sizeof (lz_lbuf),
                   lz_mload (*(const char **)(void *)ap, lz_fbuf),
                   (void *)(ap - 2));
# else
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)vsnprintf (lz_lbuf, sizeof (lz_lbuf), lz_mload (fmt, lz_fbuf), ap);
# endif
  va_end (ap);
  lz_cputs (lz_lbuf);

  return 0;
}

/******************************************************************************/

static int
lz_printf (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
# ifdef __SCCZ80
  (void)fmt;
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)vsnprintf (lz_lbuf, sizeof (lz_lbuf),
                   lz_mload (*(const char **)(void *)(ap + 2), lz_fbuf),
                   (void *)ap);
# else
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)vsnprintf (lz_lbuf, sizeof (lz_lbuf), lz_mload (fmt, lz_fbuf), ap);
# endif
  va_end (ap);
  lz_cputs (lz_lbuf);

  return 0;
}

#endif

/******************************************************************************/

#if defined(LZ_MSG_PACKED) && !defined(LZ_BDOS_IO)

# include <stdarg.h>

static char lz_fbuf[80];

static void
lz_mfputs (const char *m, FILE *f)
{
  const unsigned char *s = (const unsigned char *)m;

  while (*s)
    {
      int c = *s++;

      if (0x80 > c)
        (void)putc (c, f);
      else
        {
          const unsigned char *e = lz_ment (c - 0x80);
          int ch;

          do
            {
              ch = *e++;
              (void)putc (0x7f & ch, f);
            }
          while (0 == (0x80 & ch));
        }
    }
}

/******************************************************************************/

# if defined(__GNUC__) || defined(__clang__) && \
   !(defined(__OPEN64__) || defined(__OPENCC__) || defined(__PCC__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif

static int
lz_fprintf (FILE *f, const char *fmt, ...)
{
  int r;
  va_list ap;

  va_start (ap, fmt);
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  r = vfprintf (f, lz_mload (fmt, lz_fbuf), ap);
  va_end (ap);

  return r;
}

/******************************************************************************/

static int
lz_printf (const char *fmt, ...)
{
  int r;
  va_list ap;

  va_start (ap, fmt);
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  r = vfprintf (stdout, lz_mload (fmt, lz_fbuf), ap);
  va_end (ap);

  return r;
}

# if defined(__GNUC__) || defined(__clang__) && \
   !(defined(__OPEN64__) || defined(__OPENCC__) || defined(__PCC__))
#  pragma GCC diagnostic pop
# endif

#endif

/******************************************************************************/

#ifndef LZPACK_STREAM
static long
readfile (const char *fn, unsigned char *buf, size_t max)
{
  LZF *f = lzopen (fn, 0);
  size_t n;

  if (!f)
    return -1;

  n = lzread (buf, max, f);

  if (n == max)
    {
      unsigned char c;

      if (lzread (&c, 1, f) > 0)
        {
          (void)lzclose (f);

          return (long)max + 1;
        }
    }

  (void)lzclose (f);

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

#if !defined(LZPACK_STREAM) || !defined(LZPACK_COMPRESS_ONLY)

static int
writefile (const char *fn, const unsigned char *buf, long n)
{
  LZF *f = lzopen (fn, 1);

  if (!f)
    return -1;

  (void)lzwrite (buf, (size_t)n, f);
  (void)lzclose (f);

# ifdef LZ_CPM
  (void)cpm_set_byte_count (fn, n);
# endif

  return 0;
}

#endif

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY
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

#ifdef LZPACK_STREAM

/*
 * Streaming-build helpers shared by the streaming compressor and the
 * streaming (-R) restore path, kept outside the compress-only region so
 * a decode-only (LZUNPACK) build gets them too.  mg_byte/min_gap_stream
 * walk the emitted format to find the tightest source-over-destination
 * gap the in-place decompressor will see; count_file sizes an input
 * without buffering it.
 */

static LZF *s_mg_f;
static long s_mg_rd;

static int
mg_byte (void)
{
  s_mg_rd++;

  return lzgetc (s_mg_f);
}

/******************************************************************************/

static long
min_gap_stream (LZF *f, long pl_len, long outlen, int litcnt, long pl_dst_top)
{
  long src_base = pl_dst_top + 1 - pl_len;
  long dst_base = (long)TPA + litcnt;
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

# ifndef __VBCC__
  (void)bit;
# endif

  return ming;
}

/******************************************************************************/

static long
count_file (const char *fn)
{
  LZF *f = lzopen (fn, 0);
  long n = 0;
  unsigned char buf[128];

  if (!f)
    return -1;


  for (;;)
    {
      size_t r = lzread (buf, sizeof (buf), f);

      n += (long)r;

      if (r < sizeof (buf))
        break;
    }

  if (lzerror (f))
    {
      (void)lzclose (f);

      return -1;
    }

  (void)lzclose (f);

# ifdef LZ_CPM
  {
    long exact = cpm_file_size (fn);

    if (exact > 0 && exact <= n && n - exact < 128)
      n = exact;
  }
# endif

  return n;
}

#endif

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY

# ifndef LZPACK_NO_AUTOARCH

static const unsigned char op8080_len[256] = {
  1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
  1, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
  1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1,
  1, 3, 3, 1, 1, 1, 2, 1, 1, 1, 3, 1, 1, 1, 2, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 3, 3, 3, 1, 2, 1, 1, 1, 3, 3, 3, 3, 2, 1,
  1, 1, 3, 2, 3, 1, 2, 1, 1, 1, 3, 2, 3, 3, 2, 1,
  1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1,
  1, 1, 3, 1, 3, 1, 2, 1, 1, 1, 3, 1, 3, 3, 2, 1
};

# endif

/******************************************************************************/

/*
 * Emit the optional (-C) runtime memory-check block, which loads at stub_v
 * ahead of the setup block and falls through into it.  Patches the internal
 * +stub_v references and the two limits: wtop is the highest address the
 * unpack will write (Z80 stub_dst_top / 8080 dcmp_dsttop).  -F raises the
 * BDOS bound beyond wtop to the runtime floor; the stack bound stays tied
 * to the unpack writes, which are all that can reach the live stack.
 * Returns the number of bytes emitted (0 when -C is off); the caller
 * shifts the setup block and everything measured from it by that amount.
 */

static int
put_check (unsigned char *dst, lzpos stub_v, lzpos wtop)
{
  int i;

  if (!opt_checked)
    return 0;

  (void)memcpy (dst, chkstub, CHK_LEN);

  for (i = 0; i < CHKSTUB_FIX_N; i++)
    put16 (dst + chkstub_fix[i][0], (unsigned)(stub_v + chkstub_fix[i][1]));

  put16 (dst + CHK_DST_LIM, (unsigned)(wtop + 1));

# ifndef LZPACK_COMPRESS_ONLY
  if (opt_chk_floor && (lzpos)opt_chk_floor > wtop)
    put16 (dst + CHK_DST_LIM,
           (unsigned)(opt_chk_floor < 0xFFFFU
                        ? opt_chk_floor + 1U
                        : 0xFFFFU));
# endif

  put16 (dst + CHK_SP_LIM, (unsigned)(wtop + 1 + CHK_SP_SLACK));

  return CHK_LEN;
}

/******************************************************************************/

# ifndef LZPACK_STREAM

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
  long stub_dst_top = pl_dst_top + (Z80_HEADROOM + Z80_DCMP_LEN - 1);
  unsigned char *stub;
  long chk;

  put_header (outf, data, stub_v, outlen);

  (void)memcpy (outf + LITCNT, pl, (size_t)pllen);
  (void)memcpy (outf + LITCNT + pllen, data, LITCNT);

  stub = outf + LITCNT + pllen + LITCNT;

  chk = put_check (stub, stub_v, stub_dst_top);
  stub += chk;

  (void)memcpy (stub, z80_stub, STUBLEN);

  put16 (stub + P_LIT_SRC, (unsigned)lit_src);
  put16 (stub + P_STUB_SRCTOP, (unsigned)(stub_v + chk + (STUBLEN - 1)));
  put16 (stub + P_STUB_DSTTOP, (unsigned)stub_dst_top);
  put16 (stub + P_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (stub + P_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (stub + P_PL_LEN, (unsigned)pllen);
  put16 (stub + P_JP_RELOC, (unsigned)(stub_dst_top - (Z80_DCMP_LEN - 1)));

  stub[P_CP_HI] = (unsigned char)((out_end >> 8) & 0xff);
  stub[P_CP_LO] = (unsigned char)(out_end & 0xff);

  {
    /* GETBIT is CALLed; retarget call operands to the relocated routine */
    long getbit_v = stub_dst_top - (Z80_DCMP_LEN - 1) + Z80_GETBIT_OFF;
    int gi;

    for (gi = 0; gi < Z80_GETBIT_FIX_N; gi++)
      put16 (stub + z80_getbit_fix[gi], (unsigned)getbit_v);
  }

  return LITCNT + pllen + LITCNT + chk + STUBLEN;
}

/******************************************************************************/

static long
build_8080 (unsigned char *outf, const unsigned char *data, long pllen,
            const unsigned char *pl, long outlen, long pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  long lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  long decomp_file_v;
  long stub_run = pl_dst_top + 51;
  long dcmp_dsttop = stub_run + S8_DLEN - 1;
  unsigned char *su, *de;
  long chk;
  int i;

  put_header (outf, data, stub_v, outlen);

  (void)memcpy (outf + LITCNT, pl, (size_t)pllen);
  (void)memcpy (outf + LITCNT + pllen, data, LITCNT);

  su = outf + LITCNT + pllen + LITCNT;

  chk = put_check (su, stub_v, dcmp_dsttop);
  su += chk;
  de = su + S8_SLEN;
  decomp_file_v = stub_v + chk + S8_SLEN;

  (void)memcpy (su, setup8080, S8_SLEN);
  (void)memcpy (de, decomp8080, S8_DLEN);

  for (i = 0; i < SETUP8080_FIX_N; i++)
    put16 (su + setup8080_fix[i][0],
           (unsigned)(stub_v + chk + setup8080_fix[i][1]));

  put16 (su + S8S_LIT_SRC, (unsigned)lit_src);
  put16 (su + S8S_DCMP_SRCTOP, (unsigned)(decomp_file_v + S8_DLEN - 1));
  put16 (su + S8S_DCMP_DSTTOP, (unsigned)dcmp_dsttop);
  su[S8S_DCMP_LEN] = (unsigned char)S8_DLEN; /* 8-bit reloc count; see guard */
  put16 (su + S8S_DCMP_RUN, (unsigned)stub_run);

  for (i = 0; i < DECOMP8080_FIX_N; i++)
    put16 (de + decomp8080_fix[i][0],
           (unsigned)(stub_run + decomp8080_fix[i][1]));

  de[S8D_OUT_END_HI] = (unsigned char)((out_end >> 8) & 0xff);
  de[S8D_OUT_END_LO] = (unsigned char)(out_end & 0xff);

  put16 (de + S8D_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (de + S8D_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (de + S8D_PL_LEN, (unsigned)pllen);

  return LITCNT + pllen + LITCNT + chk + S8_SLEN + S8_DLEN;
}

/******************************************************************************/

static int parse_header (const unsigned char *data, long n, unsigned *stubv,
                         unsigned *lit_src, long *outlen);

/******************************************************************************/

#  ifndef LZPACK_NO_AUTOARCH
static int
is_z80_image (const unsigned char *d, long n)
{
  long i = 0;

  while (i < n)
    {
      int op = d[i];

      if (op == 0xCB || op == 0xDD || op == 0xED || op == 0xFD)
        return 1;

      i += op8080_len[op];
    }

  return 0;
}

/******************************************************************************/
#  endif

static int
do_compress (const char *fn, const char *oname, int verbose, int use8080,
             int auto_stub, int optimal)
{
  unsigned char *data = g_a, *pl = g_b, *outf = g_c;
  long n, pllen, outlen, pl_dst_top, ming, total, body;
  long stub_dst_top, dcmp_dsttop;
  char nb[1024];

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  /*
   * CP/M-3 GENCOM RSX-attached images carry a one-page header whose first
   * byte is 0C9h (RET); refuse them rather than packing the header.
   */

  if (n > 0 && data[0] == 0xc9)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_GENCOM, fn);

      return 1;
    }

#  ifndef LZPACK_COMPRESS_ONLY
  {
    unsigned r_stubv = 0, r_litsrc = 0;
    long r_outlen = 0;

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
          /* Flawfinder: ignore */ /* False positive CWE-134 */
          (void)fprintf (stderr, MSG_P_REPACK, fn);
      }
  }
#  endif

  if (n > MZXFILE)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_MZX, fn, (long)MZXFILE);

      return 1;
    }

  if (n > 65535L)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_HDRBIG, fn);

      return 1;
    }

  if (n <= LITCNT + 32)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_SMALL, fn);

      return 1;
    }

#  ifndef LZPACK_NO_AUTOARCH
  if (auto_stub)
    use8080 = (is_z80_image (data, n) ? 0 : 1);
#  else
  (void)auto_stub;
#  endif

  prog_begin (verbose, n - LITCNT, fn);

#  ifdef LZPACK_NO_OPT
  if (optimal && verbose)
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)fprintf (stderr, MSG_P_NOEXTRA);

  pllen = compress (data, n, LITCNT, pl, 1024, LZ_STDBLK);
#  else
  pllen = (optimal ? compress (data, n, LITCNT, pl, 4096, n)
                   : compress (data, n, LITCNT, pl, 1024, LZ_STDBLK));
#  endif

  prog_done ();

  outlen = n;
  pl_dst_top = (long)(TPA + outlen) - 1;
  ming = min_gap (pl, pllen, outlen - LITCNT, LITCNT, pl_dst_top);

  if (ming < 1)
    pl_dst_top += (1 - ming);

  stub_dst_top = pl_dst_top + (Z80_HEADROOM + Z80_DCMP_LEN - 1);
  dcmp_dsttop = (pl_dst_top + 51) + S8_DLEN - 1;

  if (use8080 ? (dcmp_dsttop > (long)memtop) : (stub_dst_top > (long)memtop))
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_NOFIT,
                     fn, ((long)memtop + 513L) / 1024L, (long)memtop,
                     (use8080 ? dcmp_dsttop : stub_dst_top));

      return 1;
    }

  body = (use8080 ? build_8080 (outf, data, pllen, pl, outlen, pl_dst_top)
                  : build_z80 (outf, data, pllen, pl, outlen, pl_dst_top));

  total = body;

  if (total >= n)
    {
      if (verbose)
        /* Flawfinder: ignore */ /* False positive CWE-134 */
        (void)fprintf (stderr, MSG_P_SKIPPED,
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
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_WRITE, oname);

      return 1;
    }

  if (verbose)
    {
      long p10 = (total * 1000L + n / 2) / n; /* n > LITCNT + 32 here */
#  ifdef LZPACK_NO_AUTOARCH
      const char *amark = "";
#  else
      const char *amark = (auto_stub ? " auto" : "");
#  endif

      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_P_PACKED,
                     fn, n, total, p10 / 10, p10 % 10,
                     (use8080 ? "8080" : "Z80"), amark, oname);
    }

  return 0;
}

/******************************************************************************/

# else

/*
 * Streaming compressor for tiny (CP/M-80) hosts.  The input is read from
 * disk through a fixed sliding window and the payload is written to a temp
 * file, so working memory does not depend on file size and large executables
 * can be packed on a 64K machine.  Match choices may differ from the in-RAM
 * path, but the emitted format is identical and self-extracts the same way.
 *
 * The sliding window (window bytes + a 2-byte link per slot, = 3*WINSZ of
 * RAM) is allocated dynamically: at startup the largest power-of-two window
 * that fits in the available heap is chosen, from WIN_MAX down to WINMIN,
 * so the compressor uses as big a window -- and packs as tightly -- as the
 * host's TPA allows.
 *
 * The effective match distance is min(WINSZ - MAXLEN - 1, MAXDIST).
 *
 * WIN_MAX is not a tunable: a window larger than 2*MAXDIST cannot help,
 * since the format caps match distance at MAXDIST, so that is where the
 * probe starts.
 */

#  define LOOKAHEAD MAXLEN

#  ifndef WIN_MAX
#   define WIN_MAX (MAXDIST * 2)
#  endif

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
static lzpos s_winsz, s_wmask, s_maxback;
static LZF *s_in;
static lzpos s_N, s_loaded;
static lzpos s_win_start = WIN_MAX;
static lzpos s_win_reserve;

/******************************************************************************/

static int
win_alloc (void)
{
  for (s_winsz = s_win_start; s_winsz >= WINMIN; s_winsz >>= 1)
    {
      s_win = (unsigned char *)malloc ((size_t)s_winsz);
      s_lnk = (int *)malloc ((size_t)s_winsz * sizeof (int));

      if (s_win && s_lnk)
        {
          if (s_win_reserve)
            {
              /*
               * This window leaves enough heap for the DP block only if a
               * probe of the reserve succeeds; otherwise try a smaller one.
               */
              void *guard = malloc ((size_t)s_win_reserve);

              if (!guard)
                {
                  FREE (s_win);
                  FREE (s_lnk);

                  continue;
                }

              free (guard);
            }

          break;
        }

      FREE (s_win);
      FREE (s_lnk);
    }

  if (!s_win)
    return -1;

  s_wmask = s_winsz - 1;
  s_maxback = s_winsz - LOOKAHEAD - 1;

  if (s_maxback > (lzpos)MAXDIST)
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
win_load_ahead (lzpos pos)
{
  lzpos upto = (s_N - pos > (lzpos)(LOOKAHEAD + 1)) ? (pos + (LOOKAHEAD + 1))
                                                    : s_N;

  if (upto > s_N)
    upto = s_N;

  while (s_loaded < upto)
    {
      int c = lzgetc (s_in);

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
s_hash3 (lzpos i)
{
  return (int)((((unsigned)s_win[i & s_wmask] << 10)
                ^ ((unsigned)s_win[(i + 1) & s_wmask] << 5)
                ^ s_win[(i + 2) & s_wmask]) & (HSZ - 1));
}

/******************************************************************************/

static void
s_hinsert (lzpos i)
{
  int h;

  if (i >= s_N || s_N - i <= 2)
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

/******************************************************************************/

/*
 * Bounded-block parse-DP infrastructure, shared by the standard streaming
 * engine and the -E engine.  Both run the same cost-optimal shortest-path
 * parser over a sliding block of positions; they differ only in the block
 * size (and so the working memory): the standard engine uses a small block
 * that leaves the largest possible match window even on a 48K TPA, while -E
 * uses a large block for the tightest possible parse.
 */

#  ifndef LZPACK_NO_OPT
#   ifndef LZ_OPTBLK
#    define LZ_OPTBLK 2048 /* -E parse block; standard engine never uses it */
#   endif
#  endif

#  ifndef LZ_STDBLK_MIN
#   define LZ_STDBLK_MIN 128L
#  endif

#  ifdef LZ_POS16
#   define LZ_OCOST_MAX 0xFFFFU
#   if LZ_STDBLK > 7000
#    error "LZ_STDBLK too large for 16-bit lzcost (max 7000)"
#   endif
#   ifndef LZPACK_NO_OPT
#    if LZ_OPTBLK > 7000
#     error "LZ_OPTBLK too large for 16-bit lzcost (max 7000)"
#    endif
#   endif
#  else
#   define LZ_OCOST_MAX 0x3fffffffL
#  endif

/*
 * Heap to reserve below the match window for the parse-DP arrays: enough
 * for the smallest block both engines fall back to.  opt_alloc then grows
 * the block into any heap left beyond the window, so the window is never
 * shrunk by more than this minimum.
 */

#  ifndef LZ_STD_RESERVE
#   define LZ_STD_RESERVE \
  ((long)(LZ_STDBLK_MIN + 1) * (sizeof (lzcost) + 3 * sizeof (int)) + 512L)
#  endif

#  ifndef LZ_OPTDEPTH
#   define LZ_OPTDEPTH 1024
#  endif

#  ifndef LZ_OPT_ALLOC
#   define LZ_OPT_ALLOC(n) malloc ((n))
#  endif

#  ifndef LZ_OPT_FREE
#   define LZ_OPT_FREE(p) free ((p))
#  endif

#  define OFREE(p)     \
  do {                 \
    LZ_OPT_FREE ((p)); \
    (p) = NULL;        \
  } while (never)

static lzcost *o_cost;
static int *o_tlen;
static int *o_tdist;
static int *o_stk;
static lzpos o_blk;

static int o_l2d[MAXLEN + 1];

/*
 * Bit costs from match_bits() top out around 35 (1+16 header bits plus the
 * extension-length coding), so a byte per entry suffices and halves these
 * three tables; o_l2d above stays int because it stores distances.
 */

static unsigned char o_mb0[MAXLEN + 1];
static unsigned char o_mb1[MAXLEN + 1];
static unsigned char o_mb3[MAXLEN + 1];

#  define OMBITS(d, L) \
  ((d) <= 128 ? o_mb0[L] : (d) <= 1152 ? o_mb1[L] : o_mb3[L])

/******************************************************************************/

static void
opt_cost_tables (void)
{
  int L;

  for (L = 2; L <= MAXLEN; L++)
    {
      o_mb0[L] = (unsigned char)match_bits (1, L);
      o_mb1[L] = (unsigned char)match_bits (200, L);
    }

  for (L = 3; L <= MAXLEN; L++)
    o_mb3[L] = (unsigned char)match_bits (2000, L);
}

/******************************************************************************/

/*
 * Allocate the parse-DP arrays.  The block size is probed from `want' down
 * to `lo' (halving), so the largest block that fits the heap left over after
 * the window is used.  o_blk records the size actually obtained.
 */

static int
opt_alloc (lzpos want, lzpos lo)
{
  for (o_blk = want; o_blk >= lo; o_blk >>= 1)
    {
      size_t s = (size_t)(o_blk + 1);

      o_cost = (lzcost *)LZ_OPT_ALLOC (s * sizeof (lzcost));
      o_tlen = (int *)LZ_OPT_ALLOC (s * sizeof (int));
      o_tdist = (int *)LZ_OPT_ALLOC (s * sizeof (int));
      o_stk = (int *)LZ_OPT_ALLOC (s * sizeof (int));

      if (o_cost && o_tlen && o_tdist && o_stk)
        return 0;

      OFREE (o_cost);
      OFREE (o_tlen);
      OFREE (o_tdist);
      OFREE (o_stk);
    }

  return -1;
}

/******************************************************************************/

static void
opt_free (void)
{
  OFREE (o_cost);
  OFREE (o_tlen);
  OFREE (o_tdist);
  OFREE (o_stk);
}

/******************************************************************************/

static long
compress_stream (LZF *in, lzpos n, int start, LZF *out, int depth,
                 unsigned char *first16, const char *ptag)
{
  lzpos seg_start, apos, ins;
  lzpos k;

#  ifdef LZPACK_NO_PROGRESS
  (void)ptag; /* progress is compiled out; prog_show() ignores its tag */
#  endif

  s_in = in;
  s_N = n;
  s_loaded = 0;

  opt_cost_tables ();

  for (k = 0; k < (lzpos)HSZ; k++)
    head[k] = -1;

  e_init_stream (out);

  win_load_ahead (0);

  for (k = 0; k < LITCNT && k < n; k++)
    first16[k] = s_win[k & s_wmask];

  for (apos = 0; apos < (lzpos)start && n - apos > 2; apos++)
    {
      win_load_ahead (apos);
      s_hinsert (apos);
    }

  ins = (lzpos)start;

  for (seg_start = (lzpos)start; seg_start < n;)
    {
      /* seg_start + o_blk, subtract-first: a 16-bit lzpos cannot wrap */
      lzpos seg_end = (n - seg_start > o_blk) ? (seg_start + o_blk) : n;
      lzpos span, j;

      span = seg_end - seg_start;

      for (j = 0; j <= span; j++)
        {
          o_cost[j] = LZ_OCOST_MAX;
          o_tlen[j] = 0;
          o_tdist[j] = 0;
        }

      o_cost[0] = 0;

      for (apos = seg_start; apos < seg_end; apos++)
        {
          lzpos jc = apos - seg_start;
          int cap;

          while (ins < apos)
            {
              win_load_ahead (ins);
              s_hinsert (ins);
              ins++;
            }

          win_load_ahead (apos);

          if (o_cost[jc] + 9 < o_cost[jc + 1])
            {
              o_cost[jc + 1] = o_cost[jc] + 9;
              o_tlen[jc + 1] = 1;
              o_tdist[jc + 1] = s_win[apos & s_wmask];
            }

          cap = MAXLEN;

          if ((lzpos)cap > seg_end - apos)
            cap = (int)(seg_end - apos);

          if ((lzpos)cap > n - apos)
            cap = (int)(n - apos);

          if (cap >= 3 && n - apos > 2)
            {
              lzpos base = apos & ~s_wmask;
              int stored = head[s_hash3 (apos)];
              int dep = depth, maxml = 0;

              while (stored >= 0 && dep-- > 0)
                {
                  lzpos p = base | (lzpos)stored;
                  lzpos d;
                  int ml;

                  if (p > apos)
                    p -= s_winsz;

                  d = apos - p;

                  if (d == 0 || d > s_maxback)
                    break;

                  if (maxml > 0 && maxml < cap
                      && s_win[(p + (lzpos)maxml) & s_wmask]
                           != s_win[(apos + (lzpos)maxml) & s_wmask])
                    {
                      stored = s_lnk[p & s_wmask];

                      continue;
                    }

                  ml = 0;

                  while (ml < cap
                         && s_win[(p + (lzpos)ml) & s_wmask]
                              == s_win[(apos + (lzpos)ml) & s_wmask])
                    ml++;

                  if (ml > maxml)
                    {
                      int L;

                      for (L = maxml + 1; L <= ml; L++)
                        o_l2d[L] = (int)d;

                      maxml = ml;

                      if (maxml >= cap)
                        break;
                    }

                  stored = s_lnk[p & s_wmask];
                }

              {
                int L;

                for (L = 3; L <= maxml; L++)
                  {
                    int d = o_l2d[L];
                    lzpos jL = jc + (lzpos)L;
                    lzcost c2 = o_cost[jc] + OMBITS (d, L);

                    if (c2 < o_cost[jL])
                      {
                        o_cost[jL] = c2;
                        o_tlen[jL] = L;
                        o_tdist[jL] = d;
                      }
                  }
              }
            }

          if (cap >= 2 && n - apos > 1)
            {
              lzpos lo = (apos > 128) ? (apos - 128) : 0;
              lzpos p;

              for (p = apos; p-- > lo;)
                if (s_win[p & s_wmask] == s_win[apos & s_wmask]
                    && s_win[(p + 1) & s_wmask] == s_win[(apos + 1) & s_wmask])
                  {
                    int d2 = (int)(apos - p);
                    lzcost c2 = o_cost[jc] + OMBITS (d2, 2);

                    if (c2 < o_cost[jc + 2])
                      {
                        o_cost[jc + 2] = c2;
                        o_tlen[jc + 2] = 2;
                        o_tdist[jc + 2] = (int)(apos - p);
                      }

                    break;
                  }
            }
        }

      {
        int sp = 0, kk;

        for (kk = (int)span; kk > 0;)
          {
            o_stk[sp++] = kk;
            kk -= (o_tlen[kk] > 1 ? o_tlen[kk] : 1);
          }

        for (kk = sp - 1; kk >= 0; kk--)
          {
            int e = o_stk[kk];
            int L = o_tlen[e];

            if (L > 1)
              e_match (o_tdist[e], L);
            else
              e_lit (o_tdist[e]);
          }
      }

      while (ins < seg_end)
        {
          win_load_ahead (ins);
          s_hinsert (ins);
          ins++;
        }

      seg_start = seg_end;

      prog_show (ptag, (long)(seg_start - (lzpos)start));
    }

  obuf_flush ();

  return s_ototal;
}

/******************************************************************************/

static long
assemble_z80_stream (LZF *outf, const unsigned char *first16, lzpos pllen,
                     LZF *pl, lzpos outlen, lzpos pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  lzpos lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  lzpos stub_dst_top = pl_dst_top + (Z80_HEADROOM + Z80_DCMP_LEN - 1);
  unsigned char hdr[LITCNT], stub[STUBLEN], chkb[CHK_LEN];
  lzpos chk, k;

  (void)memcpy (hdr, first16, LITCNT);
  hdr[0] = 0xc3;
  put16 (hdr + 1, (unsigned)stub_v);
  (void)memcpy (hdr + 5, "-pc1-", 5);
  put16 (hdr + 10, (unsigned)outlen);
  hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0;
  (void)lzwrite (hdr, LITCNT, outf);

  for (k = 0; k < pllen; k++)
    {
      int c = lzgetc (pl);

      if (c == EOF)
        break;

      (void)lzputc (c, outf);
    }

  (void)lzwrite (first16, LITCNT, outf);

  chk = (lzpos)put_check (chkb, stub_v, stub_dst_top);

  if (chk)
    (void)lzwrite (chkb, (size_t)chk, outf);

  (void)memcpy (stub, z80_stub, STUBLEN);

  put16 (stub + P_LIT_SRC, (unsigned)lit_src);
  put16 (stub + P_STUB_SRCTOP, (unsigned)(stub_v + chk + (STUBLEN - 1)));
  put16 (stub + P_STUB_DSTTOP, (unsigned)stub_dst_top);
  put16 (stub + P_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (stub + P_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (stub + P_PL_LEN, (unsigned)pllen);
  put16 (stub + P_JP_RELOC, (unsigned)(stub_dst_top - (Z80_DCMP_LEN - 1)));

  stub[P_CP_HI] = (unsigned char)((out_end >> 8) & 0xff);
  stub[P_CP_LO] = (unsigned char)(out_end & 0xff);

  {
    /* GETBIT is CALLed; retarget call operands to the relocated routine */
    lzpos getbit_v = stub_dst_top - (Z80_DCMP_LEN - 1) + Z80_GETBIT_OFF;
    int gi;

    for (gi = 0; gi < Z80_GETBIT_FIX_N; gi++)
      put16 (stub + z80_getbit_fix[gi], (unsigned)getbit_v);
  }

  (void)lzwrite (stub, STUBLEN, outf);

  return (long)(pllen + chk + (lzpos)(LITCNT + LITCNT + STUBLEN));
}

/******************************************************************************/

static long
assemble_8080_stream (LZF *outf, const unsigned char *first16, lzpos pllen,
                      LZF *pl, lzpos outlen, lzpos pl_dst_top)
{
  unsigned out_end = (unsigned)(TPA + outlen);
  lzpos lit_src = TPA + LITCNT + pllen, stub_v = lit_src + LITCNT;
  lzpos decomp_file_v;
  lzpos stub_run = pl_dst_top + 51;
  lzpos dcmp_dsttop = stub_run + S8_DLEN - 1;
  unsigned char hdr[LITCNT], su[S8_SLEN], de[S8_DLEN], chkb[CHK_LEN];
  lzpos chk, k;
  int i;

  (void)memcpy (hdr, first16, LITCNT);
  hdr[0] = 0xc3;
  put16 (hdr + 1, (unsigned)stub_v);
  (void)memcpy (hdr + 5, "-pc1-", 5);
  put16 (hdr + 10, (unsigned)outlen);
  hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0;
  (void)lzwrite (hdr, LITCNT, outf);

  for (k = 0; k < pllen; k++)
    {
      int c = lzgetc (pl);

      if (c == EOF)
        break;

      (void)lzputc (c, outf);
    }

  (void)lzwrite (first16, LITCNT, outf);

  chk = (lzpos)put_check (chkb, stub_v, dcmp_dsttop);

  if (chk)
    (void)lzwrite (chkb, (size_t)chk, outf);

  decomp_file_v = stub_v + chk + S8_SLEN;

  (void)memcpy (su, setup8080, S8_SLEN);
  (void)memcpy (de, decomp8080, S8_DLEN);

  for (i = 0; i < SETUP8080_FIX_N; i++)
    put16 (su + setup8080_fix[i][0],
           (unsigned)(stub_v + chk + setup8080_fix[i][1]));

  put16 (su + S8S_LIT_SRC, (unsigned)lit_src);
  put16 (su + S8S_DCMP_SRCTOP, (unsigned)(decomp_file_v + S8_DLEN - 1));
  put16 (su + S8S_DCMP_DSTTOP, (unsigned)dcmp_dsttop);
  su[S8S_DCMP_LEN] = (unsigned char)S8_DLEN; /* 8-bit reloc count; see guard */
  put16 (su + S8S_DCMP_RUN, (unsigned)stub_run);

  for (i = 0; i < DECOMP8080_FIX_N; i++)
    put16 (de + decomp8080_fix[i][0],
           (unsigned)(stub_run + decomp8080_fix[i][1]));

  de[S8D_OUT_END_HI] = (unsigned char)((out_end >> 8) & 0xff);
  de[S8D_OUT_END_LO] = (unsigned char)(out_end & 0xff);
  put16 (de + S8D_PL_SRCTOP, (unsigned)(lit_src - 1));
  put16 (de + S8D_PL_DSTTOP, (unsigned)pl_dst_top);
  put16 (de + S8D_PL_LEN, (unsigned)pllen);

  (void)lzwrite (su, S8_SLEN, outf);
  (void)lzwrite (de, S8_DLEN, outf);

  return (long)(pllen + chk + (lzpos)(LITCNT + LITCNT + S8_SLEN + S8_DLEN));
}

/******************************************************************************/

#  ifndef LZPACK_NO_AUTOARCH

static int
is_z80_file (LZF *f, lzpos n)
{
  lzpos rem = n;

  while (rem)
    {
      int op = lzgetc (f);
      int skip;

      if (op == EOF)
        return 0;

      rem--;

      if (op == 0xCB || op == 0xDD || op == 0xED || op == 0xFD)
        return 1;

      for (skip = op8080_len[op] - 1; skip > 0; skip--)
        {
          if (lzgetc (f) == EOF)
            return 0;

          if (rem)
            rem--;
        }
    }

  return 0;
}

/******************************************************************************/
#  endif

static int
do_compress_stream (const char *fn, const char *oname, int verbose,
                    int use8080, int auto_stub, int optimal)
{
  LZF *in, *tmp, *outf;
  long n, pllen, outlen, pl_dst_top, ming, total, body;
  long stub_dst_top, dcmp_dsttop;
  lzpos dp_blk = (lzpos)LZ_STDBLK; /* parse-DP block (LZ_OPTBLK for -E) */
  const char *dp_tag = ""; /* progress tag, "-E" for the -E engine */
  unsigned char first16[LITCNT];
  char nb[64];

  /* cppcheck-suppress variableScope */
  const char *oom = MSG_E_WINMEM;

  n = count_file (fn);

  if (n < 0)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  if (n >= 16)
    {
      unsigned rsv = 0, rls = 0;
      long rol = 0;

      in = lzopen (fn, 0);

      if (in)
        {
          unsigned char hdr[LITCNT];
          long k;

          k = (long)lzread (hdr, LITCNT, in);
          (void)lzclose (in);

          /* CP/M-3 GENCOM header record: first byte is 0C9h (RET). */
          if (k >= 1 && hdr[0] == 0xc9)
            {
              /* Flawfinder: ignore */ /* False positive CWE-134 */
              (void)fprintf (stderr, MSG_E_GENCOM, fn);

              return 1;
            }

          if (k == LITCNT && parse_header (hdr, n, &rsv, &rls, &rol) == 0)
            {
              /* Flawfinder: ignore */ /* False positive CWE-134 */
              (void)fprintf (stderr, MSG_E_PACKED,
                             fn);

              return 1;
            }
        }
    }

  if (n > MZXFILE)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_MZX, fn, (long)MZXFILE);

      return 1;
    }

  if (n > 65535L)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_HDRBIG, fn);

      return 1;
    }

  if (n <= LITCNT + 32)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_SMALL, fn);

      return 1;
    }

#  ifdef LZPACK_NO_OPT
  if (optimal && verbose)
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)fprintf (stderr, MSG_P_NOEXTRA);

#  else
  if (optimal)
    {
      dp_blk = LZ_OPTBLK; /* -E: grow the parse block into spare heap */
      dp_tag = "-E";
    }
#  endif

#  ifndef LZPACK_NO_AUTOARCH
  if (auto_stub)
    {
      LZF *df = lzopen (fn, 0);

      if (df)
        {
          use8080 = (is_z80_file (df, (lzpos)n) ? 0 : 1);
          (void)lzclose (df);
        }
    }
#  else
  (void)auto_stub;
#  endif

  in = lzopen (fn, 0);

  if (!in)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  tmp = lzopen (LZTMP, 1);

  if (!tmp)
    {
      (void)lzclose (in);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_TMPMAKE, LZTMP);

      return 1;
    }

  /*
   * Window first: both engines reserve only enough heap for the smallest DP
   * block, so the match window is made as large as the TPA allows (a bigger
   * window helps far more than a bigger parse block).  opt_alloc then grabs
   * whatever heap is left over for the block -- up to LZ_OPTBLK for -E, which
   * is the only thing that distinguishes the two engines' memory use.
   */

  s_win_start = WIN_MAX;
  s_win_reserve = (lzpos)LZ_STD_RESERVE;

  if (win_alloc ())
    {
      (void)lzclose (in);
      (void)lzclose (tmp);
      lzunlink (LZTMP);
      LZ_MPUTS (stderr, oom);

      return 1;
    }

  if (opt_alloc (dp_blk, (lzpos)LZ_STDBLK_MIN))
    {
      win_free ();
      (void)lzclose (in);
      (void)lzclose (tmp);
      lzunlink (LZTMP);
      LZ_MPUTS (stderr, oom);

      return 1;
    }

  if (verbose)
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)fprintf (stderr, MSG_P_WINDOW,
                   fn, (long)s_winsz, (long)s_maxback);

  prog_begin (verbose, n - LITCNT, fn);

  pllen = compress_stream (in, (lzpos)n, LITCNT, tmp, LZ_OPTDEPTH, first16,
                           dp_tag);

  opt_free ();

  prog_done ();

  win_free (); /* release the window before reopening files */
  (void)lzclose (in);
  (void)lzclose (tmp);

  outlen = n;
  pl_dst_top = (long)(TPA + outlen) - 1;

  /* CP/M stdio cannot reliably read a file back through "w+b"; reopen "rb". */

  tmp = lzopen (LZTMP, 0);

  if (!tmp)
    {
      lzunlink (LZTMP);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_TMPOPEN, LZTMP);

      return 1;
    }

  ming = min_gap_stream (tmp, pllen, outlen - LITCNT, LITCNT, pl_dst_top);
  (void)lzclose (tmp);

  if (ming < 1)
    pl_dst_top += (1 - ming);

  stub_dst_top = pl_dst_top + (Z80_HEADROOM + Z80_DCMP_LEN - 1);
  dcmp_dsttop = (pl_dst_top + 51) + S8_DLEN - 1;

  if (use8080 ? (dcmp_dsttop > (long)memtop) : (stub_dst_top > (long)memtop))
    {
      lzunlink (LZTMP);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_NOFIT,
                     fn, ((long)memtop + 513L) / 1024L, (long)memtop,
                     (use8080 ? dcmp_dsttop : stub_dst_top));

      return 1;
    }

  body = (use8080 ? (LITCNT + pllen + LITCNT + S8_SLEN + S8_DLEN)
                  : (LITCNT + pllen + LITCNT + STUBLEN))
         + (opt_checked ? CHK_LEN : 0);

  total = body;

  if (total >= n)
    {
      if (verbose)
        /* Flawfinder: ignore */ /* False positive CWE-134 */
        (void)fprintf (stderr, MSG_P_SKIPPED,
                       fn, n, total);

      lzunlink (LZTMP);

      return 2;
    }

  if (!oname)
    {
      mkname (fn, ".pop", nb, sizeof (nb));
      oname = nb;
    }

  outf = lzopen (oname, 1);

  if (!outf)
    {
      lzunlink (LZTMP);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_WRITE, oname);

      return 1;
    }

  tmp = lzopen (LZTMP, 0);

  if (!tmp)
    {
      (void)lzclose (outf);
      lzunlink (LZTMP);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_TMPOPEN, LZTMP);

      return 1;
    }

  if (use8080)
    (void)assemble_8080_stream (outf, first16, (lzpos)pllen, tmp,
                                (lzpos)outlen, (lzpos)pl_dst_top);
  else
    (void)assemble_z80_stream (outf, first16, (lzpos)pllen, tmp,
                               (lzpos)outlen, (lzpos)pl_dst_top);

  (void)lzclose (outf);
  (void)lzclose (tmp);
  lzunlink (LZTMP);

#  ifdef LZ_CPM
  (void)cpm_set_byte_count (oname, total);
#  endif

  if (verbose)
    {
      long p10 = (total * 1000L + n / 2) / n; /* n > LITCNT + 32 here */
#  ifdef LZPACK_NO_AUTOARCH
      const char *amark = "";
#  else
      const char *amark = (auto_stub ? " auto" : "");
#  endif

      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_P_PACKED, fn,
                     n, total, p10 / 10, p10 % 10, (use8080 ? "8080" : "Z80"),
                     amark, oname);
    }

  return 0;
}

# endif
#endif

/******************************************************************************/

/*
 * The three outputs are written only on the success (0) return, and every
 * caller reads them only then -- but older flow analyzers cannot see
 * through the out-parameters (Open64 4.5 warned "might be used
 * uninitialized" at each call site), so callers zero-initialize the
 * receiving locals at declaration.  Keep that convention at new call
 * sites.
 */

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

#ifndef LZPACK_COMPRESS_ONLY
# ifndef LZPACK_STREAM
static int
do_restore (const char *fn, const char *oname, int verbose)
{
  unsigned char *data = g_a, *out = g_b;
  long n, outlen = 0, pstart;
  unsigned stubv = 0, lit_src = 0;
  char nb[1024];

  n = readfile (fn, data, (size_t)BUFSZ);

  if (n < 0)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  if (n > BUFSZ)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_RESTBIG, fn);

      return 1;
    }

  if (parse_header (data, n, &stubv, &lit_src, &outlen))
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_NOTPOP, fn);

      return 1;
    }

  pstart = TPA + LITCNT - TPA;

  if (outlen > MZXFILE)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_EXPMZX, fn,
               (long)MZXFILE);

      return 1;
    }

  if ((long)lit_src - TPA < 0 ||
      (long)lit_src - TPA + LITCNT > n || outlen < LITCNT)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_BADHDR, fn);

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
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_WRITE, oname);

      return 1;
    }

  if (verbose)
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)fprintf (stderr, MSG_P_RESTORED,
                   fn, n, outlen, oname);

  return 0;
}

# else

/******************************************************************************/

static size_t
fread_full (void *p, size_t n, LZF *f)
{
  unsigned char *d = (unsigned char *)p;
  size_t got = 0;
  int c;

  while (got < n && (c = lzgetc (f)) != EOF)
    d[got++] = (unsigned char)c;

  return got;
}

/******************************************************************************/

static int
do_restore (const char *fn, const char *oname, int verbose)
{
  unsigned char *buf;
  unsigned char hdr[LITCNT], lit[LITCNT];
  long n, outlen = 0, pllen, ming, bufsz, srcoff;
  unsigned stubv = 0, lit_src = 0;
  char nb[64];
  LZF *f;

  n = count_file (fn);

  if (n < 0)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  /*
   * Read the 16-byte header, then scan the payload for the overlap gap.  The
   * header read leaves the file positioned at the payload, so the gap scan
   * needs no reopen (and no seek -- CP/M stdio cannot rewind reliably).
   */

  f = lzopen (fn, 0);

  if (!f)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  if (lzread (hdr, (size_t)LITCNT, f) != (size_t)LITCNT
      || parse_header (hdr, n, &stubv, &lit_src, &outlen))
    {
      (void)lzclose (f);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_NOTPOP, fn);

      return 1;
    }

  if (outlen > MZXFILE)
    {
      (void)lzclose (f);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_EXPMZX, fn,
               (long)MZXFILE);

      return 1;
    }

  if ((long)lit_src - TPA < LITCNT
      || (long)lit_src - TPA + LITCNT > n || outlen < LITCNT)
    {
      (void)lzclose (f);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_BADHDR, fn);

      return 1;
    }

  pllen = ((long)lit_src - TPA) - LITCNT;
  ming = min_gap_stream (f, pllen, outlen - LITCNT, LITCNT,
                         (long)(TPA + outlen) - 1);
  (void)lzclose (f);

  bufsz = outlen + (ming < 1 ? (1 - ming) : 0);

  if (ming < outlen + 1 - LONG_MAX)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_BADHDR, fn);

      return 1;
    }

  buf = (unsigned char *)malloc ((size_t)bufsz);

  if (!buf)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_RESTOOM, fn);

      return 1;
    }

  srcoff = bufsz - pllen;
  f = lzopen (fn, 0);

  if (!f || fread_full (hdr, (size_t)LITCNT, f) != (size_t)LITCNT
         || fread_full (buf + srcoff, (size_t)pllen, f) != (size_t)pllen
         || fread_full (lit, (size_t)LITCNT, f) != (size_t)LITCNT)
    {
      if (f)
        (void)lzclose (f);

      FREE (buf);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  (void)lzclose (f);

#  ifdef LZ_ASM_RESTORE
  {
    /* Reuse the self-extracting decompressor for -R: copy CALL-able core */

    static unsigned char rcore[LZ_RCORE_LEN];
    unsigned cbase, sv, dv, oe;
    int i;

    (void)memcpy (buf, lit, (size_t)LITCNT); /* seed the 16 literal bytes */
    (void)memcpy (rcore, LZ_RCORE, (size_t)LZ_RCORE_LEN);

    cbase = (unsigned)rcore;

    for (i = 0; i < LZ_RCORE_FIX_N; i++)
      {
        unsigned t = cbase + (unsigned)LZ_RCORE_FIX[i][1];

        rcore[LZ_RCORE_FIX[i][0]] = (unsigned char)(t & 0xff);
        rcore[LZ_RCORE_FIX[i][0] + 1] = (unsigned char)((t >> 8) & 0xff);
      }

    sv = (unsigned)(buf + srcoff) - 1;
    dv = (unsigned)(buf + LITCNT);
    oe = (unsigned)(buf + outlen);

    rcore[LZ_RCORE_SRCV] = (unsigned char)(sv & 0xff);
    rcore[LZ_RCORE_SRCV + 1] = (unsigned char)((sv >> 8) & 0xff);
    rcore[LZ_RCORE_DSTV] = (unsigned char)(dv & 0xff);
    rcore[LZ_RCORE_DSTV + 1] = (unsigned char)((dv >> 8) & 0xff);
    rcore[LZ_RCORE_OEHI] = (unsigned char)((oe >> 8) & 0xff);
    rcore[LZ_RCORE_OELO] = (unsigned char)(oe & 0xff);

    ((void (*) (void)) rcore) ();
  }
#  else
  (void)decode (buf + srcoff, pllen, buf, outlen, LITCNT, lit);
#  endif

  if (!oname)
    {
      mkname (fn, ".unp", nb, sizeof (nb));
      oname = nb;
    }

  if (writefile (oname, buf, outlen))
    {
      FREE (buf);
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_WRITE, oname);

      return 1;
    }

  if (verbose)
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)fprintf (stderr, MSG_P_RESTORED, fn, n, outlen,
                   oname);

  FREE (buf);

  return 0;
}
# endif
#endif

/******************************************************************************/

#ifndef LZPACK_COMPRESS_ONLY

/*
 * True for the per-file patch slots in the -C check block: the two limit
 * words and the three +stub_v fixup operands.  Every other byte of a
 * genuine block must equal the chkstub[] template byte.
 */

static int
chk_patch_slot (int k)
{
  int i;

  if (k == CHK_DST_LIM || k == CHK_DST_LIM + 1
      || k == CHK_SP_LIM || k == CHK_SP_LIM + 1)
    return 1;

  for (i = 0; i < CHKSTUB_FIX_N; i++)
    if (k == (int)chkstub_fix[i][0] || k == (int)chkstub_fix[i][0] + 1)
      return 1;

  return 0;
}

#endif

/******************************************************************************/

/*
 * The -C check-block recognizer and stub-architecture tag below are
 * compiled out of compress-only builds: on the split CP/M-80 pair listing
 * is LZUNPACK's job, and every byte of the packer's static footprint
 * competes with its match window (the Z80 packer's
 * 8K-window-at-52,978-bytes TPA floor depends on it).
 */

static int
do_list (const char *fn)
{
  unsigned char hdr[LITCNT];
#ifndef LZPACK_COMPRESS_ONLY
  static unsigned char chkb[CHK_LEN + 7];
  const char *tag = "";
  long chk_off = -1;
  int chk_got = 0;
  int ischk = 0;
#endif
  long n, outlen = 0;
  unsigned stubv = 0, lit_src = 0;
  size_t got;
  LZF *f = lzopen (fn, 0);

  if (!f)
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_READ, fn);

      return 1;
    }

  /*
   * Only the 16-byte header is parsed; the rest is read solely to size the
   * file, so listing never needs a whole-file buffer.  Count only after a
   * full header was read, and stop on the first short read, so fread is
   * never issued on a stream already at EOF or in error.
   *
   * The sizing pass doubles as the -C check-block capture: the JMP target
   * in the header names the stub address, so the CHK_LEN + 7 bytes at that
   * file offset -- the check block, when the file carries one, plus the
   * start of the setup block behind it (the stub-architecture tag below
   * reads its prologue) -- are copied out of the passing 128-byte chunks.
   * Nothing here trusts the offset: bytes are only captured where the
   * chunks actually cover them, and the window is validated against the
   * chkstub[] template only after parse_header() accepts the file, so a
   * foreign or corrupted input at worst reports no floor and no tag.
   */

  got = lzread (hdr, (size_t)LITCNT, f);
  n = (long)got;

  if (got == (size_t)LITCNT)
    {
      unsigned char buf[128];

#ifndef LZPACK_COMPRESS_ONLY
      if (hdr[0] == 0xc3)
        chk_off = (long)get16 (hdr + 1) - TPA;
      else if (hdr[0] == 0x18 && hdr[2] == 0xc3)
        chk_off = (long)get16 (hdr + 3) - TPA;

      if (chk_off < LITCNT) /* would overlap the header: not a -C stub */
        chk_off = -1;
#endif

      for (;;)
        {
          size_t r = lzread (buf, sizeof (buf), f);

#ifndef LZPACK_COMPRESS_ONLY
          if (chk_off >= 0)
            {
              long lo = (chk_off > n) ? chk_off : n;
              long hi = chk_off + (long)(CHK_LEN + 7);

              if (hi > n + (long)r)
                hi = n + (long)r;

              for (; lo < hi; lo++, chk_got++)
                chkb[lo - chk_off] = buf[lo - n];
            }
#endif

          n += (long)r;

          if (r < sizeof (buf))
            break;
        }
    }

  (void)lzclose (f);

#ifdef LZ_CPM
  {
    long exact = cpm_file_size (fn);

    if (exact > 0 && exact <= n && n - exact < 128)
      n = exact;
  }
#endif

  if (got < (size_t)LITCNT || parse_header (hdr, n, &stubv, &lit_src, &outlen))
    {
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)printf (MSG_P_LNOTPOP, fn);

      return 0;
    }

#ifndef LZPACK_COMPRESS_ONLY

  /*
   * Recognize the -C runtime memory check, and the memory floor embedded
   * in it when the file carries one.  The captured window counts as a
   * check block only when every byte was read from inside the
   * (LRBC-corrected) file, every template byte matches chkstub[], and all
   * three +stub_v fixup operands equal the values put_check() would have
   * patched for this stub address; DST_LIM then holds the enforced floor
   * + 1.
   */

  if (chk_got >= CHK_LEN && chk_off + (long)CHK_LEN <= n)
    {
      int k;

      ischk = 1;

      for (k = 0; k < CHK_LEN; k++)
        if (chkb[k] != chkstub[k] && !chk_patch_slot (k))
          {
            ischk = 0;

            break;
          }

      if (ischk)
        for (k = 0; k < CHKSTUB_FIX_N; k++)
          if (get16 (chkb + chkstub_fix[k][0])
              != ((stubv + (unsigned)chkstub_fix[k][1]) & 0xFFFFU))
            {
              ischk = 0;

              break;
            }
    }

  /*
   * Stub-architecture tag for the size line.  The setup block -- at the
   * stub address, behind the check block when the file carries one --
   * opens with the same "restore the 16 literal header bytes" prologue on
   * both CPUs, 21 ll hh (LXI H/LD HL, LIT_SRC) 11 00 01 (LXI D/LD DE,
   * 0100h), and diverges on the seventh byte's counter load: 01 (Z80
   * LD BC,16 in sz80s.asm) vs 06 (8080 MVI B,16 in s8080s.asm).  The tag
   * is claimed only when that prologue matches byte-for-byte AND its
   * LIT_SRC operand equals the header-derived literal-save address, so
   * old, foreign, or future stub layouts simply list untagged (the
   * harness pins both encodings).
   */

  {
    int sbase = ischk ? CHK_LEN : 0;

    if (chk_got >= sbase + 7 && chk_off + (long)sbase + 7 <= n)
      {
        const unsigned char *sp = chkb + sbase;

        if (sp[0] == 0x21 && get16 (sp + 1) == lit_src
            && sp[3] == 0x11 && sp[4] == 0x00 && sp[5] == 0x01)
          {
            if (sp[6] == 0x01)
              tag = "  [Z80]";
            else if (sp[6] == 0x06)
              tag = "  [8080]";
          }
      }
  }

#endif

  {
    long p10 = (outlen ? (n * 1000L + outlen / 2) / outlen : 0);

#ifndef LZPACK_COMPRESS_ONLY
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_P_LSIZES,
                  fn, n, outlen, p10 / 10, p10 % 10, tag);
#else
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_P_LSIZES,
                  fn, n, outlen, p10 / 10, p10 % 10);
#endif
  }

#ifndef LZPACK_COMPRESS_ONLY

  if (ischk)
    {
      unsigned dlim = get16 (chkb + CHK_DST_LIM);

      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)printf (MSG_P_LFLOOR0, fn, (dlim - 1U) & 0xFFFFU);
    }
  else
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_P_LNOCHK, fn);

#endif

  return 0;
}

/******************************************************************************/

static void
herald (FILE *f)
{
  /* Flawfinder: ignore */ /* False positive CWE-134 */
  (void)fprintf (f, MSG_V_HERALD1, LZ_PROG, LZPACK_VER, LZ_KIND);
  LZ_MPUTS (f, MSG_V_COPYR);
}

/******************************************************************************/

#ifndef LZPACK_DECODE_ONLY

/*
 * -M argument: a bare number up to 64 (optionally with a trailing K) is a
 * memory size in KB, mapped so that 48 gives the built-in 0xBDFF default
 * (K*1024 - 0x201); a larger bare number or a 0x-prefixed hex value is the
 * literal MEMTOP address.  CP/M's CCP upper-cases the command tail, so
 * everything is accepted in either case.  Pure 16-bit unsigned arithmetic:
 * the single-compare guards stop any accumulation past 0xFFFF except
 * 65536..65539, which wrap to 0..3 and are then caught by the KB-form
 * floor, so no accepted parse can exceed 16 bits and the 0xFFFF ceiling is
 * structural.  Returns 0 (never a valid MEMTOP) if the value is malformed
 * or outside [MEMTOP_MIN, 0xFFFF].
 */

static unsigned
parse_memtop (const char *s)
{
  /*
   * function static direct addressing is far smaller than IX-relative
   * stack locals under sccz80, and a CLI parser needs no reentrancy
   */

  static unsigned v;

  v = 0;

  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
      for (s += 2;; s++)
        {
          static unsigned char c;

          c = (unsigned char)(*s | 0x20);

          if (c >= '0' && c <= '9')
            c = (unsigned char)(c - '0');
          else if (c >= 'a' && c <= 'f')
            c = (unsigned char)(c - 'a' + 10);
          else
            break;

          if (v > 0x0FFFU) /* another digit cannot fit in 16 bits */
            return 0;

          v = (v << 4) + c;
        }
    }
  else
    {
      for (; *s >= '0' && *s <= '9'; s++)
        {
          if (v > 6553U) /* v*10 cannot fit in 16 bits */
            return 0;

          v = v * 10 + (unsigned char)(*s - '0');
        }

# if UINT_MAX > 0xFFFFU
      /* On 16-bit ints 65536..65539 wrap to 0..3 and the KB-form floor
       * rejects them below; wider ints must reject the range explicitly. */
      /* cppcheck-suppress compareValueOutOfTypeRangeError */
      if (v > 0xFFFFU)
        return 0;
# endif

      if (*s == 'k' || *s == 'K') /* trailing K: the KB form only */
        {
          s++;

          if (v > 64U)
            return 0;
        }

      if (v <= 64U) /* KB form: K*1024 - 0x201, so 48 -> 0xBDFF */
        {
          if (v < 5U)
            return 0;

          v = v * 1024U - 0x201U;
        }
    }

  if (*s || v < MEMTOP_MIN)
    return 0;

  return v;
}

#endif

/******************************************************************************/

static void
usage (void)
{
  /*
   * The conditional lines are hoisted into plain variables BEFORE the
   * LZ_MPUTS calls: with LZ_BDOS_IO, a preprocessing directive inside a
   * macro argument list is undefined behavior (V1119).  The -E and no-E
   * compress lines are whole-message variants in the catalog (selected by
   * LZPACK_NO_OPT), so no %s splicing is needed here.
   */

  const char *lrbc = MSG_NONE;
#ifndef LZPACK_DECODE_ONLY
  const char *rline;
#endif

  herald (stderr);

#ifdef LZPACK_DECODE_ONLY

# ifdef LZ_CPM
  if (cpm_has_lrbc ())
    lrbc = MSG_U_UILINE;
# endif

  /* LZUNPACK: a bare <file> restores; -R is still accepted for symmetry. */
  LZ_MPUTS (stderr, MSG_U_HDR);
  LZ_MPUTS (stderr, MSG_U_UBARE);
  LZ_MPUTS (stderr, MSG_U_UO);
  LZ_MPUTS (stderr, lrbc);
  LZ_MPUTS (stderr, MSG_U_UVLINE);

#else

# ifdef LZ_CPM
  if (cpm_has_lrbc ())
    lrbc = MSG_U_ILINE;
# endif

# ifdef LZPACK_COMPRESS_ONLY
  rline = MSG_NONE;
# else
  rline = MSG_U_RLINE;
# endif

  LZ_MPUTS (stderr, MSG_U_HDR);
  LZ_MPUTS (stderr, MSG_U_PACKLN);
  LZ_MPUTS (stderr, rline);
  LZ_MPUTS (stderr, MSG_U_LO);
  LZ_MPUTS (stderr, MSG_U_MC);
# ifndef LZPACK_COMPRESS_ONLY
  LZ_MPUTS (stderr, MSG_U_FLINE);
# endif
  LZ_MPUTS (stderr, lrbc);
  LZ_MPUTS (stderr, MSG_U_VLINE);

#endif
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

    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_V_CPM, os, (ver >> 4) & 0xf, ver & 0xf, ver, sys);
    LZ_MPUTS (stdout, ((ver >= 0x30) ? MSG_V_LRBC1 : MSG_V_LRBC0));
  }

  {
    unsigned tk;
    const char *cpu;
    const char *memword;
# ifndef LZPACK_DECODE_ONLY
#  ifdef LZPACK_NO_OPT
    const char *eopt = "unavailable";
#  else
    const char *eopt = "available";
#  endif
# endif

# ifdef __AZTEC_C_42T__
    /*
     * CP/M-86 (Aztec): BDOS 53 (Get Max Mem) reports the largest free memory
     * region.  Request more than can exist (0xFFFF paragraphs) so it returns
     * the maximum available rather than allocating; the length comes back in
     * the MCB's second word, in 16-byte paragraphs.  Clamp to the 64K data
     * segment -- a small-model program cannot use more than that regardless of
     * how much the system reports free.
     */
    {
      unsigned mcb[3];

      mcb[1] = 0xFFFFU;
      (void)bdos (53, BDOS_FCB (mcb));
      tk = mcb[1] / 64U;
    }

    if (tk > 64U)
      tk = 64U;

    cpu = "Intel x86";
    memword = "memory";
# else
    /* CP/M-80 (z88dk): the BDOS entry word at 0x0006 (from the JMP at 0x0005)
     * tops the flat TPA, which starts at TPA (0x100). */
    /* cppcheck-suppress intToPointerCast */
    tk = (unsigned)((*(unsigned *)6 - (unsigned)TPA) / 1024U);
    memword = "TPA";
#  ifdef LZPACK_8080
    cpu = "Intel 8080";
#  else
    cpu = "Zilog Z80";
#  endif
# endif

# ifdef LZPACK_DECODE_ONLY
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_V_BUILDD, cpu, tk, memword);
# else
    /* Flawfinder: ignore */ /* False positive CWE-134 */
    (void)printf (MSG_V_BUILDC, cpu, eopt, tk, memword);

#  ifdef LZPACK_NO_AUTOARCH
    {
#   ifdef LZPACK_8080
      static const char *defstub = "8080";
#   else
      static const char *defstub = "Z80";
#   endif

      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)printf (MSG_V_AUTO0, defstub);
    }
#  else
    LZ_MPUTS (stdout, MSG_V_AUTO1);
#  endif
# endif
  }
#endif

  LZ_MPUTS (stdout, MSG_V_HOME);
}

/******************************************************************************/

#if LZ_INT_WILDCARD
# ifndef LZPACK_DECODE_ONLY
#  ifdef LZPACK_STREAM
#   define LZ_RUN_COMPRESS(FN) \
     do_compress_stream ((FN), oname, 1, use8080, auto_stub, optimal)
#  else
#   define LZ_RUN_COMPRESS(FN) \
     do_compress ((FN), oname, 1, use8080, auto_stub, optimal)
#  endif
# endif

/******************************************************************************/

# ifdef LZPACK_COMPRESS_ONLY
/* Flawfinder: ignore */ /* False positive CWE-134 */
#  define LZ_RUN_RESTORE(FN) ((void)fprintf (stderr, MSG_E_NOREST), 1)
# else
#  define LZ_RUN_RESTORE(FN) do_restore ((FN), oname, 1)
# endif

/******************************************************************************/

# ifndef LZPACK_DECODE_ONLY
#  define LZ_RUN_ONE(FN) \
     ((mode == 0) ? LZ_RUN_COMPRESS (FN) \
      : (mode == 1) ? LZ_RUN_RESTORE (FN) : do_list (FN))
# else
#  define LZ_RUN_ONE(FN) ((mode == 1) ? LZ_RUN_RESTORE (FN) : do_list (FN))
# endif
#endif

/******************************************************************************/

#if LZ_INT_WILDCARD

static int
is_wildcard (const char *s)
{
  int c;

  while ((c = *s++) != '\0')
    if (c == '*' || c == '?')
      return 1;

  return 0;
}
#endif

/******************************************************************************/

#if defined(LZ_CPM) && LZPACK_WILDCARD

static unsigned char g_dma[128];
static char g_wname[13];

/******************************************************************************/

static char *
fcb_to_name (const unsigned char *e)
{
  char *d = g_wname;
  int i;

  for (i = 1; i <= 11; i++)
    {
      int c = e[i] & 0x7f;

      if (i == 9 && (e[9] & 0x7f) != ' ')
        *d++ = '.';

      if (c != ' ')
        *d++ = (char)c;
    }

  *d = '\0';

  return g_wname;
}
#endif

/******************************************************************************/

#if LZ_WIN_WILDCARD

# define LZ_FILE_ATTR_DIRECTORY 0x10
# define LZ_INVALID_HANDLE ((void *)-1) /* INVALID_HANDLE_VALUE */

/******************************************************************************/

/* cppcheck-suppress-begin unusedStructMember */
typedef struct
{
  unsigned long dwFileAttributes;
  unsigned long ftCreation[2];
  unsigned long ftLastAccess[2];
  unsigned long ftLastWrite[2];
  unsigned long nFileSizeHigh;
  unsigned long nFileSizeLow;
  unsigned long dwReserved0;
  unsigned long dwReserved1;
  char cFileName[260];
  char cAlternateFileName[14];
} LZ_FINDDATA;
/* cppcheck-suppress-end unusedStructMember */

/******************************************************************************/

extern void *__stdcall FindFirstFileA (const char *, LZ_FINDDATA *);
extern int __stdcall FindNextFileA (void *, LZ_FINDDATA *);
extern int __stdcall FindClose (void *);
#endif

/******************************************************************************/

int
main (int argc, char **argv)
{
  int mode = 0;
  int i, rc = 0, nfiles = 0;
  const char *oname = 0;
  int use8080 = DEFAULT_USE8080, auto_stub = 1, optimal = 0;
  int showver = 0;

#ifndef __VBCC__
  LZ_GUARD_STACK (); /* before the first allocation; see LZ_STACK_RESERVE */
#endif

#ifndef LZPACK_DECODE_ONLY
  memtop = MEMTOP;
#endif

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
            {
              use8080 = 1;
              auto_stub = 0;
            }
          else if (c == 'Z' || c == 'z')
            {
              use8080 = 0;
              auto_stub = 0;
            }
          else if (c == 'e' || c == 'E')
            optimal = 1;
#ifndef LZPACK_DECODE_ONLY
          else if (c == 'c' || c == 'C')
            opt_checked = 1;
          else if (c == 'm' || c == 'M')
            {
              unsigned v = (i + 1 < argc) ? parse_memtop (argv[++i]) : 0;

              if (!v)
                {
                  /* Flawfinder: ignore */ /* False positive CWE-134 */
                  (void)fprintf (stderr, MSG_E_BADM);

                  return 2;
                }

              memtop = v;
            }
# ifndef LZPACK_COMPRESS_ONLY
          else if (c == 'f' || c == 'F')
            {
              unsigned v = (i + 1 < argc) ? parse_memtop (argv[++i]) : 0;

              if (!v)
                {
                  /* Flawfinder: ignore */ /* False positive CWE-134 */
                  (void)fprintf (stderr, MSG_E_BADF);

                  return 2;
                }

              opt_chk_floor = v;
              opt_checked = 1; /* -F implies -C */
            }
# endif
#else
          else if (c == 'c' || c == 'C')
            {
              /* ignored: the stub check is baked in at pack time */
            }
          else if (c == 'm' || c == 'M')
            {
              if (i + 1 < argc)
                i++;
            }
#endif
          else if (c == 'o' || c == 'O')
            {
              if (i + 1 >= argc)
                {
                  /* Flawfinder: ignore */ /* False positive CWE-134 */
                  (void)fprintf (stderr, MSG_E_OARG);

                  return 2;
                }

              oname = argv[++i];
            }
#ifdef LZ_CPM
          else if (c == 'i' || c == 'I')
            {
              if (cpm_has_lrbc ())
                opt_lrbc_isx = 1;
              else
                {
                  /* Flawfinder: ignore */ /* False positive CWE-134 */
                  (void)fprintf (stderr, MSG_E_IREQ);

                  return 2;
                }
            }
#endif
          else if (c == 'V' || c == 'v')
            showver = 1;
          else if (c == 'h' || c == 'H')
            {
              usage ();

              return 0;
            }
          else
            {
              /* Flawfinder: ignore */ /* False positive CWE-134 */
              (void)fprintf (stderr, MSG_E_UNKNOWN, argv[i]);

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
      /* Flawfinder: ignore */ /* False positive CWE-134 */
      (void)fprintf (stderr, MSG_E_OMULTI);

      return 2;
    }

#ifdef LZPACK_DECODE_ONLY
  if (mode == 0)
    mode = 1;
#endif

  /* Second pass: process each input file (skipping options). */

  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-' && argv[i][1])
        {
          char c = argv[i][1];

          if (c == 'o' || c == 'O' || c == 'm' || c == 'M'
#if !defined(LZPACK_DECODE_ONLY) && !defined(LZPACK_COMPRESS_ONLY)
              || c == 'f' || c == 'F'
#endif
          )
            i++;

          continue;
        }

#if LZ_INT_WILDCARD
      if (is_wildcard (argv[i]))
        {
          int found = 0;

# if defined(LZ_CPM) && LZPACK_WILDCARD
          unsigned char fcb[36];
          int k, j, hit = 0;

          cpm_setfcb (fcb, argv[i]);
          fcb[0] = 0; /* current drive only, ignores drive prefix */

          for (k = 1; k <= 11; k++)
            {
              if (k == 9)
                hit = 0;
              if (fcb[k] == '*')
                hit = 1;
              if (hit)
                fcb[k] = '?';
            }

          for (k = 0;; k++)
            {
              int r;

              (void)bdos (26, BDOS_FCB (g_dma));
              r = bdos (17, BDOS_FCB (fcb)) & 0xff;

              for (j = 0; j < k && r != 0xff; j++)
                r = bdos (18, BDOS_FCB (fcb)) & 0xff;

              if (r == 0xff)
                break;

              found = 1;
              rc |= LZ_RUN_ONE (fcb_to_name (g_dma + (r & 3) * 32));
            }
# elif LZ_WIN_WILDCARD
          LZ_FINDDATA fd;
          void *h = FindFirstFileA (argv[i], &fd);

          if (h != LZ_INVALID_HANDLE)
            {
              const char *s, *sep = 0;
              char wbuf[600];
              size_t plen;

              for (s = argv[i]; *s; s++)
                if (*s == '\\' || *s == '/' || *s == ':')
                  sep = s;
              plen = sep ? (size_t)(sep - argv[i] + 1) : 0;

              if (plen < sizeof (wbuf))
                {
                  (void)memcpy (wbuf, argv[i], plen);

                  do
                    {
                      size_t fn = strlen (fd.cFileName);

                      if (!(fd.dwFileAttributes & LZ_FILE_ATTR_DIRECTORY)
                          && plen + fn < sizeof (wbuf))
                        {
                          found = 1;
                          (void)memcpy (wbuf + plen, fd.cFileName, fn + 1);
                          rc |= LZ_RUN_ONE (wbuf);
                        }
                    }
                  while (FindNextFileA (h, &fd));
                }

              (void)FindClose (h);
            }
# endif

          if (!found)
            {
              /* Flawfinder: ignore */ /* False positive CWE-134 */
              (void)fprintf (stderr, MSG_E_READ, argv[i]);
              rc |= 1;
            }

          continue;
        }

      rc |= LZ_RUN_ONE (argv[i]);
#else
# ifndef LZPACK_DECODE_ONLY
      if (mode == 0)
        {
#  ifdef LZPACK_STREAM
          rc |= do_compress_stream (argv[i], oname, 1, use8080, auto_stub,
                                    optimal);
#  else
          rc |= do_compress (argv[i], oname, 1, use8080, auto_stub, optimal);
#  endif
        }
      else
# endif
      if (mode == 1)
        {
# ifdef LZPACK_COMPRESS_ONLY
          /* Flawfinder: ignore */ /* False positive CWE-134 */
          (void)fprintf (stderr, MSG_E_NOREST);
          rc |= 1;
# else
          rc |= do_restore (argv[i], oname, 1);
# endif
        }
      else
        rc |= do_list (argv[i]);
#endif
    }

#ifndef __VBCC__
  (void)use8080;
  (void)auto_stub;
  (void)optimal;
#endif

  return (rc ? 1 : 0);
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
