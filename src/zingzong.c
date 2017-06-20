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
static const char bugreport[] =                                 \
  "Report bugs to <https://github.com/benjihan/zingzong/issues>";

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if !defined(DEBUG) && !defined(NDEBUG)
#define NDEBUG 1
#endif

#ifndef WITHOUT_LIBAO
/* libao */
# include <ao/ao.h>
# define WAVOPT "wf"
#else
# define WAVOPT
#endif

#ifndef SPR_MIN
#define SPR_MIN 4000                    /* sampling rate minimum */
#endif

#ifndef SPR_MAX
#define SPR_MAX 96000                   /* sampling rate maximum */
#endif

#ifndef SPR_DEF
#define SPR_DEF 48000                   /* sampling rate default */
#endif

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

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "zingzong"
#endif

#ifndef PACKAGE_VERSION
#error PACKAGE_VERSION should be defined
#endif

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

static char me[] = PACKAGE_NAME;
static int opt_sampling = SPR_DEF, opt_tickrate = 200, opt_stdout;

#ifndef WITHOUT_LIBAO
static int opt_wav, opt_force;
#endif

#define VSET_UNROLL   1024u
#define VSET_XSIZE    (20u*VSET_UNROLL)
#define VSET_MAX_SIZE (1<<21) /* arbitrary .set max size */
#define SONG_MAX_SIZE (1<<18) /* arbitrary .4v max size  */
#define INFO_MAX_SIZE 4096    /* arbitrary .4q info max size */
#define MAX_LOOP      67      /* max loop depth (singsong.prg) */

enum {
  E_OK, E_ERR, E_ARG, E_SYS, E_SET, E_SNG, E_OUT, E_PLA,
  E_666 = 66
};

/* ----------------------------------------------------------------------
 *  Messages
 * ----------------------------------------------------------------------
 */

static int newline = 1; /* Lazy newline tracking for message display */

static FILE * stdout_or_stderr(void)
{
  return opt_stdout ? stderr : stdout;
}

static void set_newline(const char * fmt)
{
  if (*fmt)
    newline = fmt[strlen(fmt)-1] == '\n';
}

static void ensure_newline(void)
{
  if (!newline) {
    fputc('\n',stdout_or_stderr());
    newline = 1;
  }
}

static void emsg(const char * fmt, ...)
{
  va_list list;
  va_start(list,fmt);
  fprintf(stderr,"\n%s: "+newline, me);
  newline = 0;
  vfprintf(stderr,fmt,list);
  set_newline(fmt);
  va_end(list);
}

static int sysmsg(const char * obj, const char * alt)
{
  const char * msg = errno ? strerror(errno) : alt;

  if (obj && msg)
    emsg("%s -- %s\n", msg, obj);
  else if (obj)
    emsg("%s\n", obj);
  else if (msg)
    emsg("%s\n", msg);
  return E_SYS;
}

static void wmsg(const char * fmt, ...)
{
  va_list list;
  va_start(list,fmt);
  fprintf(stderr,"\nWARNING: "+newline);
  newline = 0;
  vfprintf(stderr,fmt,list);
  set_newline(fmt);
  fflush(stderr);
  va_end(list);
}

static void dmsg(const char *fmt, ...)
{
#if defined(DEBUG) && (DEBUG == 1)
  FILE * const out = stdout_or_stderr();
  va_list list;
  va_start(list,fmt);
  ensure_newline();
  vfprintf(out,fmt,list);
  set_newline(fmt);
  fflush(out);
  va_end(list);
#endif
}

static void imsg(const char *fmt, ...)
{
  FILE * const out = stdout_or_stderr();
  va_list list;
  va_start(list,fmt);
  ensure_newline();
  vfprintf(out,fmt,list);
  set_newline(fmt);
  fflush(out);
  va_end(list);
}

/* ----------------------------------------------------------------------
 *  Types definitions
 * ----------------------------------------------------------------------
 */

typedef unsigned int   uint_t;
typedef unsigned short ushort_t;
typedef struct info_s  info_t;
typedef struct inst_s  inst_t;
typedef struct vset_s  vset_t;
typedef struct song_s  song_t;
typedef struct sequ_s  sequ_t;
typedef struct bin_s   bin_t;

typedef struct play_s play_t;
typedef struct chan_s chan_t;

struct bin_s {
  uint_t   size;                  /* data size */
  uint_t   xtra;                  /* xtra allocated bytes */
  uint8_t  data[sizeof(int)];
};

struct inst_s {
  int len;                            /* size in byte */
  int lpl;                            /* loop length in byte */
  uint8_t * pcm;                      /* sample address */
};

struct vset_s {
  bin_t * bin;

  int khz;                          /* sampling rate from .set */
  int nbi;                          /* number of instrument [1..20] */
  inst_t inst[20];
};

struct sequ_s {
  uint8_t cmd[2],len[2],stp[4],par[4];
};

struct song_s {
  bin_t * bin;
  uint8_t khz;
  uint8_t barm;
  uint8_t tempo;
  uint8_t sigm;
  uint8_t sigd;
  sequ_t *seq[4];
};

struct info_s {
  bin_t *bin;
  char * comment;
};

struct chan_s {
  sequ_t * seq;                         /* sequence address */
  sequ_t * cur;                         /* next sequence */
  sequ_t * end;                         /* last sequence */

