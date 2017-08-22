/**
 * @file   zingzong.c
 * @data   2017-06-06
 * @author Benjamin Gerard
 * @brief  a simple quartet music player.
 *
 * ----------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

static const char copyright[] = \
  "Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR";
static const char license[] = \
  "Licensed under MIT license";
static const char bugreport[] = \
  "Report bugs to <https://github.com/benjihan/zingzong/issues>";

#include "zz_private.h"

/* ----------------------------------------------------------------------
 * Includes
 * ---------------------------------------------------------------------- */

/* stdc */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <libgen.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

/* libs */
#ifndef NO_AO
# include <ao/ao.h>                     /* Xiph ao */
#endif

/* ----------------------------------------------------------------------
 * Package info
 * ---------------------------------------------------------------------- */

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "zingzong"
#endif

#ifndef PACKAGE_VERSION
#error PACKAGE_VERSION should be defined
#endif

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

/* ----------------------------------------------------------------------
 * Globals
 * ---------------------------------------------------------------------- */

char me[] = PACKAGE_NAME;

enum {
  /* First (0) is default */
#ifndef NO_AO
  OUT_IS_LIVE, OUT_IS_WAVE,
#endif
  OUT_IS_STDOUT, OUT_IS_NULL,
};

/* Options */
static int opt_splrate=SPR_DEF, opt_mixerid=-1, opt_tickrate=200;
static int8_t opt_mute, opt_help, opt_outtype;
static char * opt_length, * opt_output;
static play_t play;

/* ----------------------------------------------------------------------
 * Usage and version
 * ----------------------------------------------------------------------
 */

/**
 * Print usage message.
 */
static void print_usage(int level)
{
  int i;
  printf (
    "Usage: zingzong [OPTIONS] <song.4v> [<inst.set>]" "\n"
    "       zingzong [OPTIONS] <music.4q>"  "\n"
    "\n"
    "  A simple /|\\ Atari ST /|\\ quartet music file player\n"
    "\n"
    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz).\n"
    " -r --rate=[R,]HZ   Set re-sampling method and rate (%s,%uK).\n",
    zz_default_mixer->name, SPR_DEF/1000u);

  if (!level)
    puts("                    Try `-hh' to print the list of [R]esampler.");
  else
    for (i=0; zz_mixers[i]; ++i) {
      const  mixer_t * const m = zz_mixers[i];
      printf("%6s `%s' %s %s.\n",
             i?"":" R :=", m->name,"............."+strlen(m->name),m->desc);
    }

  puts(
    " -l --length=TIME   Set play time.\n"
    " -m --mute=ABCD     Mute selected channels (bit-field or string).\n"
    " -o --output=URI    Set output file name (-w or -c).\n"
    " -c --stdout        Output raw PCM to stdout or file (native 16-bit).\n"
    " -n --null          Output to the void.\n"
#ifndef NO_AO
    " -w --wav           Generated a .wav file.\n"
#endif
    );

  puts(
    !level ?
    "Try `-hh' for more details on OUTPUT and TIME." :

    "OUTPUT:\n"
#ifndef NO_AO
    " Without -o/--output the .wav file is the song file stripped of its\n"
    " path with its extension replaced by .wav.\n"
#endif

#ifdef NO_AO
    "\n"
    "IMPORTANT:\n"
    " This version of zingzong has been built without libao support.\n"
    " Therefore it can not produce audio output nor RIFF .wav file.\n"
#endif

    "\n"
    "TIME:\n"
    "  * pure integer number to represent a number of ticks\n"
    "  * comma `,' to separate seconds and milliseconds\n"
    "  * `h' to suffix hours; `m' to suffix minutes\n"
    " If time is not set the player tries to auto-detect the music duration.\n"
    " However a number of musics are going into unnecessary loops which make\n"
    " it harder to properly detect. Detection threshold is set to 30 minutes'.\n"
    " If time is set to `0` or `inf` the player will run forever.");
  puts("");
  puts(copyright);
  puts(license);
  puts(bugreport);
}

/**
 * Print version and copyright message.
 */
static void print_version(void)
{
  puts(PACKAGE_STRING "\n");
  puts(copyright);
  puts(license);
}


/* ----------------------------------------------------------------------
 * Argument parsing functions
 * ----------------------------------------------------------------------
 */

