/*
 * strpack - LZPACK build-time message-string compressor
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: ae4ee402-60aa-11f1-9faf-80ee73e9b8e7
 */

/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#ifndef SP_MAXSTR
# define SP_MAXSTR 128
#endif

#ifndef SP_MAXTXT
# define SP_MAXTXT 8192
#endif

#ifndef SP_MAXBOOK
# define SP_MAXBOOK 126
#endif

#ifndef SP_MAXENT
# define SP_MAXENT 32
#endif

#ifndef SP_MINENT
# define SP_MINENT 2
#endif

#ifndef SP_TOPK
# define SP_TOPK 96
#endif

#ifndef SP_MAXNAME
# define SP_MAXNAME 32
#endif

#if 128 < SP_MAXBOOK
# error "SP_MAXBOOK must not exceed 128 (book refs are 0x80 + index)"
#endif

#if 1 > SP_MINENT
# error "SP_MINENT must be at least 1"
#endif

/******************************************************************************/

static unsigned char sp_text[SP_MAXTXT];
static long sp_off[SP_MAXSTR];
static long sp_len[SP_MAXSTR];
static char sp_name[SP_MAXSTR][SP_MAXNAME + 1];
static char sp_guard[SP_MAXSTR][120];
static int sp_train[SP_MAXSTR];
static int sp_nstr = 0;
static long sp_total = 0;
static unsigned char sp_book[SP_MAXBOOK][SP_MAXENT + 1];
static int sp_blen[SP_MAXBOOK];
static int sp_nbook = 0;
static long sp_cost[SP_MAXTXT + 1];

/******************************************************************************/

static long
dp_one (const unsigned char *s, long n)
{
  long i;
  int b;

  sp_cost[n] = 0;

  for (i = n - 1; 0 <= i; i--)
    {
      long best = 1 + sp_cost[i + 1];

      for (b = 0; b < sp_nbook; b++)
        {
          long el = (long)sp_blen[b];

          if (el <= n - i && 0 == memcmp (sp_book[b], s + i, (size_t)el))
            {
              long c = 1 + sp_cost[i + el];

              if (c < best)
                best = c;
            }
        }

      sp_cost[i] = best;
    }

  return sp_cost[0];
}

/******************************************************************************/

static long
dp_all (void)
{
  long t = 0;
  int i;

  for (i = 0; i < sp_nstr; i++)
    if (sp_train[i])
      t += dp_one (sp_text + sp_off[i], sp_len[i]);

  return t;
}

/******************************************************************************/

static long
book_cost (void)
{
  long t = 0;
  int i;

  for (i = 0; i < sp_nbook; i++)
    t += 1 + (long)sp_blen[i];

  return t;
}

/******************************************************************************/

typedef struct sp_cand
{
  long est;
  long pos;
  int len;
} sp_cand;

static sp_cand sp_top[SP_TOPK];
static int sp_ntop;

static unsigned char sp_resid[SP_MAXTXT];

static void
make_resid (void)
{
  int b, s;

  (void)memcpy (sp_resid, sp_text, sizeof (sp_resid));

  for (b = 0; b < sp_nbook; b++)
    for (s = 0; s < sp_nstr; s++)
      {
        unsigned char *t = sp_resid + sp_off[s];
        long left = sp_len[s];

        while ((long)sp_blen[b] <= left)
          {
            if (0 == memcmp (t, sp_book[b], (size_t)sp_blen[b]))
              {
                (void)memset (t, 0, (size_t)sp_blen[b]);
                t += sp_blen[b];
                left -= sp_blen[b];
              }
            else
              {
                t++;
                left--;
              }
          }
      }
}

/******************************************************************************/

static int
cand_known (const unsigned char *p, int len)
{
  int i;

  for (i = 0; i < sp_nbook; i++)
    if (sp_blen[i] == len && 0 == memcmp (sp_book[i], p, (size_t)len))
      return 1;

  return 0;
}

static int
cand_seen (const unsigned char *p, int len)
{
  int i;

  for (i = 0; i < sp_ntop; i++)
    if (sp_top[i].len == len
        && 0 == memcmp (sp_text + sp_top[i].pos, p, (size_t)len))
      return 1;

  return 0;
}

/******************************************************************************/

static long
cand_occur (const unsigned char *p, int len)
{
  long n = 0;
  int s;

  for (s = 0; s < sp_nstr; s++)
    {
      const unsigned char *t = sp_resid + sp_off[s];
      long left = sp_len[s];

      if (!sp_train[s])
        continue;

      while ((long)len <= left)
        {
          if (0 == memcmp (t, p, (size_t)len))
            {
              n++;
              t += len;
              left -= len;
            }
          else
            {
              t++;
              left--;
            }
        }
    }

  return n;
}

/******************************************************************************/

