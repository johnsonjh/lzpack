/*
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 * scspell-id: aa965896-585c-11f1-8233-80ee73e9b8e7
 */

/******************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#ifndef ERANGE
# define ERANGE 1
#endif

/******************************************************************************/

static long
xstrtol (const char *nptr, const char **endptr, int base)
{
  const char *s = nptr;
  unsigned long acc = 0;
  int c, neg = 0, any = 0, overflow = 0;

  unsigned long max_pos = (unsigned long)LONG_MAX;
  unsigned long max_neg = max_pos + 1UL;

  unsigned long cutoff;
  int cutlim;

  if (base != 10 && base != 16)
    {
      if (endptr)
        *endptr = nptr;

      return 0L;
    }

  while ((c = (unsigned char)*s) != '\0' && isspace (c))
    s++;

  c = (unsigned char)*s;

  if (c == '+' || c == '-')
    {
      neg = (c == '-');
      s++;
    }

  if (base == 16)
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
      s += 2;

  cutoff = (neg ? max_neg : max_pos);
  cutlim = (int)(cutoff % (unsigned long)base);
  cutoff /= (unsigned long)base;

  for (;; s++)
    {
      c = (unsigned char)*s;

      if (base == 10)
        {
          if (!isdigit (c))
            break;

          c -= '0';
        }
      else
        {
          if (c >= '0' && c <= '9')
            c -= '0';
          else if (c >= 'a' && c <= 'f')
            c = c - 'a' + 10;
          else if (c >= 'A' && c <= 'F')
            c = c - 'A' + 10;
          else
            break;
        }

      if (acc > cutoff || (acc == cutoff && c > cutlim))
        {
          overflow = 1;
          errno = ERANGE;
          acc = (neg ? max_neg : max_pos);

          for (s++;; s++)
            {
              c = (unsigned char)*s;

              if (base == 10)
                {
                  if (!isdigit (c))
                    break;
                }
              else
                {
                  if (!isxdigit (c))
                    break;
                }
            }

          break;
        }

      any = 1;
      acc = acc * (unsigned long)base + (unsigned long)c;
    }

  if (endptr)
    *endptr = (any ? s : nptr);

  if (!any)
    return 0L;

  if (overflow)
    return (neg ? LONG_MIN : LONG_MAX);

  if (neg)
    return -(long)acc;

  return (long)acc;
}

/******************************************************************************/

/*
 * Capacity limits.  Defaults are generous for the host; a CP/M-80 build can
 * shrink them (e.g. -DMAXSYM=96 -DMAXREF=96 -DMAXCODE=768) to fit 64K.  The
 * stub sources only need a fraction of these.
 */

#ifndef MAXSYM
# define MAXSYM 256
#endif

#ifndef MAXREF
# define MAXREF 512
#endif

#ifndef MAXCODE
# define MAXCODE 4096
#endif

#ifndef NAMELEN
# define NAMELEN 24 /* longest stub symbol is ~12 chars; 23 + NUL fits! */
#endif

/******************************************************************************/

static char sym_name[MAXSYM][NAMELEN];
static long sym_val[MAXSYM];
static int sym_islabel[MAXSYM];
static int nsym;

/******************************************************************************/

typedef struct
{
  int off;
  char name[NAMELEN];
  int width;
} Ref;

static Ref refs[MAXREF];

/******************************************************************************/

static int nref;
static unsigned char code[MAXCODE];
static int clen;

/******************************************************************************/

static void
die (const char *m)
{
  (void)fprintf (stderr, "stubasm: %s\n", m);

  exit (1);
}

/******************************************************************************/

static int
sym_find (const char *n)
{
  int i;

  for (i = 0; i < nsym; i++)
    if (strcmp (sym_name[i], n) == 0)
      return i;

  return -1;
}

/******************************************************************************/

static void
sym_set (const char *n, long v, int islabel)
{
  int i = sym_find (n);

  if (i < 0)
    {
      if (nsym >= MAXSYM)
        die ("too many symbols");

      i = nsym++;

      if (strlen (n) >= (size_t)NAMELEN)
        die ("symbol too long");

      (void)strcpy (sym_name[i], n);
    }

  sym_val[i] = v;
  sym_islabel[i] = islabel;
}

/******************************************************************************/