static const char * tickstr(uint_t ticks, uint_t rate)
{
  static char s[80];
  const int max = sizeof(s)-1;
  int i=0, l=1;
  uint_t ms;

  if (!ticks)
    return "infinity";
  ms = ticks * 1000u / rate;
  if (ms >= 3600000u) {
    i += snprintf(s+i,max-i,"%uh", ms/3600000u);
    ms %= 3600000u;
    l = 2;
  }
  if (i > 0 || ms >= 60000) {
    i += snprintf(s+i,max-i,"%0*u'",l,ms/60000u);
    ms %= 60000u;
    l = 2;
  }
  if (!i || ms) {
    uint_t sec = ms / 1000u;
    ms %= 1000u;
    if (ms)
      while (ms < 100) ms *= 10u;
    i += snprintf(s+i,max-i,"%0*u,%03u\"", l, sec, ms);

  }

  i += snprintf(s+i,max-i," (+%u ticks@%uhz)", ticks, rate);
  s[i] = 0;
  return s;
}
#ifndef NO_AO

static char * fileext(char * base)
{
  char * ext = strrchr(base,'.');
  return ext && ext != base
    ? ext
    : base + strlen(base)
    ;
}

/**
 * Create .wav filename from another filename
 */
static int wav_filename(str_t ** pwavname, char * sngname)
{
  char *leaf=basename(sngname), *ext=fileext(leaf);
  str_t * str;
  int l = ext - leaf;

  *pwavname = str = zz_stralloc(l+5);
  if (!str)
    return E_SYS;
  zz_memcpy(ZZSTR(str),leaf,l);
  zz_memcpy(ZZSTR(str)+l,".wav",5);
  return E_OK;
}

#endif

static uint_t mystrtoul(char **s, const int base)
{
  uint_t v; char * errp;

  errno = 0;
  if (!isdigit((int)**s))
    return -1;
  v = strtoul(*s,&errp,base);
  if (errno)
    return -1;
  *s = errp;
  return v;
}

/**
 * Parse time argument (a bit permissive)
 */
static int time_parse(uint_t * pticks, char * time)
{
  int i, w = 1;
  uint_t ticks = 0;
  char *s = time;

  if (!*s) {
    s = "?";                            /* trigger an error */
    goto done;
  }

  if (!strcasecmp(time,"inf")) {
    ticks = 0; s += 3; goto done;
  }

  for (i=0; *s && i<3; ++i) {
    uint_t v = mystrtoul(&s,10);
    if (v == (uint_t)-1)
      s = "?";
    switch (*s) {
    case 0:
      if (!i)
        ticks = v;
      else
        ticks += v * opt_tickrate * w;
      goto done;

    case ',': case '.':
      ++s;
      ticks += v * opt_tickrate;
      v = mystrtoul(&s,10);
      if (v == (uint_t)-1)
        s = "?";
      else if (v) {
        while (v > 1000) v /= 10u;
        while (v <  100) v *= 10u;
        ticks += (v * opt_tickrate) / 1000u;
      }
      goto done;

    case 'h':
      if (i>0) goto done;
      ticks = v * opt_tickrate * 3600u;
      w = 60u;
      ++s;
      break;

    case 'm': case '\'':
      if (i>1) goto done;
      ticks += v * opt_tickrate * 60u;
      w = 1u;
      ++s;
      break;

    default:
      goto done;
    }
  }

 done:
  if (*s) {
    emsg("invalid argument -- length=%s\n", time);
    return E_ARG;
  }
  *pticks = ticks;
  return E_OK;
}

/**
 * Parse integer argument with range check.
 */
static int uint_arg(char * arg, const char * name,
                    uint_t min, uint_t max, int base)
{
  uint_t v;
  char * s = arg;

  v = mystrtoul(&s,base);
  if (v == (uint_t)-1) {
    emsg("invalid number -- %s=%s\n", name, arg);
  } else {
    if (*s == 'k') {
      v *= 1000u;
      ++s;
    }
    if (*s) {
      emsg("invalid number -- %s=%s\n", name, arg);
      v = (uint_t) -1;
    } else if  (v < min || (max && v > max)) {
      emsg("out of range -- %s=%s\n", name, arg);
      v = (uint_t) -1;
    }
  }
  return v;
}

static char * xtrbrk(char * s, const char * tok)
{
  for ( ;*s && !strchr(tok,*s); ++s)
    ;
  return s;
}

/**
 * @retval 0 no match
 * @retval 1 perfect match
 * @retval 2 partial match
 */

static int modecmp(const char * mix, char * arg,  char ** pend)
{
  const char *eng, *qua;
  char *brk, *end;
  int len, elen, qlen, res = 0;

  if (!pend)
    pend = &end;

  /* Split mix into "engine:quality" */
  qua = strchr(eng = mix,':');
  if (qua)
    elen = qua++ - eng;
  else {
    elen = 0;
    qua = mix;
  }
  qlen = strlen(qua);

  brk = xtrbrk(arg,":,");
  len = brk-arg;
  if (*brk == ':') {
    /* [arg:len] = engine */
    if (len > elen || strncasecmp(eng,arg,len))
      return 0;
    res = 2 - (len == elen);   /* perfect if both have same len */
    brk = xtrbrk(arg=brk+1,",");
    len = brk-arg;
  }
  /* [arg:len] = quality */
  if (len > qlen || strncasecmp(qua,arg,len))
    return 0;
  assert (*brk == 0 || *brk == ',');
  *pend = brk + (*brk == ',');
  res |= 2 - (len == qlen);
  return res;
}