  sequ_t * sq0;                         /* First wait-able command */
  sequ_t * sqN;                         /* Last  wait-able command */

  int loop_level;
  struct {
    sequ_t * seq;                       /* loop point */
    int      cnt;                       /* loop count */
  } loop[MAX_LOOP];

  struct {
    uint8_t * pcm;
    uint_t xtp, stp, idx, len, lpl;
  } mix;

  struct {
    int aim, stp;
  } pta;

  int curi;                            /* current instrument number */
  int wait;                            /* number of tick left to wait */
  int has_loop;                        /* has loop (counter) */
};

struct play_s {
  char *setpath;
  char *sngpath;
#ifndef WITHOUT_LIBAO
  char *wavpath;
#endif

  vset_t vset;
  song_t song;
  info_t info;
  uint_t tick;
  uint_t spr;
  int16_t * mix_buf;
  int pcm_per_tick;
  int has_loop;

  FILE *outfp;

  struct {
    int              id;
#ifndef WITHOUT_LIBAO
    ao_device       *dev;
    ao_info         *info;
    ao_sample_format fmt;
#else
    void *dev;
#endif
  } ao;

  chan_t chan[4];
};

typedef struct songhd songhd_t;
struct songhd {
  uint8_t rate[2];
  uint8_t measure[2];
  uint8_t tempo[2];
  uint8_t timesig[2];
  uint8_t reserved[2*4];
};

static play_t play;


/* ----------------------------------------------------------------------
 * File functions
 * ----------------------------------------------------------------------
 */

static int my_fopen(FILE **pf, const char * path)
{
  dmsg("%s: opening\n",path);
  return !(*pf = fopen(path,"rb"))
    ? sysmsg(path,"open")
    : E_OK
    ;
}

static int my_fclose(FILE **pf, const char * path)
{
  FILE *f = *pf;
  *pf = 0;
  if (f)
    dmsg("%s: closing\n", path);
  return (f && fclose(f))
    ? sysmsg(path,"close")
    : E_OK
    ;
}

static int my_fread(FILE *f, const char * path, void * buf, uint_t n)
{
  dmsg("%s: reading %u bytes\n", path, n);
  return (n != fread(buf,1,n,f))
    ? sysmsg(path,"too short")
    : E_OK
    ;
}

static int my_fsize(FILE *f, const char * path, uint_t *psize)
{
  size_t tell, size;
  assert(f); assert(psize);
  if (-1 == (tell = ftell(f)) ||        /* save position */
      -1 == fseek(f,0,SEEK_END) ||      /* go to end */
      -1 == (size = ftell(f)) ||        /* size = position */
      -1 == fseek(f,tell,SEEK_SET))     /* restore position */
    return sysmsg(path,"file size");
  else if (size < tell || size >= UINT_MAX) {
    emsg("too large (fsize) -- %s\n", path);
    return E_ERR;
  }
  *psize = size - tell;
  dmsg("%s: tell=%u size=%u\n",path,(uint_t)tell,(uint_t)size);
  return E_OK;
}

/* ----------------------------------------------------------------------
 * Load binaries
 * ----------------------------------------------------------------------
 */

static void bin_free(bin_t ** pbin)
{
  if (*pbin) {
    free(*pbin);
    *pbin = 0;
  }
}

static int bin_alloc(bin_t ** pbin, const char * path,
                     uint_t len, uint_t xlen)
{
  bin_t * bin = 0;
  assert(pbin); assert(path);
  *pbin = bin = malloc((uint_t)(intptr_t)(bin->data+len+xlen));
  if (!bin)
    return sysmsg(path,"alloc");
  bin->size = len;
  bin->xtra = xlen;
  return E_OK;
}

static int bin_read(bin_t * bin, const char * path,
                    FILE *f, uint_t off, uint_t len)
{
  return my_fread(f,path,bin->data+off,len);
}

static int bin_load(bin_t ** pbin, const char * path,
                    FILE *f, uint_t len, uint_t xlen, uint_t max)
{
  int ecode;

  if (!len) {
    ecode = my_fsize(f, path, &len);
    if (ecode)
      goto error;
  }
  if (max && len > max) {
    emsg("too large (load > %u) -- %s\n", max, path);
    ecode = E_ERR;
    goto error;
  }
  ecode = bin_alloc(pbin, path, len, xlen);
  if (ecode)
    goto error;
  dmsg("%s: allocated: %u +%u = %u\n", path, len, xlen, len+xlen);
  ecode = bin_read(*pbin, path, f, 0, len);

error:
  if (ecode)
    bin_free(pbin);
  return ecode;
}


/* ----------------------------------------------------------------------
 */

static inline uint_t u16(const uint8_t * const v) {
  return ((uint_t)v[0]<<8) | v[1];
}

static inline uint_t u32(const uint8_t * const v) {
  return (u16(v)<<16) | u16(v+2);
}

/* ----------------------------------------------------------------------
 * quartet song
 * ----------------------------------------------------------------------
 */