static void
top_insert (long est, long pos, int len)
{
  int i, j;

  for (i = 0; i < sp_ntop; i++)
    if (sp_top[i].est < est)
      break;

  if (SP_TOPK <= i)
    return;

  if (SP_TOPK > sp_ntop)
    sp_ntop++;

  for (j = sp_ntop - 1; j > i; j--)
    sp_top[j] = sp_top[(long)j - 1]; /* //-V557 */

  sp_top[i].est = est;
  sp_top[i].pos = pos;
  sp_top[i].len = len;
}

/******************************************************************************/

static void
harvest (void)
{
  int s, len;

  sp_ntop = 0;
  make_resid ();

  for (s = 0; s < sp_nstr; s++)
    {
      long i;

      if (!sp_train[s])
        continue;

      for (i = 0; i < sp_len[s]; i++)
        {
          const unsigned char *p = sp_resid + sp_off[s] + i;

          for (len = SP_MINENT; SP_MAXENT >= len && i + len <= sp_len[s];
               len++)
            {
              long occ, est;

              if (NULL != memchr (p, 0, (size_t)len))
                break;

              if (cand_known (p, len) || cand_seen (p, len))
                continue;

              occ = cand_occur (p, len);

              if (2 > occ)
                break;

              est = occ * (long)(len - 1) - (long)(len + 1);

              if (0 < est)
                top_insert (est, sp_off[s] + i, len);
            }
        }
    }
}

/******************************************************************************/

static void
build_book (void)
{
  for (;;)
    {
      long base, bestgain;
      int bestc, c;

      if (SP_MAXBOOK <= sp_nbook)
        return;

      base = dp_all () + book_cost ();
      harvest ();

      bestgain = 0;
      bestc = -1;

      for (c = 0; c < sp_ntop; c++)
        {
          long now, gain;

          (void)memcpy (sp_book[sp_nbook], sp_text + sp_top[c].pos,
                        (size_t)sp_top[c].len);
          sp_blen[sp_nbook] = sp_top[c].len;
          sp_nbook++;

          now = dp_all () + book_cost ();
          gain = base - now;

          sp_nbook--;

          if (gain > bestgain)
            {
              bestgain = gain;
              bestc = c;
            }
        }

      if (0 > bestc)
        return;

      (void)memcpy (sp_book[sp_nbook], sp_text + sp_top[bestc].pos,
                    (size_t)sp_top[bestc].len);
      sp_blen[sp_nbook] = sp_top[bestc].len;
      sp_nbook++;
    }
}

/******************************************************************************/

static void
prune_book (void)
{
  int again = 1;

  while (again)
    {
      int i;

      again = 0;

      for (i = 0; i < sp_nbook; i++)
        {
          unsigned char keep[SP_MAXENT + 1];
          long base, now;
          int klen, j;

          base = dp_all () + book_cost ();

          (void)memcpy (keep, sp_book[i], sizeof (keep));
          klen = sp_blen[i];

          for (j = i; j < sp_nbook - 1; j++)
            {
              (void)memcpy (sp_book[j], sp_book[(long)j + 1],
                            sizeof (sp_book[j]));
              sp_blen[j] = sp_blen[(long)j + 1];
            }

          sp_nbook--;
          now = dp_all () + book_cost ();

          if (now <= base)
            {
              again = 1;
            }
          else
            {
              for (j = sp_nbook; j > i; j--)
                {
                  (void)memcpy (sp_book[j], sp_book[(long)j - 1],
                                sizeof (sp_book[j]));
                  sp_blen[j] = sp_blen[(long)j - 1];
                }

              (void)memcpy (sp_book[i], keep, sizeof (keep));
              sp_blen[i] = klen;
              sp_nbook++;
            }
        }
    }
}

/******************************************************************************/

static long
emit_one (const unsigned char *s, long n, unsigned char *out)
{
  long i = 0, o = 0;

  if (0 > n || SP_MAXTXT < n)
    {
      out[0] = 0;

      return 0;
    }

  (void)dp_one (s, n);

  while (i < n)
    {
      int b, hit = -1;

      for (b = 0; b < sp_nbook; b++)
        {
          long el = (long)sp_blen[b];

          if (el <= n - i && 0 == memcmp (sp_book[b], s + i, (size_t)el)
              && sp_cost[i] == 1 + sp_cost[i + el])
            {
              hit = b;
              break;
            }
        }

      if (0 <= hit)
        {
          out[o++] = (unsigned char)(0x80 + hit);
          i += sp_blen[hit];
        }
      else
        {
          out[o++] = s[i++];
        }
    }

  out[o] = 0;

  return o;
}

/******************************************************************************/