/**
 * @return mixer id
 * @retval 0   not found
 * @retval >0  mixer_id+1
 * @retval <0  -(mixer_id+1)
 */
static int find_mixer(char * arg, char ** pend)
{
  int i,f;
  const mixer_t * m;
  /* Get re-sampling mode */
  for (i=0, f=-1; !!(m = zz_mixers[i]); ++i) {
    int res = modecmp(m->name, arg, pend);

    if (res == 1) {
      /* prefect match */
      f = i; m = 0;
      break;
    } else if (res) {
      /* partial match */
      if (f != -1)
        break;
      f = i;
    }
  }

  if (f < 0)
    f = 0;                              /* not found */
  else if (m)
    f = -(f+1);                         /* ambiguous */
  else
    f = f+1;                            /* found */

  dmsg("Find mixer -- %d -- [%s]\n",f, pend ? *pend : "(nil)");

  return f;
}


/**
 * Parse -r,--rate=[M:Q,]Hz.
 */
static int uint_spr(char * arg, const char * name, int * prate, int * pmixer)
{
  int rate=SPR_DEF;
  char * end = arg;

  if (isalpha(arg[0])) {
    int f;
    if (f = find_mixer(arg, &end), f <= 0) {
      emsg("%s sampling method -- %s=%s\n",
           !f?"invalid":"ambiguous", name, arg);
    } else {
      *pmixer = f-1;
    }
  }

  if (end && *end)
    rate = uint_arg(end, name, SPR_MIN, SPR_MAX, 10);
  if (rate != -1)
    *prate = rate;
  return rate;
}

/**
 * Parse -m/--mute option argument. Either a string := [A-D]\+ or an
 * integer {0..15}
 */
static int uint_mute(char * arg, char * name)
{
  int c = tolower(*arg), mute;
  if (c >= 'a' && c <= 'd') {
    char *s = arg;
    mute = 0;
    do {
      mute |= 1 << (c-'a');
    } while (c = tolower(*++s), (c >= 'a' && c <= 'd') );
    if (c) {
      emsg("invalid channels -- %s=%s\n",name,arg);
      mute = -1;
    }
  } else {
    mute = uint_arg(arg,name,0,15,0);
  }
  return mute;
}

static int too_few_arguments(void)
{
  emsg("too few arguments. Try --help.\n");
  return E_ARG;
}

static int too_many_arguments(void)
{
  emsg("too many arguments. Try --help.\n");
  return E_ARG;
}

/* ----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------
 */

#define RETURN(V) do { ecode = V; goto error_exit; } while(0)

#ifndef NO_AO
# define WAVOPT "w"  /* libao adds support for wav file generation */
#else
# define WAVOPT
#endif