static int song_parse(song_t *song, const char * path, FILE *f,
                      uint8_t *hd, uint_t size)
{
  int ecode, k, has_note=0;
  uint_t off;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"in */
  };

  /* Parse song header */
  song->khz   = u16(hd+0);
  song->barm  = u16(hd+2);
  song->tempo = u16(hd+4);
  song->sigm  = hd[6];
  song->sigd  = hd[7];
  dmsg("%s: rate: %ukHz, bar:%u, tempo:%u, signature:%u/%u\n",
       path, song->khz, song->barm, song->tempo, song->sigm, song->sigd);

  /* Load data */
  ecode = bin_load(&song->bin, path, f, size, 0, SONG_MAX_SIZE);
  if (ecode)
    goto error;
  ecode = E_SNG;
  size = (song->bin->size / 12u) * 12u;

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  for (k=off=0; k<4 && off<size; off+=12) {
    sequ_t * const seq = (sequ_t *)(song->bin->data+off);
    uint_t   const cmd = u16(seq->cmd);

    if (!song->seq[k])
      song->seq[k] = seq;               /* Sequence */

    if (1) {
      dmsg("%c %04u %c %04x %08x %04x-%04x\n",
           k+'A',
           seq-song->seq[k],
           isgraph(cmd) ? cmd : '?',
           u16(seq->len),
           u32(seq->stp),
           u16(seq->par),
           u16(seq->par+2));
    }


    switch (cmd) {
    case 'F':                           /* End-Voice */
      if (!has_note) {
        song->seq[k] = (sequ_t *) nullseq;
        dmsg("%c: replaced by default sequence\n");
      }
      has_note = 0;
      ++k;
      break;
    case 'P':                           /* Play-Note */
      has_note = 1;
    case 'S': case 'R':
    case 'l': case 'L': case 'V':
      break;
    default:
      emsg("invalid sequence command $%04x('%c') at %c:%u\n",
           cmd, isgraph(cmd)?cmd:'.', 'A'+k, off);
      goto error;
    }
  }

  if (k != 4) {
    emsg("channel %c is truncated\n", 'A'+k);
    goto error;
  }

  if (off != song->bin->size) {
    wmsg("garbage data after voice sequences -- %u bytes\n",
         song->bin->size - off);
  }

  ecode = E_OK;
error:
  if (ecode)
    bin_free(&song->bin);
  return ecode;
}