static int
verify_one (const unsigned char *coded, const unsigned char *plain,
            long plen)
{
  long o = 0;

  while (*coded)
    {
      int c = *coded++;

      if (0x80 > c)
        {
          if (o >= plen || plain[o] != (unsigned char)c)
            return -1;

          o++;
        }
      else
        {
          int idx = c - 0x80;

          if (idx >= sp_nbook)
            return -1;

          if (o + sp_blen[idx] > plen
              || 0 != memcmp (plain + o, sp_book[idx],
                              (size_t)sp_blen[idx]))
            return -1;

          o += sp_blen[idx];
        }
    }

  return ((o == plen) ? 0 : -1);
}

/******************************************************************************/

static void
put_esc (FILE *f, int c)
{
  if ('\\' == c || '"' == c)
    (void)fprintf (f, "\\%c", c);
  else if ('\n' == c)
    (void)fprintf (f, "\\n");
  else if (32 <= c && 127 > c)
    (void)fprintf (f, "%c", c);
  else
    (void)fprintf (f, "\\%03o", (unsigned)(c & 0xff));
}

/******************************************************************************/

static long
parse_text (const char *p)
{
  long base = sp_off[sp_nstr];
  long n = 0;

  if (0 > base || SP_MAXTXT <= base)
    return -1;

  while (*p)
    {
      int c = (unsigned char)*p++;

      if ('\\' == c)
        {
          int e = (unsigned char)*p++;

          if ('n' == e)
            c = '\n';
          else if ('t' == e)
            c = '\t';
          else if ('r' == e)
            c = '\r';
          else if ('\\' == e || '"' == e)
            c = e;
          else if ('x' == e)
            {
              int v = 0, k;

              for (k = 0; 2 > k; k++)
                {
                  int d = (unsigned char)*p;

                  if ('0' <= d && '9' >= d)
                    d -= '0';
                  else if ('a' <= d && 'f' >= d)
                    d -= 'a' - 10;
                  else if ('A' <= d && 'F' >= d)
                    d -= 'A' - 10;
                  else
                    break;

                  v = v * 16 + d;
                  p++;
                }

              c = v;
            }
          else
            return -1;
        }

      if (0 == c || 0x7f < c)
        return -1;

      if (SP_MAXTXT - 1 <= base + n)
        return -1;

      sp_text[base + n++] = (unsigned char)c;
    }

  sp_text[base + n] = 0;

  return n;
}

/******************************************************************************/

static int
read_input (FILE *f)
{
  static char line[1024];

  while (fgets (line, (int)sizeof (line), f))
    {
      char *tab, *nl;
      size_t nlen;
      long n;

      nl = strchr (line, '\n');

      if (NULL != nl)
        *nl = 0;

      if (0 == line[0] || '#' == line[0])
        continue;

      tab = strchr (line, '\t');

      if (NULL == tab || line == tab || 900 < strlen (line))
        return -1;

      *tab = 0;
      nlen = strlen (line);

      if (SP_MAXNAME < nlen || 0 > sp_nstr || SP_MAXSTR <= sp_nstr)
        return -1;

      {
        const char *q;

        for (q = line; *q; q++)
          if (('A' > *q || 'Z' < *q) && ('a' > *q || 'z' < *q)
              && '_' != *q && (line == q || '0' > *q || '9' < *q))
            return -1;
      }

      (void)memcpy (sp_name[sp_nstr], line, nlen + 1);

      {
        char *g = tab + 1;
        size_t glen;

        tab = strchr (g, '\t');

        if (NULL == tab)
          return -1;

        *tab = 0;
        glen = strlen (g);

        if (0 == glen || (sizeof (sp_guard[0]) - 1) < glen)
          return -1;

        (void)memcpy (sp_guard[sp_nstr], g, glen + 1);

        {
          const char *d = strstr (g, "defined(LZPACK_DECODE_ONLY)");

          sp_train[sp_nstr] = ((NULL == d || (d > g && '!' == d[-1]))
                                 ? 1
                                 : 0);
        }
      }
      sp_off[sp_nstr] = ((0 == sp_nstr)
                           ? 0
                           : (sp_off[(long)sp_nstr - 1]
                              + sp_len[(long)sp_nstr - 1] + 1));
      n = parse_text (tab + 1);

      if (0 > n)
        return -1;

      sp_len[sp_nstr] = n;
      sp_total += n;
      sp_nstr++;
    }

  if (0 != ferror (f))
    return -1;

  return ((0 < sp_nstr) ? 0 : -1);
}

/******************************************************************************/

