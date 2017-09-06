/**
 * @file   zingzong.c
 * @data   2017-06-06
 * @author Benjamin Gerard
 * @brief  a simple Microdeal quartet music player.
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

#define ZZ_DBG_PREFIX "(cli) "
#include "zingzong.h"
#include "zz_def.h"

#ifndef MAX_DETECT
# define MAX_DETECT 1800    /* maximum seconds for length detection */
#endif

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


#ifdef WIN32
#ifdef __MINGW32__
# include <libgen.h>     /* no GNU version of basename() with mingw */
#endif
#include <io.h>
#include <fcntl.h>
#endif

typedef uint_fast32_t uint_t;

/* /\* libs *\/ */
/* #ifndef NO_AO */
/* # include <ao/ao.h>                     /\* Xiph ao *\/ */
/* #endif */

/* ----------------------------------------------------------------------
 * Globals
 * ---------------------------------------------------------------------- */

ZZ_EXTERN_C
zz_vfs_dri_t zz_file_vfs(void);         /* vfs_file.c */

static char me[] = "zingzong";

enum {
  /* First (0) is default */
#ifndef NO_AO
  OUT_IS_LIVE, OUT_IS_WAVE,
#endif
  OUT_IS_STDOUT, OUT_IS_NULL,
};

/* Options */

static int opt_splrate = SPR_DEF, opt_tickrate=200;
static int opt_mixerid = ZZ_DEFAULT_MIXER;
static int8_t opt_mute, opt_help, opt_outtype;
static char * opt_length, * opt_output;

/* ----------------------------------------------------------------------
 * Message and logging
 * ----------------------------------------------------------------------
 */

static int8_t newline = 1;              /* newline tracker */
ZZ_EXTERN_C int8_t can_use_std;            /* out_raw.c */

/* Very lazy and very wrong way to handle that. It won't work as soon
 * as stdout ans stderr are not the same file (or tty).
 */
static void set_newline(const char * fmt)
{
  if (*fmt)
    newline = fmt[strlen(fmt)-1] == '\n';
}

/* Determine log FILE from log level. */
static FILE * log_file(int log)
{
  int8_t fd = 1 + ( log <= ZZ_LOG_WRN ); /* preferred channel */
  if ( ! (fd & can_use_std) ) fd ^= 3;   /* alternate channel */
  return  (fd & can_use_std)
    ? fd == 1 ? stdout : stderr
    : 0                                 /* should not happen anyway */
    ;
}

static void log_newline(int log)
{
  if (!newline) {
    FILE * out = log_file(log);
    if (out) putc('\n',out);
    newline = 1;
  }
}

static int errcnt;

static void mylog(zz_u8_t log, void * user, const char * fmt, va_list list)
{
  FILE * out = log_file(log);

  errcnt += log == ZZ_LOG_ERR;

  if (out) {
    if (log <= ZZ_LOG_WRN) {
      fprintf(out, "\n%s: "+newline, me);
      newline = 0;
    }
    vfprintf(out, fmt, list);
    set_newline(fmt);
    fflush(out);
  }
}

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
  const char * name="?", * desc;
  zz_mixer_enum(ZZ_DEFAULT_MIXER,&name,&desc);

  printf (
    "Usage: zingzong [OPTIONS] <song.4v> [<inst.set>]" "\n"
    "       zingzong [OPTIONS] <music.4q>"  "\n"
    "\n"
    "  A Microdeal quartet music file player\n"
    "\n"
#ifndef NDEBUG
    "  -------> /!\\ DEBUG BUILD /!\\ <-------\n"
    "\n"
#endif

    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz).\n"
    " -r --rate=[R,]HZ   Set re-sampling method and rate (%s,%uK).\n",
    name, SPR_DEF/1000u);

  if (!level)
    puts("                    Try `-hh' to print the list of [R]esampler.");
  else
    for (i=0; i == zz_mixer_enum(i,&name,&desc); ++i) {
      printf("%6s `%s' %s %s.\n",
             i?"":" R :=", name,"............."+strlen(name),desc);
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
    " Options `-n/--null`,'-c/--stdout' and `-w/--wav` are used to set the\n"
    " output type. The last one is used. Without it the default output type\n"
    " is used which should be playing sound via the default or configured\n"
    " libao driver.\n"
    "\n"
    " The `-o/--output` option specify the output depending on the output\n"
    " type.\n"
    "\n"
    " -n/--nul`    output is ignored\n"
    " -c/--stdout  output to the specified file instead of `stdout`.\n"
    " -w/--wav     unless set output is a file based on song filename.\n"

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
#ifndef NDEBUG
  printf("%s (DEBUG BUILD)\n",zz_version());
#else
  printf("%s\n",zz_version());
#endif
  puts(copyright);
  puts(license);
}