static int song_load(song_t *song, const char *path)
{
  uint8_t hd[16];
  FILE *f = 0;
  int ecode = my_fopen(&f, path);
  if (ecode)
    goto error;

  ecode = my_fread(f,path,hd,16);
  if (ecode)
    goto error;
  ecode = song_parse(song,path,f,hd,0);

error:
  my_fclose(&f, path);
  if (ecode) {
    if (ecode != E_SYS)
      ecode = E_SNG;
    bin_free(&song->bin);
  }
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */
static int cmpadr(const void * a, const void * b)
{
  return (*((inst_t **)b))->pcm - (*((inst_t **)a))->pcm;
}

static void prepare_vset(vset_t *vset, const char *path)
{
  const int n = vset->nbi;
  uint8_t * const beg = vset->bin->data;
  uint8_t * const end = vset->bin->data+vset->bin->size+vset->bin->xtra;
  uint8_t * e;
  int i,j,tot,unroll;
  inst_t *pinst[20];

  assert(n>0 && n<=20);

  /* Re-order instrument in memory order. */
  for (i=tot=0; i<n; ++i) {
    pinst[i] = vset->inst+i;
    tot += vset->inst[i].len;
  }
  assert(tot <= end-beg);
  unroll = (end-beg-tot) / n;
  dmsg("%u instrument using %u/%u bytes unroll:%u\n",
       n, tot, end-beg, unroll);
  qsort(pinst, n, sizeof(*pinst), cmpadr);
  for (i=0; i<n; ++i)
    dmsg("%u %p\n", pinst[i] - vset->inst, pinst[i]->pcm);

  for (i=0, e=end; i<n; ++i) {
    inst_t * const inst = pinst[i];
    const uint_t len = inst->len;
    uint8_t * const pcm = e - unroll - len;

    if (len == 0) {
      dmsg("i#%02u is tainted -- ignoring\n", pinst[i]-vset->inst);
      continue;
    }

    dmsg("i#%02u copying %05x to %05x..%05x\n",
         pinst[i]-vset->inst,
         pinst[i]->pcm-beg, pcm-beg, pcm-beg+len-1);

    if (pcm <= pinst[i]->pcm)
      for (j=0; j<len; ++j)
        pcm[j] = 0x80 ^ pinst[i]->pcm[j];
    else
      for (j=len-1; j>=0; --j)
        pcm[j] = 0x80 ^ pinst[i]->pcm[j];
    pinst[i]->pcm = pcm;

    if (!inst->lpl) {
      /* Instrument does not loop -- smooth to middle point */
      int v = (int)(int8_t)pcm[len-1] << 8;
      for (j=0; j<unroll; ++j) {
        v = 3*v >> 2;                   /* v = .75*v */
        pcm[len+j] = 0x00 /* v>>8 */ ;
      }
    } else {
      int lpi = len;
      /* Copy loop */
      for (j=0; j<unroll; ++j) {
        if (lpi >= len)
          lpi -= inst->lpl;
        assert(lpi < len);
        assert(lpi >= len-inst->lpl);
        pcm[len+j] = pcm[lpi++];
      }
    }
    e = pcm;
  }
}

static int vset_parse(vset_t *vset, const char *path, FILE *f,
                      uint8_t *hd, uint_t size)
{
  int ecode = E_SET, i;
  bin_t * bin;

  /* Check sampling rate */
  vset->khz = hd[0];
  dmsg("%s: %u hz\n", path, vset->khz);
  if (vset->khz < 4u || vset->khz > 20u) {
    emsg("sampling rate (%u) out of range -- %s\n", vset->khz, path);
    goto error;
  }

  /* Check instrument number */
  vset->nbi = hd[1]-1;
  dmsg("%s: %u instrument\n", path, vset->nbi);

  if (vset->nbi < 1 || vset->nbi > 20) {
    emsg("instrument count (%U) out of range -- %s\n", vset->nbi, path);
    goto error;
  }

  /* Load data */
  ecode = bin_load(&vset->bin, path, f, size, VSET_XSIZE, VSET_MAX_SIZE);
  if (ecode)
    goto error;

  for (i=0, ecode=E_SET, bin=vset->bin; i<vset->nbi; ++i) {
    int off = u32(&hd[142+4*i]), invalid = 0;
    int o = off - 222 + 8;
    uint_t len, lpl;
    uint8_t * pcm;

    pcm = bin->data+o;
    len = u32(pcm-4);
    lpl = u32(pcm-8);
    if (lpl == 0xFFFFFFFF)
      lpl = 0;

    /* These 2 tests might be a little pedantic but as the format is
     * not very robust It might be a nice safety net. These 2 words
     * should be 0.
     */
    if (len & 0xFFFF) {
      wmsg("I#%02u length LSW is not 0 [08X]\n", i+1, len);
      ++invalid;
    }
    if (lpl & 0xFFFF) {
      wmsg("I#%02u loop-length LSW is not 0 [08X]\n", i+1, lpl);
      ++invalid;
    }
    len >>= 16;
    lpl >>= 16;

    if (!len) {
      wmsg("I#%02u has no data\n", i+1);
      ++invalid;
    }

    if (lpl > len) {
      wmsg("I#%02u loop-length > length [%08X > %08X] \n",
           i+1, lpl, len);
      ++invalid;
    }

    /* Is the sample inside out memory range ? */
    if ( o < 8 || o >= bin->size || (o+len > bin->size) ) {
      wmsg("I#%02u data out of range [%05x..%05x] not in [%05x..%05x] +%u\n",
           i+1, o,o+len, 8,bin->size, o+len-bin->size);
      ++invalid;
    }

    if (invalid) {
      pcm = 0;
      len = lpl = 0;
    } else {
      dmsg("I#%02u \"%7s\" [$%05X:$%05X:$%05X] [$%05X:$%05X:$%05X]\n",
           i+1, hd+2+7*i,
           0, len-lpl, len,
           o, o+len-lpl, o+len);
    }

    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }

  prepare_vset(vset, path);
  ecode = E_OK;
error:
  if (ecode)
    bin_free(&vset->bin);
  return ecode;
}

/**
 * Load .q4 file (after header).
 */
static int q4_load(FILE * f, char * path, play_t * P,
                   uint_t sngsize, uint_t setsize, uint_t infsize)
{
  int ecode;
  uint8_t hd[222];

  assert(ftell(f) == 20);

  if (sngsize < 16 + 12*4) {
    emsg("invalid .4q song size (%u) -- %s", sngsize, path);
    ecode = E_SNG;
    goto error;
  }

  if (setsize < 222) {
    emsg("invalid .4q set size (%u) -- %s", setsize, path);
    ecode = E_SNG;
    goto error;
  }

  ecode = my_fread(f, path, hd, 16);
  if (ecode)
    goto error;
  ecode = song_parse(&play.song, path, f, hd, sngsize-16);
  if (ecode)
    goto error;

  /* Voice set */
  ecode = my_fread(f,path,hd,222);
  if (ecode)
    goto error;
  ecode = vset_parse(&play.vset, path, f, hd, setsize-222);
  if (ecode)
    goto error;

  /* Ignoring the comment for now. */
#if 0
  /* info (ignoring errors) */
  if (!bin_load(&play.info.bin, path, f, infsize, INFO_MAX_SIZE)
      && play.info.bin->size > 1) {
    play.info.comment = (char *) play.info.bin->data;
    dmsg("Comments:\n%s\n", play.info.comment);
  }
#endif

error:
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet player
 * ----------------------------------------------------------------------
 */

#define SET(N) case N: *b++  = (int8_t)(pcm[idx>>16]) << 6; idx += stp
#define ADD(N) case N: *b++ += (int8_t)(pcm[idx>>16]) << 6; idx += stp

static void inline mix_gen(play_t * const P,
                           const int k,
                           int16_t * restrict b, const int n)
{
  uint_t idx, stp;
  const int8_t * const pcm = (const int8_t *)P->chan[k].mix.pcm;

  assert(n > 0 && n <= 8);
  assert(k >= 0 && k < 4);

  idx = P->chan[k].mix.idx;
  stp = P->chan[k].mix.xtp;

  if ( k == 0 ) {
    if (!pcm) {
      memset(b,0,2*8);
      return;
    }
    switch (8-n) {
      SET(0);
      SET(1);
      SET(2);
      SET(3);
      SET(4);
      SET(5);
      SET(6);
      SET(7); break;
    default: assert(0);

    }
  } else {
    if (!pcm) return;
    switch (8-n) {
      ADD(0);
      ADD(1);
      ADD(2);
      ADD(3);
      ADD(4);
      ADD(5);
      ADD(6);
      ADD(7); break;
    default: assert(0);
    }
  }
  stp = P->chan[k].mix.len;
  if (idx >= stp) {
    if (!P->chan[k].mix.lpl)
      P->chan[k].mix.pcm = 0;
    else
      do { } while ( (idx -= P->chan[k].mix.lpl) >= stp );
  }
  P->chan[k].mix.idx = idx;
}

static void mix_set8(play_t * const P, int16_t * b) {
  mix_gen(P, 0, b, 8);
}

static void mix_setN(play_t * const P, int16_t * b, const int n) {
  mix_gen(P, 0, b, n);
}

static void mix_add8(play_t * const P, const int k, int16_t * b) {
  assert( k >= 1 && k < 4 );
  mix_gen(P, k, b, 8);
}

static void mix_addN(play_t * const P, const int k, int16_t * b, const int n) {
  assert( k >= 1 && k < 4 );
  mix_gen(P, k, b, n);
}

static void mix_all(play_t * const P)
{
  int16_t * restrict b;
  int k, n;

  /* re-scale step to our sampling rate
   *
   * GB: we could pre-compute this but once a frame is not to much.
   */
  for (k=0; k<4; ++k) {
    chan_t * const C = P->chan+k;
    C->mix.xtp = C->mix.stp * P->song.khz * 10u / (P->spr/100u);
  }

  /* Mix per block of 8 samples */
  for (b=P->mix_buf, n=P->pcm_per_tick; n >= 8; b += 8, n -= 8) {
    mix_set8(P,    b);
    mix_add8(P, 1, b);
    mix_add8(P, 2, b);
    mix_add8(P, 3, b);
  }

  /* Mix remaining */
  if (n > 0) {
    mix_setN(P,    b, n);
    mix_addN(P, 1, b, n);
    mix_addN(P, 2, b, n);
    mix_addN(P, 3, b, n);
  }
}

static int zz_play(play_t * P)
{
  const uint_t maxtick = 20*60*opt_tickrate;
  int k;
  assert(P);

  /* Setup player */
  memset(P->chan,0,sizeof(P->chan));
  for (k=0; k<4; ++k) {
    chan_t * const C = P->chan+k;
    sequ_t * seq;
    uint_t cmd;
    C->seq = P->song.seq[k];
    C->cur = P->chan[k].seq;
    for ( seq=C->seq; (cmd=u16(seq->cmd)) != 'F' ; ++seq) {
      switch(cmd) {
      case 'P': case 'R': case 'S':
        if (!C->sq0) C->sq0 = seq;
        C->sqN = seq;
      }
    }
    C->end = seq;
    assert(C->sq0);
    assert(C->sqN);
    dmsg("%c: [%05u..%05u..%05u]\n",
         'A'+k, C->sq0-C->seq, C->sqN-C->seq, C->end-C->seq);
  }

  for (;;) {
    int started = 0;

    ++P->tick;
    if (maxtick && P->tick > maxtick) {
      wmsg("unable to detect song end [%s%s%s%s]. Aborting.\n",
           "A"+((P->has_loop>>0)&1),
           "B"+((P->has_loop>>1)&1),
           "C"+((P->has_loop>>2)&1),
           "D"+((P->has_loop>>3)&1)
        );
      break;
    }

    for (k=0; k<4; ++k) {
      chan_t * const C = P->chan+k;
      sequ_t * seq = C->cur;

      /* Portamento */
      if (C->pta.stp) {
        C->mix.stp += C->pta.stp;
        if (C->pta.stp > 0) {
          if (C->mix.stp >= C->pta.aim) {
            C->mix.stp = C->pta.aim;
            C->pta.stp = 0;
          }
        } else if (C->mix.stp <= C->pta.aim) {
          C->mix.stp = C->pta.aim;
          C->pta.stp = 0;
        }
      }

      if (C->wait) --C->wait;
      while (!C->wait) {
        /* This could be an endless loop on empty track but it should
         * have been checked earlier ! */
        uint_t const cmd = u16(seq->cmd);
        uint_t const len = u16(seq->len);
        uint_t const stp = u32(seq->stp);
        uint_t const par = u32(seq->par);
        ++seq;

        dmsg("%c: %04u %c %04x %08x %04x-%04x\n",
             'A'+k,
             seq-1-C->seq,
             isgraph(cmd)?cmd:'?',
             len,stp,par>>16,(par&0xFFFF));


        switch (cmd) {

        case 'F':                       /* End-Voice */
          seq = C->seq;
          P->has_loop |= 1<<k;
          C->has_loop++;
          C->loop_level = 0;            /* Safety net */
          dmsg("%c: [%c%c%c%c] end @%u +%u\n",
               'A'+k,
               ".A"[1&(P->has_loop>>0)],
               ".B"[1&(P->has_loop>>1)],
               ".C"[1&(P->has_loop>>2)],
               ".D"[1&(P->has_loop>>3)],
               P->tick,
               C->has_loop);
          break;

        case 'V':                       /* Voice-Change */
          C->curi = par >> 2;
          break;

        case 'P':                       /* Play-Note */
          if (C->curi < 0 || C->curi >= P->vset.nbi) {
            emsg("@%c[%u]: using invalid instrument number -- %d/%d\n",
                 'A'+k, seq-1-C->seq, C->curi+1,P->vset.nbi);
            return E_SNG;
          }
          if (!P->vset.inst[C->curi].len) {
            emsg("@%c[%u]: using tainted instrument -- I#%02u\n",
                 'A'+k, seq-1-C->seq, C->curi+1);
            return E_SET;
          }

          /* Copy current instrument to channel */
          C->mix.len = P->vset.inst[C->curi].len << 16;
          C->mix.lpl = P->vset.inst[C->curi].lpl << 16;
          C->mix.pcm = P->vset.inst[C->curi].pcm;
          C->mix.idx = 0;

          /* Set step and duration / reset slide */
          C->mix.stp = C->pta.aim = stp;
          C->pta.stp = 0;
          C->wait = len;

          started |= 1<<(k<<3);
          break;

        case 'R':                       /* Rest */
          C->mix.pcm = 0;
          C->wait = len;
          started |= 2<<(k<<3);
          break;

        case 'S':                       /* Slide-to-note */
          C->pta.aim = stp;
          C->wait    = len;
          C->pta.stp = (int32_t)par;
          started |= 4<<(k<<3);
          break;

        case 'l':                       /* Set-Loop-Point */
          if (C->loop_level < MAX_LOOP) {
            const int l = C->loop_level++;
            C->loop[l].seq = seq;
            C->loop[l].cnt = 0;
            dmsg("%c: set loop[%d] point @%u\n",'A'+k, l, seq - C->seq);
          } else {
            emsg("%c off:%u tick:%u -- loop stack overflow\n",
                 'A'+k, (unsigned) (seq-C->seq-1), P->tick);
            return E_PLA;
          }
          break;

        case 'L':                       /* Loop-To-Point */
        {
          int l = C->loop_level - 1;

          if (l < 0) {
            C->loop_level = 1;
            l = 0;
            C->loop[0].cnt = 0;
            C->loop[0].seq = C->seq;
          }

          if (!C->loop[l].cnt) {
            /* Start a new loop unless the loop point is the start of
             * the sequence and the next row is the end.
             *
             * This (not so) effectively removes useless loops on the
             * whole sequence that messed up the song duration.
             */

            /* if (seq >= C->sqN) */
            /*   dmsg("'%c: seq:%05u loop:%05u sq:[%05u..%05u]\n", */
            /*        'A'+k, */
            /*        seq - C->seq, */
            /*        C->loop[l].seq - C->seq, */
            /*        C->sq0 - C->seq, */
            /*        C->sqN - C->seq); */

            if (C->loop[l].seq <= C->sq0 && seq > C->sqN) {
              C->loop[l].cnt = 1;
            } else {
              C->loop[l].cnt = (par >> 16) + 1;
            }
            dmsg("%c: set loop[%d] @%u x%u\n",
                 'A'+k, l, C->loop[l].seq-C->seq,
                 C->loop[l].cnt-1);
          }

          if (--C->loop[l].cnt) {
            dmsg("%c: loop[%d] to @%u rem:%u\n",
                 'A'+k, l, C->loop[l].seq-C->seq,
                 C->loop[l].cnt);
            seq = C->loop[l].seq;
          } else {
            --C->loop_level;
            assert(C->loop_level >= 0);
            dmsg("%c: loop[%d] end\n",'A'+k,C->loop_level);
          }


        } break;

        default:
          emsg("invalid seq-%c command #%04X\n",'A'+k,cmd);
          return E_PLA;
        } /* switch */
      } /* while !wait */
      C->cur = seq;
    } /* per chan */

    if (P->has_loop == 15)
      break;

    /* audio output */
    if (P->ao.dev || P->outfp) {
      int n = P->pcm_per_tick << 1;
#ifndef WITHOUT_LIBAO
      if (P->ao.info && P->ao.info->type == AO_TYPE_LIVE) {
        FILE * const out = stdout_or_stderr();
        const uint_t s = P->tick / opt_tickrate;
        fprintf(out,"\r|> %02u:%02u", s / 60u, s % 60u);
        newline = 0;
        fflush(out);
      }
#endif

      mix_all(P);

      if (P->ao.dev) {
#ifndef WITHOUT_LIBAO
        if (!ao_play(P->ao.dev, (void*)P->mix_buf, n)) {
          emsg("libao: failed to play buffer #%d \"%s\"\n",
               P->ao.id, P->ao.info->short_name);
          return E_OUT;
        }
#endif
      } else if ( n != fwrite(P->mix_buf, 1, n, P->outfp) ) {
        return sysmsg("<stdout>","write");
      }
    }
  } /* loop ! */
  return E_OK;
}

static int zing_zong(play_t * P)
{
  int ecode = E_OUT;

  imsg("zinging that zong at %uhz with %u ticks per second\n"
       "vset: \"%s\" (%ukHz, %u sound)\n"
       "song: \"%s\" (%ukHz, %u, %u, %u:%u)\n",
       opt_sampling, opt_tickrate,
       basename(P->setpath), P->vset.khz, P->vset.nbi,
       basename(P->sngpath), P->song.khz, P->song.barm,
       P->song.tempo, P->song.sigm, P->song.sigd);
#ifndef WITHOUT_LIBAO
  if (P->wavpath)
    imsg("wave: \"%s\"\n", P->wavpath);
#endif

  if (opt_stdout) {
    /* Setup for stdout:
     *
     * Currently for Windows platform only where we have to set the
     * file mode to binary.
     */
    P->spr = opt_sampling;
    P->outfp = stdout;

    dmsg("output to stdout!\n");
#ifdef WIN32
    if (1) {
      int err, fd = fileno(P->outfp);
      dmsg("stdout fd:%d\n", fd);
      if (fd == -1) {
        ecode = sysmsg("<stdout>","fileno");
        goto error;
      }

      errno = 0;
      err = _setmode(fd, _O_BINARY);
      dmsg("setmode returns: %d\n", err);
      if ( err == -1 || errno ) {
        sysmsg("<stdout>","setmode");
      } else {
        dmsg("<stdout> should be in binary mode\n");
      }
    }
#endif
  }

#ifndef WITHOUT_LIBAO
  else {
    /* Setup libao */
    ao_initialize();
    P->ao.fmt.bits = 16;
    P->ao.fmt.rate = opt_sampling;
    P->ao.fmt.channels = 1;
    P->ao.fmt.byte_format = AO_FMT_NATIVE;
    P->ao.id = P->wavpath
      ? ao_driver_id("wav")
      : ao_default_driver_id()
      ;
    P->ao.info = ao_driver_info(P->ao.id);
    if (!P->ao.info) {
      emsg("libao: failed to get driver #%d info\n", P->ao.id);
      goto error;
    }
    P->ao.dev = P->wavpath
      ? ao_open_file(P->ao.id, P->wavpath, 1, &P->ao.fmt, 0)
      : ao_open_live(P->ao.id, &P->ao.fmt, 0)
      ;
    if (!P->ao.dev) {
      emsg("libao: failed to initialize audio driver #%d \"%s\"\n",
           P->ao.id, P->ao.info->short_name);
      goto error;
    }
    dmsg("ao %s device \"%s\" is open\n",
         P->ao.info->type == AO_TYPE_LIVE ? "live" : "file",
         P->ao.info->short_name);
    P->spr = P->ao.fmt.rate;
  }
#endif

  P->pcm_per_tick = (P->spr + (opt_tickrate>>1)) / opt_tickrate;
  P->mix_buf = (int16_t *) malloc ( 2 * P->pcm_per_tick );
  if (!P->mix_buf) {
    ecode = sysmsg("audio", "alloc");
    goto error;
  }

  ecode = zz_play(&play);
error:
  /* Close and ignore closing error if error is already raised */
  if (P->outfp) {
    dmsg("Closing <stdout>\n");
    if (( fflush(P->outfp) || fclose(P->outfp)) && ecode == E_OK)
      ecode = sysmsg("<stdout>","flush/close");
    P->outfp = 0;
  }

#ifndef WITHOUT_LIBAO
  if (!opt_stdout) {
    if (P->ao.dev && !ao_close(P->ao.dev) && ecode == E_OK) {
      sysmsg("audio","close");
      ecode = E_OUT;
    }
    P->ao.dev = 0;
    P->ao.info = 0;
    ao_shutdown();
  }
#endif

  free(P->mix_buf);
  bin_free(&P->vset.bin);
  bin_free(&P->song.bin);
  bin_free(&P->info.bin);
  return ecode;
}

/* ----------------------------------------------------------------------
 * Usage and version
 * ----------------------------------------------------------------------
 */

/**
 * Print usage message.
 */

#ifndef WITHOUT_LIBAO
# define OUTWAV " [output.wav]"
#else
# define OUTWAV ""
#endif

static void print_usage(void)
{
  printf(
    "Usage: zingzong [OPTIONS] <inst.set> <song.4v>" OUTWAV "\n"
    "       zingzong [OPTIONS] <music.q4>" OUTWAV "\n"
    "\n"
    "  A simple /|\\ Atari ST /|\\ quartet music file player\n"
    "\n"
    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz)\n"
    " -r --rate=HZ       Set sampling rate (default is %ukHz)\n"
    " -c --stdout        Output raw sample to stdout\n"
#ifndef WITHOUT_LIBAO
    " -w --wav           Generated a .wav file (implicit if output is set)\n"
    " -f --force         Clobber output .wav file.\n"
    "\n"
    "OUTPUT:\n"
    " If output is set it creates a .wav file of this name (implies `-w').\n"
    " Else with `-w' alone the .wav file is the song file stripped of its\n"
    " path with its extension replaced by .wav.\n"
    " If output exists the program will refuse to create the file unless\n"
    " the -f/--force option is used or it is either empty or a RIFF file."
#else
    "\n"
    "IMPORTANT:\n"
    " This version of zingzong has been built without libao support.\n"
    " Therefore it can not produce audio output nor RIFF .wav file."
#endif
    "\n", SPR_DEF/1000u);
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

#ifndef WITHOUT_LIBAO

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
static int wav_filename(char ** pwavname, char * sngname)
{
  char *leaf=basename(sngname), *ext=fileext(leaf), *wavname;
  int l = ext - leaf;
  *pwavname = wavname = malloc(l+5);
  if (!wavname)
    return sysmsg(leaf,"alloc");
  memcpy(wavname, leaf, l);
  strcpy(wavname+l, ".wav");
  return E_OK;
}

#endif

/**
 * Parse integer argument with range check.
 */
static int uint_arg(char * arg, char * name, uint_t min, uint_t max)
{
  uint_t v = -1; char * errp = 0;

  errno = 0;
  v = strtoul(arg, &errp, 10);
  if (errno) {
    sysmsg("arg","NAN");
    v = (uint_t) -1;
  } else {
    if (errp) {
      if(*errp == 'k') {
        v *= 1000u;
        ++errp;
      }
      if (*errp) {
        emsg("invalid number -- %s=%s\n", name, arg);
        v = (uint_t) -1;
      } else if  (v < min || (max && v > max)) {
        emsg("out range -- %s=%s\n", name, arg);
        v = (uint_t) -1;
      }
    }
  }
  return v;
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

int main(int argc, char *argv[])
{
  static char sopts[] = "hV" WAVOPT "c" "r:t:";
  static struct option lopts[] = {
    { "help",    0, 0, 'h' },
    { "usage",   0, 0, 'h' },
    { "version", 0, 0, 'V' },
    /**/
#ifndef WITHOUT_LIBAO
    { "wav",     0, 0, 'w' },
    { "force",   0, 0, 'f' },
#endif
    { "stdout",  0, 0, 'c' },
    { "tick=",   1, 0, 't' },
    { "rate=",   1, 0, 'r' },
    { 0 }
  };
  int c, can_do_wav=0, ecode=E_ERR;
  FILE * f=0;
  uint8_t hd[222];

  assert( sizeof(songhd_t) ==  16);
  assert( sizeof(sequ_t)   ==  12);

  opterr = 0;
  while ((c = getopt_long (argc, argv, sopts, lopts, 0)) != -1) {
    switch (c) {
    case 'h': print_usage(); return E_OK;
    case 'V': print_version(); return E_OK;
#ifndef WITHOUT_LIBAO
    case 'w': opt_wav = 1; break;
    case 'f': opt_force = 1; break;
#endif
    case 'c': opt_stdout = 1; break;
    case 'r':
      if (-1 == (opt_sampling = uint_arg(optarg,"rate",SPR_MIN,SPR_MAX)))
        RETURN (E_ARG);
      break;
    case 't':
      if (-1 == (opt_tickrate = uint_arg(optarg,"tick",200/4,200*4)))
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

  if (optind >= argc)
    RETURN (too_few_arguments());

#ifndef WITHOUT_LIBAO
  if (opt_wav && opt_stdout) {
    emsg("-w/--wav and -c/--stdout are exclusive\n");
    RETURN (E_ARG);
  }
  can_do_wav = !opt_stdout;
#endif

  memset(&play,0,sizeof(play));

  /* Get the header of the first file to check if its a .4q file */
  play.setpath = play.sngpath = argv[optind++];
  ecode = my_fopen(&f, play.setpath);
  if (ecode)
    goto error_exit;
  ecode = my_fread(f, play.setpath, hd, 20);
  if (ecode)
    goto error_exit;

  /* Check for .q4 "QUARTET" magic id */
  if (!memcmp(hd,"QUARTET",8)) {
    uint_t sngsize = u32(hd+8), setsize = u32(hd+12), infsize = u32(hd+16);
    dmsg("QUARTET header [sng:%u set:%u inf:%u]\n",
         sngsize, setsize, infsize);
    if (optind+can_do_wav < argc)
      RETURN (too_many_arguments());
    ecode = q4_load(f, play.setpath, &play, sngsize, setsize, infsize);
    my_fclose(&f,play.setpath);
  }
  else if (optind >= argc)
    RETURN(too_few_arguments());
  else if (optind+1+can_do_wav < argc)
    RETURN (too_many_arguments());
  else {
    /* Load voice set file */
    ecode = my_fread(f, play.setpath,hd+20, 222-20);
    if (ecode)
      goto error_exit;

    ecode = vset_parse(&play.vset, play.setpath, f, hd, 0);
    if (ecode)
      goto error_exit;

    my_fclose(&f,play.setpath);

    play.sngpath = argv[optind++];
    ecode = song_load(&play.song, play.sngpath);
  }
  if (ecode)
    goto error_exit;

#ifndef WITHOUT_LIBAO
  if (optind < argc) {
    play.wavpath = argv[optind++];
    opt_wav = 1;
  }

  if (opt_wav && !play.wavpath) {
    ecode = wav_filename(&play.wavpath, play.sngpath);
    if (ecode)
      goto error_exit;
    opt_wav = 2;                        /* mark for free */
  }

  if (!opt_force && play.wavpath) {
    /* Unless opt_force is set, tries to load 4cc out of the output
     * .wav file. Non RIFF files are rejected unless they are empty.
     */
    f = fopen(play.wavpath,"rb");
    if (f) {
      hd[3] = 0;     /* memcmp() will fail unless 4 bytes are read. */
      if (fread(hd,1,4,f) != 0 && memcmp(hd,"RIFF",4)) {
        ecode = E_OUT;
        emsg("output file exists and is not a RIFF wav -- %s\n", play.wavpath);
        goto error_exit;
      }
      my_fclose(&f,play.wavpath);
    }
    errno = 0;
  }
#endif

  ecode = zing_zong(&play);

error_exit:
  /* clean exit */
  my_fclose(&f,play.setpath);
#ifndef WITHOUT_LIBAO
  if (opt_wav == 2)
    free(play.wavpath);
#endif

  ensure_newline();
  return ecode;
}