int main(int argc, char *argv[])
{
  static char sopts[] = "hV" WAVOPT "cno:" "r:t:l:m:";
  static struct option lopts[] = {
    { "help",    0, 0, 'h' },
    { "usage",   0, 0, 'h' },
    { "version", 0, 0, 'V' },
    /**/
#ifndef NO_AO
    { "wav",     0, 0, 'w' },
#endif
    { "output",  1, 0, 'o' },
    { "stdout",  0, 0, 'c' },
    { "null",    0, 0, 'n' },
    { "tick=",   1, 0, 't' },
    { "rate=",   1, 0, 'r' },
    { "length=", 1, 0, 'l' },
    { "mute=",   1, 0, 'm' },
    { 0 }
  };
  int c, ecode=E_ERR;
  vfs_t inp = 0;
  play_t * const P = &play;
  str_t  * outuri = 0;
  char * songuri = 0, * vseturi = 0;

  assert( sizeof(songhd_t) ==  16);
  assert( sizeof(sequ_t)   ==  12);

  opterr = 0;
  while ((c = getopt_long (argc, argv, sopts, lopts, 0)) != -1) {
    switch (c) {
    case 'h': opt_help++; break;
    case 'V': print_version(); return E_OK;
#ifndef NO_AO
    case 'w': opt_outtype = OUT_IS_WAVE; break;
#endif
    case 'o': opt_output = optarg; break;
    case 'n': opt_outtype = OUT_IS_NULL; break;
    case 'c': opt_outtype = OUT_IS_STDOUT; break;
    case 'l': opt_length = optarg; break;
    case 'r':
      if (-1 == uint_spr(optarg, "rate", &opt_splrate, &opt_mixerid))
        RETURN (E_ARG);
      break;
    case 't':
      if (-1 == (opt_tickrate = uint_arg(optarg,"tick",200/4,200*4,0)))
        RETURN (E_ARG);
      break;
    case 'm':
      if (-1 == (opt_mute = uint_mute(optarg,"mute")))
        RETURN (E_ARG);
      break;
    case 0: break;
    case '?':
      if (optopt) {
        if (isgraph(optopt))
          emsg("unknown option -- `%c'\n",optopt);
        else
          emsg("unknown option -- `\\x%02X'\n",optopt);
      } else if (optind-1 > 0 && optind-1 < argc) {
        emsg("unknown option -- `%s'\n", argv[optind-1]+2);

      }
      RETURN (E_ARG);
    default:
      emsg("unexpected option -- `%c' (%d)\n",
           isgraph(c)?c:'.', c);
      assert(!"should not happen");
      RETURN (E_666);
    }
  }

  if (opt_help) {
    print_usage(opt_help > 1);
    return E_OK;
  }

  if (optind >= argc)
    RETURN (too_few_arguments());

  zz_memclr(P,sizeof(*P));

  if (opt_mixerid < 0)
    opt_mixerid = find_mixer((char *)zz_default_mixer->name,0) - 1;
  if (opt_mixerid < 0)
    opt_mixerid = 0;

  P->mixer      = zz_mixers[opt_mixerid];
  P->spr        = opt_splrate;
  P->rate       = opt_tickrate;
  P->max_ticks  = MAX_DETECT * P->rate;
  P->end_detect = !opt_length;
  if (!P->end_detect) {
    ecode = time_parse(&P->max_ticks, opt_length);
    if (ecode)
      goto error_exit;
  }

  songuri = argv[optind++];
  if (optind<argc)
    vseturi = argv[optind++];

  ecode = zz_load(P, songuri, vseturi);
  if (ecode == E_ARG)
    RETURN (too_many_arguments());
  if (ecode)
    goto error_exit;

  optind -= vseturi && P->songuri == P->vseturi;
  if (optind < argc)
    RETURN(too_many_arguments());       /* or we could just warn */

  /* ----------------------------------------
   *  Output
   * ---------------------------------------- */

  switch (opt_outtype) {

#ifndef NO_AO
  case OUT_IS_WAVE:
    ecode = !opt_output
      ? wav_filename(&outuri, ZZSTR(P->songuri))
      : ! zz_strset(outuri,opt_output) ? E_SYS : E_OK
      ;
    if (ecode)
      goto error_exit;
  case OUT_IS_LIVE:
    P->out = out_ao_open(opt_splrate, outuri ? ZZSTR(outuri) : 0);
    break;
#endif

  case OUT_IS_STDOUT:
    P->out = out_raw_open(opt_splrate, opt_output ? opt_output : "stdout:");
    break;

  default:
    assert(!"unexpected output type");

  case OUT_IS_NULL:
    P->out = out_raw_open(opt_splrate,"null:");
    break;
  }

  if (!P->out)
    RETURN (E_OUT);

  dmsg("Output via %s to \"%s\"\n", P->out->name, P->out->uri);
  P->spr = P->out->hz;
  P->muted_voices = opt_mute;

  imsg("Zing that zong\n"
       " with the \"%s\" mixer at %uhz\n"
       " for %s%s\n"
       " via \"%s\"\n"
       "vset: \"%s\" (%ukHz, %u sound)\n"
       "song: \"%s\" (%ukHz, %u, %u, %u:%u)\n",
       P->mixer->name,
       P->spr,
       P->end_detect ? "max " : "",
       tickstr(P->max_ticks, P->rate), P->out->uri,
       basename(ZZSTR(P->vseturi)), P->vset.khz, P->vset.nbi,
       basename(ZZSTR(P->songuri)), P->song.khz, P->song.barm,
       P->song.tempo, P->song.sigm, P->song.sigd);

  if (P->info.comment && *P->info.comment)
    imsg("Comment:\n~~~~~\n%s\n~~~~~\n",P->info.comment);

#ifndef NO_AO
  if (outuri)
    imsg("wave: \"%s\"\n", ZZSTR(outuri));
#endif

  if (!ecode)
    ecode = zz_init(P);

  if (P->max_ticks) {
    ecode = zz_measure(P);
    if (!ecode && P->end_detect)
      imsg("duration: %s\n", tickstr(P->max_ticks, P->rate));
  }

  if (!ecode)
    ecode = zz_play(P);

error_exit:
  /* clean exit */
  zz_strfree(&outuri);
  vfs_del(&inp);
  if (c = zz_kill(P), (c && !ecode))
    ecode = c;
  ensure_newline();
  return ecode;
}