static void
upcase (char *s)
{
  for (; *s; s++)
    *s = (char)toupper ((unsigned char)*s);
}

/******************************************************************************/

static int
is_ident (const char *s)
{
  return s && (s[0] == '_' || s[0] == '.' || isalpha ((unsigned char)s[0]));
}

/******************************************************************************/

static int
is_symbol (const char *s)
{
  if (!is_ident (s))
    return 0;

  for (; *s; s++)
    if (!(isalnum ((unsigned char)*s) || *s == '_' || *s == '.'))
      return 0;

  return 1;
}

/******************************************************************************/

static long
eval1 (const char *tok, int pass, long loc)
{
  size_t n = strlen (tok);

  if (strcmp (tok, "$") == 0)
    return loc;

  if (n > 1 &&
       (tok[n - 1] == 'h' || tok[n - 1] == 'H') &&
         (tok[0] >= '0' && tok[0] <= '9'))
    {
      unsigned long v = 0;
      size_t i;

      for (i = 0; i < n - 1; i++)
        {
          char c = tok[i];
          int d;

          if (c >= '0' && c <= '9')
            d = c - '0';
          else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
          else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
          else
            {
              (void)fprintf (stderr, "stubasm: bad hex '%s'\n", tok);

              exit (1);
            }

          v = v * 16 + (unsigned long)d;
        }

      return (long)v;
    }

  if (tok[0] >= '0' && tok[0] <= '9')
    {
      if (n > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
        return xstrtol (tok + 2, 0, 16);

      return xstrtol (tok, 0, 10);
    }

  {
    int i = sym_find (tok);

    if (i >= 0)
      return sym_val[i];

    if (pass == 1)
      return 0;

    (void)fprintf (stderr, "stubasm: undefined symbol '%s'\n", tok);

    exit (1);
  }
}

/******************************************************************************/

static long
evals (const char *s, int pass, long loc)
{
  long acc = 0;
  int sign = 1;
  char term[256];
  int ti = 0;
  size_t i;

  if (!s)
    return 0;

  (void)memset (term, 0, sizeof (term));

  for (i = 0;; i++)
    {
      char c = s[i];

      if (c == '+' || c == '-' || c == 0)
        {
          if (ti > 0)
            {
              term[ti] = 0;
              acc += sign * eval1 (term, pass, loc);
              ti = 0;
            }

          if (c == 0)
            break;

          sign = ((c == '+') ? 1 : -1);
        }
      else if (!isspace ((unsigned char)c))
        {
          if (ti < (int)sizeof (term) - 1)
            term[ti++] = c;
          else
            {
              (void)fprintf (stderr, "stubasm: expression term too long\n");

              exit (1);
            }
        }
    }

  return acc;
}

/******************************************************************************/

static int
regidx (const char *const *tab, const char *t, const char *what)
{
  int i;

  if (!t)
    {
      (void)fprintf (stderr, "stubasm: missing %s\n", what);

      exit (1);
    }

  for (i = 0; tab[i]; i++)
    if (!strcmp (tab[i], t))
      return i;

  (void)fprintf (stderr, "stubasm: bad %s '%s'\n", what, t);

  exit (1);

  /*NOTREACHED*/ /* unreachable */
  return 0;
}

/******************************************************************************/

static int
r8 (const char *t)
{
  static const char *T[] = { "B", "C", "D", "E", "H", "L", "M", "A", 0 };

  return regidx (T, t, "reg");
}

/******************************************************************************/

static int
rp (const char *t)
{
  static const char *T[] = { "B", "D", "H", "SP", 0 };

  return regidx (T, t, "rp");
}

/******************************************************************************/

static int
rpp (const char *t)
{
  static const char *T[] = { "B", "D", "H", "PSW", 0 };

  return regidx (T, t, "rpp");
}

/******************************************************************************/

static void
emit (int b)
{
  if (clen >= MAXCODE)
    die ("code overflow");

  code[clen++] = (unsigned char)(b & 0xff);
}

/******************************************************************************/

static void
rec (const char *tok, int width)
{
  if (is_symbol (tok))
    {
      if (nref >= MAXREF)
        die ("too many refs");

      refs[nref].off = clen + 1;

      if (tok)
        {
          if (strlen (tok) >= (size_t)NAMELEN)
            die ("reference name too long");

          (void)strcpy (refs[nref].name, tok);
        }

      refs[nref].width = width;
      nref++;
    }
}

/******************************************************************************/

typedef struct
{
  const char *name;
  int op;
} OpB;

/******************************************************************************/

static int
lookup (const OpB *t, const char *n)
{
  int i;

  for (i = 0; t[i].name; i++)
    if (!strcmp (t[i].name, n))
      return t[i].op;

  return -1;
}

/******************************************************************************/

static void
assemble (const char *path)
{
  static const OpB simple[] =
    { { "NOP",  0x00 }, { "HLT",  0x76 }, { "XCHG", 0xEB }, { "XTHL", 0xE3 },
      { "SPHL", 0xF9 }, { "PCHL", 0xE9 }, { "RET",  0xC9 }, { "RLC",  0x07 },
      { "RRC",  0x0F }, { "RAL",  0x17 }, { "RAR",  0x1F }, { "CMA",  0x2F },
      { "STC",  0x37 }, { "CMC",  0x3F }, { "DAA",  0x27 }, { "RNZ",  0xC0 },
      { "RZ",   0xC8 }, { "RNC",  0xD0 }, { "RC",   0xD8 }, { "RPO",  0xE0 },
      { "RPE",  0xE8 }, { "RP",   0xF0 }, { "RM",   0xF8 }, { 0, 0 } };

  static const OpB alu[] =
    { { "ADD", 0x80 }, { "ADC", 0x88 }, { "SUB", 0x90 },
      { "SBB", 0x98 }, { "ANA", 0xA0 }, { "XRA", 0xA8 },
      { "ORA", 0xB0 }, { "CMP", 0xB8 }, { 0, 0 } };

  static const OpB alui[] =
    { { "ADI", 0xC6 }, { "ACI", 0xCE }, { "SUI", 0xD6 },
      { "SBI", 0xDE }, { "ANI", 0xE6 }, { "XRI", 0xEE },
      { "ORI", 0xF6 }, { "CPI", 0xFE }, { 0, 0 } };

  static const OpB jmp[] =
    { { "JMP", 0xC3 }, { "JNZ", 0xC2 }, { "JZ",  0xCA }, { "JNC", 0xD2 },
      { "JC",  0xDA }, { "JPO", 0xE2 }, { "JPE", 0xEA }, { "JP",  0xF2 },
      { "JM",  0xFA }, { 0, 0 } };

  static const OpB call[] =
    { { "CALL", 0xCD }, { "CNZ", 0xC4 }, { "CZ",  0xCC }, { "CNC", 0xD4 },
      { "CC",   0xDC }, { "CPO", 0xE4 }, { "CPE", 0xEC }, { "CP",  0xF4 },
      { "CM",   0xFC }, { 0, 0 } };

  static const OpB mem[] =
    { { "LDA",  0x3A }, { "STA",  0x32 },
      { "LHLD", 0x2A }, { "SHLD", 0x22 }, { 0, 0 } };

  char line[1024];
  FILE *f;
  int pass;

  nsym = 0;

  for (pass = 1; pass <= 2; pass++)
    {
      long org = 0, have_org = 0;
      clen = 0;
      nref = 0;

      f = fopen (path, "r");

      if (!f)
        {
          (void)fprintf (stderr, "stubasm: cannot open %s\n", path);

          exit (1);
        }

      while (fgets (line, (int)sizeof (line), f))
        {
          char *s, *semi, *a0, *colon;
          const char *op, *a1;
          char opU[256];
          long loc;
          int v;

          semi = 0;

          {
            int q = 0;
            char *p;
            for (p = line; *p; p++)
              {
                if (*p == '\'' || *p == '"')
                  {
                    if (q == 0) q = *p;
                    else if (q == *p) q = 0;
                  }
                else if (*p == ';' && q == 0)
                  {
                    semi = p;

                    break;
                  }
              }
          }

          if (semi)
            *semi = 0;

          s = line;

          while (*s && isspace ((unsigned char)*s))
            s++;

          {
            char *e = s + strlen (s);

            while (e > s && isspace ((unsigned char)e[-1]))
              *--e = 0;
          }

          if (!*s)
            continue;

          colon = strchr (s, ':');

          if (colon)
            {
              char save = *colon;
              char *p;
              int isl = 1;

              *colon = 0;

              for (p = s; *p; p++)
                if (!(is_ident (p) || (*p >= '0' && *p <= '9')))
                  {
                    isl = 0;

                    break;
                  }

              if (isl && p != s)
                {
                  loc = (have_org ? org : 0) + clen;
                  sym_set (s, loc, 1);
                  s = colon + 1;

                  while (*s && isspace ((unsigned char)*s))
                    s++;

                  if (!*s)
                    continue;
                }
              else
                *colon = save;
            }

          op = s;

          while (*s && !isspace ((unsigned char)*s))
            s++;

          if (*s)
            {
              *s++ = 0;

              while (*s && isspace ((unsigned char)*s))
                s++;
            }

          {
            char *t = s;
            char w[32];
            char *q = t;
            int k = 0;

            while (*q && !isspace ((unsigned char)*q) && k < 31)
              w[k++] = *q++;

            w[k] = 0;
            upcase (w);

            if (!strcmp (w, "EQU"))
              {
                char *val = q;

                while (*val && isspace ((unsigned char)*val))
                  val++;

                {
                  char *e = val + strlen (val);

                  while (e > val && isspace ((unsigned char)e[-1]))
                    *--e = 0;
                }

                sym_set (op, evals (val, pass, (long)0), 0);

                continue;
              }
          }

          a0 = s;
          a1 = 0;

          {
            char *cm = strchr (s, ',');

            if (cm)
              {
                *cm = 0;
                a1 = cm + 1;

                while (*a1 && isspace ((unsigned char)*a1))
                  a1++;

                {
                  char *e = a0 + strlen (a0);

                  while (e > a0 && isspace ((unsigned char)e[-1]))
                    *--e = 0;
                }
              }
          }

          if (strlen (op) >= sizeof (opU))
            {
              (void)fprintf (stderr, "stubasm: opcode too long '%s'\n", op);

              exit (1);
            }

          (void)strcpy (opU, op);
          upcase (opU);
          loc = (have_org ? org : 0) + clen;

          if (!strcmp (opU, "ORG"))
            {
              long nv = evals (a0, pass, loc);
              if (!have_org)
                {
                  org = nv;
                  have_org = 1;
                }
              else
                while (org + clen < nv)
                  emit (0);

              continue;
            }

          if (!strcmp (opU, "DB"))
            {
              char *tok = a0;

              while (tok)
                {
                  char *cm = 0;
                  {
                    int q = 0;
                    char *p;
                    for (p = tok; *p; p++)
                      {
                        if (*p == '\'' || *p == '"')
                          {
                            if (q == 0) q = *p;
                            else if (q == *p) q = 0;
                          }
                        else if (*p == ',' && q == 0)
                          {
                            cm = p;

                            break;
                          }
                      }
                  }

                  if (cm)
                    *cm = 0;

                  while (*tok && isspace ((unsigned char)*tok))
                    tok++;

                  if (tok[0] == '\'' || tok[0] == '"')
                   {
                     const char *p = tok + 1;

                     while (*p && *p != tok[0])
                       emit (*p++);
                   }
                  else
                    emit ((int)evals (tok, pass, loc));

                  tok = (cm ? cm + 1 : (char *)0);
                }

              continue;
            }

          if (!strcmp (opU, "DW"))
            {
              long v2 = evals (a0, pass, loc);

              emit ((int)(v2 & 0xff));
              emit ((int)((v2 >> 8) & 0xff));

              continue;
            }

          if ((v = lookup (simple, opU)) >= 0)
            {
              emit (v);

              continue;
            }

          if (!strcmp (opU, "LDAX"))
            {
              if (!a0)
                {
                  (void)fprintf (stderr, "stubasm: missing reg for LDAX\n");

                  exit (1);
                }

              emit (!strcmp (a0, "B") ? 0x0A : 0x1A);

              continue;
            }

          if (!strcmp (opU, "STAX"))
            {
              if (!a0)
                {
                  (void)fprintf (stderr, "stubasm: missing reg for STAX\n");

                  exit (1);
                }

              emit (!strcmp (a0, "B") ? 0x02 : 0x12);

              continue;
            }

          if ((v = lookup (mem, opU)) >= 0)
            {
              long w;

              if (a0)
                rec (a0, 2);

              w = evals (a0, pass, loc);

              emit (v);
              emit ((int)(w & 0xff));
              emit ((int)((w >> 8) & 0xff));

              continue;
            }

          if (!strcmp (opU, "LXI"))
            {
              long w;
              int r = rp (a0);

              if (a1)
                rec (a1, 2);

              w = evals (a1, pass, loc);

              emit (0x01 | (r << 4));
              emit ((int)(w & 0xff));
              emit ((int)((w >> 8) & 0xff));

              continue;
            }

          if (!strcmp (opU, "MVI"))
            {
              long w;
              int r = r8 (a0);

              if (a1)
                rec (a1, 1);

              w = evals (a1, pass, loc);

              emit (0x06 | (r << 3));
              emit ((int)(w & 0xff));

              continue;
            }

          if (!strcmp (opU, "MOV"))
            {
              emit (0x40 | (r8 (a0) << 3) | r8 (a1));

              continue;
            }

          if (!strcmp (opU, "INX"))
            {
              emit (0x03 | (rp (a0) << 4));

              continue;
            }

          if (!strcmp (opU, "DCX"))
            {
              emit (0x0B | (rp (a0) << 4));

              continue;
            }

          if (!strcmp (opU, "DAD"))
            {
              emit (0x09 | (rp (a0) << 4));

              continue;
            }

          if (!strcmp (opU, "INR"))
            {
              emit (0x04 | (r8 (a0) << 3));

              continue;
            }

          if (!strcmp (opU, "DCR"))
            {
              emit (0x05 | (r8 (a0) << 3));

              continue;
            }

          if (!strcmp (opU, "PUSH"))
            {
              emit (0xC5 | (rpp (a0) << 4));

              continue;
            }

          if (!strcmp (opU, "POP"))
            {
              emit (0xC1 | (rpp (a0) << 4));

              continue;
            }

          if ((v = lookup (alu, opU)) >= 0)
            {
              emit (v | r8 (a0));

              continue;
            }

          if ((v = lookup (alui, opU)) >= 0)
            {
              long w;

              if (a0)
                rec (a0, 1);

              w = evals (a0, pass, loc);

              emit (v);
              emit ((int)(w & 0xff));

              continue;
            }

          if ((v = lookup (jmp, opU)) >= 0)
            {
              long w;

              if (a0)
                rec (a0, 2);

              w = evals (a0, pass, loc);

              emit (v);
              emit ((int)(w & 0xff));
              emit ((int)((w >> 8) & 0xff));

              continue;
            }

          if ((v = lookup (call, opU)) >= 0)
            {
              long w;

              if (a0)
                rec (a0, 2);

              w = evals (a0, pass, loc);

              emit (v);
              emit ((int)(w & 0xff));
              emit ((int)((w >> 8) & 0xff));

              continue;
            }

          (void)fprintf (stderr, "stubasm: bad op '%s'\n", opU);

          exit (1);
        }

      (void)fclose (f);
    }
}

/******************************************************************************/

static const char *SETUP_PATCH[] =
  { "LIT_SRC",  "DCMP_SRCTOP", "DCMP_DSTTOP",
    "DCMP_LEN", "DCMP_RUN", 0 };

/******************************************************************************/

static const char *DECOMP_PATCH[] =
  { "OUT_END_HI", "OUT_END_LO", "PL_SRCTOP",
    "PL_DSTTOP",  "PL_LEN",     "PL_DSTBOT", 0 };

/******************************************************************************/

static int
in_list (const char *const *l, const char *n)
{
  int i;

  for (i = 0; l[i]; i++)
    if (!strcmp (l[i], n))
      return 1;

  return 0;
}

/******************************************************************************/

static void
emit_bytes (const char *name, const unsigned char *b, int n)
{
  int i;

  (void)printf ("static const unsigned char %s[%d] = {\n", name, n);

  for (i = 0; i < n; i++)
    {
      if (i % 12 == 0)
        (void)printf ("    ");

      (void)printf ("0x%02x,%s",
                    b[i], ((i % 12 == 11 || i == n - 1) ? "\n" : " "));
    }

  (void)printf ("};\n\n");
}

/******************************************************************************/

static int fx_off[MAXREF], fx_tgt[MAXREF], nfx;
static char sl_name[MAXREF][NAMELEN];
static int sl_off[MAXREF], sl_w[MAXREF], nsl;

static void
collect (const char *const *patch)
{
  int j;

  nfx = 0;
  nsl = 0;

  for (j = 0; j < nref; j++)
    {
      int k = sym_find (refs[j].name);

      if (k >= 0 && sym_islabel[k])
        {
          fx_off[nfx] = refs[j].off;
          fx_tgt[nfx] = (int)sym_val[k];

          nfx++;
        }
      else if (in_list (patch, refs[j].name))
        {
          if (nsl >= MAXREF)
            die ("too many patches");

          if (strlen (refs[j].name) >= (size_t)NAMELEN) /* //-V547 */
            die ("patch name too long");

          (void)strcpy (sl_name[nsl], refs[j].name);
          sl_off[nsl] = refs[j].off;
          sl_w[nsl] = refs[j].width;

          nsl++;
        }
    }
}

/******************************************************************************/

static unsigned char setup[MAXCODE], decomp[MAXCODE];
static int s_fx_off[MAXREF], s_fx_tgt[MAXREF];
static char s_sl_name[MAXREF][NAMELEN];
static int s_sl_off[MAXREF];

int
main (int argc, char **argv)
{
  int slen, dlen, i;
  int s_nfx = 0;
  int s_nsl = 0;

  (void)memset (s_fx_off, 0, sizeof (s_fx_off));
  (void)memset (s_fx_tgt, 0, sizeof (s_fx_tgt));
  (void)memset (s_sl_name, 0, sizeof (s_sl_name));
  (void)memset (s_sl_off, 0, sizeof (s_sl_off));

  if (argc < 3)
    {
      (void)fprintf (stderr, "usage: stubasm setup.asm decomp.asm > stub.h\n");

      return 2;
    }

  assemble (argv[1]);
  slen = clen;
  (void)memcpy (setup, code, (size_t)clen);
  collect (SETUP_PATCH);

  s_nfx = nfx;

  for (i = 0; i < nfx; i++)
    {
      s_fx_off[i] = fx_off[i];
      s_fx_tgt[i] = fx_tgt[i];
    }

  s_nsl = nsl;

  for (i = 0; i < nsl; i++)
    {
      (void)strcpy (s_sl_name[i], sl_name[i]);
      s_sl_off[i] = sl_off[i];
    }

  assemble (argv[2]);
  dlen = clen;
  (void)memcpy (decomp, code, (size_t)clen);
  collect (DECOMP_PATCH);

  (void)printf ("#ifndef STUBASM_CS8080_H\n");
  (void)printf ("# define STUBASM_CS8080_H\n\n");

  (void)printf ("# define S8_SLEN %d\n", slen);
  (void)printf ("# define S8_DLEN %d\n\n", dlen);

  emit_bytes ("setup8080", setup, slen);
  emit_bytes ("decomp8080", decomp, dlen);

  (void)printf ("static const unsigned short setup8080_fix[][2] = {\n");

  for (i = 0; i < s_nfx; i++)
    (void)printf ("    { %#4x, %#4x },\n", s_fx_off[i], s_fx_tgt[i]);

  (void)printf ("};\n\n# define SETUP8080_FIX_N %d\n", s_nfx);
  (void)printf ("\nstatic const unsigned short decomp8080_fix[][2] = {\n");

  for (i = 0; i < nfx; i++)
    (void)printf ("    { %#4x, %#5x },\n", fx_off[i], fx_tgt[i]);

  (void)printf ("};\n# define DECOMP8080_FIX_N %d\n", nfx);

  {
    int p;

    for (p = 0; SETUP_PATCH[p]; p++)
      for (i = 0; i < s_nsl; i++)
        if (!strcmp (s_sl_name[i], SETUP_PATCH[p]))
          (void)printf ("# define S8S_%s 0x%x\n", s_sl_name[i], s_sl_off[i]);
  }

  {
    int p;

    for (p = 0; DECOMP_PATCH[p]; p++)
      for (i = 0; i < nsl; i++)
        if (!strcmp (sl_name[i], DECOMP_PATCH[p]))
          (void)printf ("# define S8D_%s 0x%x\n", sl_name[i], sl_off[i]);
  }

  (void)printf ("\n#endif\n");

  return 0;
}

/******************************************************************************/