/* ----------------------------------------------------------------------
 * Argument parsing functions
 * ----------------------------------------------------------------------
 */

#ifndef NO_LOG
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
    i += snprintf(s+i,max-i,"%hu", HU(ms/3600000u));
    ms %= 3600000u;
    l = 2;
  }
  if (i > 0 || ms >= 60000) {
    i += snprintf(s+i,max-i,"%0*hu'",l,HU(ms/60000u));
    ms %= 60000u;
    l = 2;
  }
  if (!i || ms) {
    uint_t sec = ms / 1000u;
    ms %= 1000u;
    if (ms)
      while (ms < 100) ms *= 10u;
    i += snprintf(s+i,max-i,"%0*hu,%03hu\"", l, HU(sec), HU(ms));

  }

  i += snprintf(s+i,max-i," (+%lu ticks@%huhz)", LU(ticks), HU(rate));
  s[i] = 0;
  return s;
}

#endif

#ifndef NO_AO

static char * fileext(char * s)
{
  int i, l = strlen(s);
  for ( i=l-2; i>=1; --i )
    if (s[i] == '.') { l = i; break; }
  dmsg("extension of \"%s\" is \"%s\"\n", s, s+l);
  return s+l;
}

/**
 * Create .wav filename from another filename
 */
static zz_err_t
wav_filename(char ** pwavname, char * sngname)
{
  zz_err_t ecode;
  char *leaf = basename(sngname), *ext = fileext(leaf);
  const int l = ext - leaf;

  ecode = zz_malloc(pwavname, l+8);
  if (ecode == ZZ_OK) {
    char *str  = *pwavname;
    memcpy(str,leaf,l);
    memcpy(str+l,".wav",5);
    dmsg("wav path: \"%s\"\n",str);
  } else {
    emsg("(%d) %s -- %s\n", errno, strerror(errno),sngname);
  }
  return ecode;
}

static zz_err_t
wav_dupname(char ** pwavname, char * outname)
{
  if ( ! ( *pwavname = strdup(outname) ) ) {
    emsg("(%d) %s -- %s\n",errno, strerror(errno),outname);
    return ZZ_ESYS;
  }
  return ZZ_OK;
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
    return ZZ_EARG;
  }
  *pticks = ticks;
  return ZZ_OK;
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
  zz_assert(*brk == 0 || *brk == ',');
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

  /* Get re-sampling mode */
  for (i=0, f=-1; ; ++i) {
    const char * name, * desc;
    int res, id = zz_mixer_enum(i,&name,&desc);

    if (id == ZZ_DEFAULT_MIXER) {
      i = ZZ_DEFAULT_MIXER;
      break;
    }

    res = modecmp(name, arg, pend);
    dmsg("testing mixer#%i(%i) => (%i) \"%s\" == \"%s\"\n",
         i, id, res, name, arg);

    if (res == 1) {
      /* prefect match */
      f = i; i = ZZ_DEFAULT_MIXER;
      dmsg("perfect match: %i \"%s\" == \"%s\"\n",f,name,arg);
      break;
    } else if (res) {
      /* partial match */
      dmsg("partial match: %i \"%s\" == \"%s\"\n",f,name,arg);
      if (f != -1)
        break;
      f = i;
    }
  }

  if (f < 0)
    f = 0;                              /* not found */
  else if (i != ZZ_DEFAULT_MIXER)
    f = -(f+1);                         /* ambiguous */
  else
    f = f+1;                            /* found */

  return f;
}


/**
 * Parse -r,--rate=[M:Q,]Hz.
 */