static void
write_header (FILE *f)
{
  static unsigned char coded[SP_MAXTXT];
  long enc_total = 0, blob;
  int i;
  long k;

  (void)fprintf (f, "/*  Generated by strpack - DO NOT EDIT. */\n\n");
  (void)fprintf (f, "#ifdef LZ_MSG_PACKED\n\n");
  (void)fprintf (f, "static const unsigned char lz_msgbook[] = {\n");

  for (i = 0; i < sp_nbook; i++)
    {
      int j;

      (void)fprintf (f, "  %d, /* codespell:ignore */ /* \"", sp_blen[i]);

      for (j = 0; j < sp_blen[i]; j++)
        put_esc (f, sp_book[i][j]);

      (void)fprintf (f, "\" */\n  ");

      for (j = 0; j < sp_blen[i]; j++)
        (void)fprintf (f, "0x%02x,", (unsigned)sp_book[i][j]);

      (void)fprintf (f, "\n");
    }

  (void)fprintf (f, "  0\n};\n\n");

  for (i = 0; i < sp_nstr; i++)
    {
      long n = emit_one (sp_text + sp_off[i], sp_len[i], coded);
      long j;
      int guarded = (0 != strcmp (sp_guard[i], "-"));

      if (0 != verify_one (coded, sp_text + sp_off[i], sp_len[i]))
        {
          (void)fprintf (stderr,
                         "strpack: FATAL: %s does not round-trip\n",
                         sp_name[i]);
          exit (1);
        }

      enc_total += n;

      if (guarded)
        (void)fprintf (f, "# if %s\n", sp_guard[i]);

      {
        long pv = sp_len[i];

        if (40 < pv)
          {
            pv = 40;

            while (0 < pv && ' ' != sp_text[sp_off[i] + pv - 1])
              pv--;
          }

        (void)fprintf (f, "/* \"");

        for (k = 0; k < pv; k++)
          put_esc (f, sp_text[sp_off[i] + k]);

        (void)fprintf (f, "%s\" */\n", ((pv < sp_len[i]) ? "..." : ""));
      }
      (void)fprintf (f, "static const unsigned char lz_m_%s[%ld] = {\n  ",
                     sp_name[i], n + 1);

      for (j = 0; j <= n; j++)
        (void)fprintf (f, "0x%02x,%s", (unsigned)coded[j],
                       ((11 == (j % 12)) ? "\n  " : ""));

      (void)fprintf (f, "\n};\n#  define %s ((const char *)lz_m_%s)\n",
                     sp_name[i], sp_name[i]);

      if (guarded)
        (void)fprintf (f, "# endif\n");

      (void)fprintf (f, "\n");
    }

  (void)fprintf (f, "#else /* !LZ_MSG_PACKED */\n\n");

  for (i = 0; i < sp_nstr; i++)
    {
      long j = 0;
      int guarded = (0 != strcmp (sp_guard[i], "-"));

      if (guarded)
        (void)fprintf (f, "# if %s\n", sp_guard[i]);

      (void)fprintf (f, "#  define %s \\\n", sp_name[i]);

      do
        {
          long cut = j + 56;

          if (cut < sp_len[i])
            {
              long w = cut;

              while (j < w && ' ' != sp_text[sp_off[i] + w - 1])
                w--;

              if (j < w)
                cut = w;
            }

          (void)fprintf (f, "  \"");

          while (j < sp_len[i] && j < cut)
            put_esc (f, sp_text[sp_off[i] + j++]);

          (void)fprintf (f, "\"%s\n",
                         ((j < sp_len[i]) ? " \\" : ""));
        }
      while (j < sp_len[i]);

      if (guarded)
        (void)fprintf (f, "# endif\n");

      (void)fprintf (f, "\n");
    }

  (void)fprintf (f, "#endif /* LZ_MSG_PACKED */\n\n");

  blob = book_cost () + 1;
  (void)fprintf (f, "/* STRPACK_PLAIN  == %ld */\n", sp_total + sp_nstr);
  (void)fprintf (f, "/* STRPACK_CODED  == %ld */\n", enc_total + sp_nstr);
  (void)fprintf (f, "/* STRPACK_BOOK   == %ld */\n", blob);
  (void)fprintf (f, "/* STRPACK_NET    == %ld */\n",
                 sp_total - enc_total - blob);
  (void)fprintf (f, "/* STRPACK_NBOOK  == %d */\n", sp_nbook);
}

/******************************************************************************/

int
main (int argc, char **argv)
{
  FILE *f;

  if (2 != argc)
    {
      (void)fprintf (stderr, "usage: strpack <messages.def> > header.h\n");

      return 2;
    }

  f = fopen (argv[1], "r");

  if (NULL == f)
    {
      (void)fprintf (stderr, "strpack: cannot read %s\n", argv[1]);

      return 1;
    }

  if (0 != read_input (f))
    {
      (void)fclose (f);
      (void)fprintf (stderr, "strpack: bad input line in %s\n", argv[1]);

      return 1;
    }

  (void)fclose (f);

  build_book ();
  prune_book ();
  write_header (stdout);

  if (0 != fflush (stdout) || 0 != ferror (stdout))
    {
      (void)fprintf (stderr, "strpack: write error\n");

      return 1;
    }

  return 0;
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