static int uint_spr(char * arg, const char * name, int * prate, int * pmixer)
{
  int rate = SPR_DEF;
  char * end = arg;

  if (isalpha(arg[0])) {
    int f = f = find_mixer(arg, &end);
    dmsg("find mixer in \"%s\" -> %i \"%s\"\n", arg, f, end);
    if (f <= 0) {
      emsg("%s sampling method -- %s=%s\n",
           !f?"invalid":"ambiguous", name, arg);
    } else {
      dmsg("set mixer id --  %i\n", f-1);
      *pmixer = f-1;
    }
  }

  if (end && *end) {
    rate = uint_arg(end, name, SPR_MIN, SPR_MAX, 10);
    dmsg("set sampling rate -- %i\n", rate);
  }
  if (rate != -1)
    *prate = rate;

  zz_assert( *pmixer >= 0 );
  zz_assert( *prate  >= SPR_MIN );
  zz_assert( *prate  <= SPR_MAX );

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

static zz_err_t too_few_arguments(void)
{
  emsg("too few arguments. Try --help.\n");
  return ZZ_EARG;
}

static zz_err_t too_many_arguments(void)
{
  emsg("too many arguments. Try --help.\n");
  return ZZ_EARG;
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
  int c, ecode=ZZ_ERR, ecode2;
  char * wavuri = 0;
  char * songuri = 0, * vseturi = 0;
  zz_play_t P = 0;
  zz_u32_t max_ticks = 0;
  zz_u8_t format;
  zz_out_t * out = 0;

  /* Install logger */
  zz_log_fun(mylog,0);

  opterr = 0;
  while ((c = getopt_long (argc, argv, sopts, lopts, 0)) != -1) {
    switch (c) {
    case 'h': opt_help++; break;
    case 'V': print_version(); return ZZ_OK;
#ifndef NO_AO
    case 'w': opt_outtype = OUT_IS_WAVE; break;
#endif
    case 'o': opt_output = optarg; break;
    case 'n': opt_outtype = OUT_IS_NULL; break;
    case 'c': opt_outtype = OUT_IS_STDOUT; break;
    case 'l': opt_length = optarg; break;
    case 'r':
      if (-1 == uint_spr(optarg, "rate", &opt_splrate, &opt_mixerid))
        RETURN (ZZ_EARG);
      break;
    case 't':
      if (-1 == (opt_tickrate = uint_arg(optarg,"tick",RATE_MIN,200*4,0)))
        RETURN (ZZ_EARG);
      break;
    case 'm':
      if (-1 == (opt_mute = uint_mute(optarg,"mute")))
        RETURN (ZZ_EARG);
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
      RETURN (ZZ_EARG);
    default:
      emsg("unexpected option -- `%c' (%d)\n",
           isgraph(c)?c:'.', c);
      zz_assert(!"should not happen");
      RETURN (ZZ_666);
    }
  }

  if (opt_help) {
    print_usage(opt_help > 1);
    return ZZ_OK;
  }

  if (opt_length) {
    ecode = time_parse(&max_ticks, opt_length);
    if (ecode)
      goto error_exit;
  } else {
    max_ticks = MAX_DETECT * opt_tickrate;
  }

  if (optind >= argc)
    RETURN (too_few_arguments());

  songuri = argv[optind++];
  if (optind < argc)
    vseturi = argv[optind++];

  if (1) {
    const char * name = "?", * desc;
    opt_mixerid = zz_mixer_enum( opt_mixerid,&name,&desc);
    dmsg("requested mixer -- %d:%s (%s)\n", opt_mixerid, name, desc);
    zz_assert( opt_mixerid >= 0 );
  }

  ecode = zz_vfs_add(zz_file_vfs());
  if (ecode)
    goto error_exit;

  ecode = zz_new(&P);
  if (ecode)
    goto error_exit;

  ecode = zz_load(P, songuri, vseturi, &format);
  if (ecode)
    goto error_exit;
  optind -= vseturi && format >= ZZ_FORMAT_BUNDLE;
  if (optind < argc)
    RETURN(too_many_arguments());       /* or we could just warn */

  /* ----------------------------------------
   *  Output
   * ---------------------------------------- */

  switch (opt_outtype) {

#ifndef NO_AO
  case OUT_IS_WAVE:
    ecode = !opt_output
      ? wav_filename(&wavuri, songuri)
      : wav_dupname(&wavuri,opt_output)
      ;
    if (ecode)
      goto error_exit;
  case OUT_IS_LIVE:
    out = out_ao_open(opt_splrate, wavuri);
    break;
#endif

  case OUT_IS_STDOUT:
    out = out_raw_open(opt_splrate, opt_output ? opt_output : "stdout:");
    break;

  default:
    zz_assert(!"unexpected output type");

  case OUT_IS_NULL:
    out = out_raw_open(opt_splrate,"null:");
    break;
  }

  if (!out)
    RETURN (ZZ_EOUT);

  ecode = zz_setup(P, opt_mixerid,
                   opt_splrate, opt_tickrate,
                   max_ticks, !opt_length);
  if (ecode)
    goto error_exit;

  dmsg("Output via %s to \"%s\"\n", out->name, out->uri);

  /*
  imsg("Zing that zong\n"
       " with the \"%s\" mixer at %uhz\n"
       " for %s%s\n"
       " via \"%s\"\n"
       "vset: \"%s\" (%ukHz, %u sound)\n"
       "song: \"%s\" (%ukHz, %u, %u, %u:%u)\n",
       mixer_name,
       out->hz,
       P->end_detect ? "max " : "",
       tickstr(P->max_ticks, P->rate), P->out->uri,
       basename(ZZSTR(P->vseturi)), P->vset.khz, P->vset.nbi,
       basename(ZZSTR(P->songuri)), P->song.khz, P->song.barm,
       P->song.tempo, P->song.sigm, P->song.sigd);
  if (P->info.comment && *P->info.comment)
    imsg("Comment:\n~~~~~\n%s\n~~~~~\n",P->info.comment);
  */

#ifndef NO_AO
  if (wavuri)
    imsg("wave: \"%s\"\n", wavuri);
#endif

  if (!ecode)
    ecode = zz_init(P);

  if (max_ticks) {
    zz_u32_t ticks = max_ticks, ms = 0;
    ecode = zz_measure(P, &ticks, &ms);
    if (ecode)
      goto error_exit;
    dmsg("measured: %lu ticks, %lu ms\n", LU(ticks), LU(ms));
    if (ticks)
      imsg("duration: %s\n", tickstr(ticks, opt_tickrate));
  }

  if (!ecode) {
    uint_t sec = (uint_t) -1;
    zz_u16_t n = 0;
    do {
      int16_t * pcm;

      pcm = zz_play(P,&n);
      if (!pcm) {
        ecode = n;
        break;
      }
      if (!n)
        break;

      n <<= 1;
      if (n != out->write(out,pcm,n))
        ecode = ZZ_EOUT;
      else {
        zz_u32_t pos;
        zz_position(P,&pos);
        pos /= 1000u;
        if (pos != sec) {
          sec = pos;
          imsg("\n |> %02u:%02u\r"+newline,
               sec / 60u, sec % 60u );
          newline = 1;
        }
      }
    } while (!ecode);

    if (ecode) {
      emsg("(%d) prematured end (tick: %lu) -- %s\n",
           ecode, LU(zz_position(P,0)), songuri);
    }
  }

error_exit:
  zz_mem_check_close();

  /* clean exit */
  zz_free(&wavuri);
  zz_assert(!wavuri);

  if (out && out->close(out) && !ecode)
    ecode = ZZ_EOUT;

  if (ecode2 = zz_close(P), (ecode2 && !ecode))
    ecode = ecode2;

  zz_del(&P);

  if (ecode && !errcnt)
    emsg("unknown error (%i)\n", ecode);

  log_newline(ZZ_LOG_INF);

#ifndef NDEBUG
  dmsg("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  dmsg("!!! Checking memory allocation on exit !!!\n");
  dmsg("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  if (ZZ_OK == zz_mem_check_close())
    dmsg("-->  Everything looks fine on my side  <--\n");
  else if (ecode == ZZ_OK)
    ecode = ZZ_666;
  dmsg("exit with -- *%d*\n", ecode);
#endif

  return ecode;
}
