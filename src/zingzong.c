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
# define WAVOPT "w"
#else
# define WAVOPT
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
static int opt_sampling = 48000, opt_tickrate = 200, opt_stdout;

#ifndef WITHOUT_LIBAO
static int opt_wav;
#endif

#define VSET_MAX_SIZE (1<<21) /* arbitrary .set max size */
#define SONG_MAX_SIZE (1<<18) /* arbitrary .4v max size  */
#define INFO_MAX_SIZE 4096    /* arbitrary .4q info max size */
#define MAX_LOOP 67           /* max loop depth (singsong.prg) */

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
#ifdef DEBUG
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
typedef struct vset_s  vset_t;
typedef struct song_s  song_t;
typedef struct sequ_s  sequ_t;
typedef struct bin_s   bin_t;

typedef struct play_s play_t;
typedef struct chan_s chan_t;

struct bin_s {
  uint_t   size;                  /* data size */
  uint8_t  data[sizeof(int)];
};

struct vset_s {
  bin_t * bin;

  int khz;                          /* sampling rate from .set */
  int nbi;                          /* number of instrument [1..20] */
  struct {
    int len;                            /* size in byte */
    int lpl;                            /* loop length in byte */
    uint8_t * pcm;                      /* sample address */
  } inst[20];

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

  int loop_level;
  struct {
    sequ_t * seq;                       /* loop point */
    int      cnt;                       /* loop count */
  } loop[MAX_LOOP];

  struct {
    uint8_t * pcm;
    int stp, idx, len, lpl;
  } mix;

  struct {
    int aim, stp;
  } pta;

  int curi;                            /* current instrument number */
  int wait;                            /* number of tick left to wait */
  int has_loop;                        /* has loop (counter) */
};

struct play_s {
  char * setpath;
  char * sngpath;
#ifndef WITHOUT_LIBAO
  char * wavpath;
#endif

  vset_t vset;
  song_t song;
  info_t info;
  uint_t tick;
  uint_t spr;
  int16_t * mix_buf;
  int pcm_per_tick;
  int has_loop;

  FILE * outfp;

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

static int bin_alloc(bin_t ** pbin, const char * path, uint_t len)
{
  bin_t * bin = 0;
  assert(pbin); assert(path);
  *pbin = bin = malloc((uint_t)(intptr_t)(bin->data+len));
  if (!bin)
    return sysmsg(path,"alloc");
  bin->size = len;
  return E_OK;
}

static int bin_read(bin_t * bin, const char * path,
                    FILE *f, uint_t off, uint_t len)
{
  return my_fread(f,path,bin->data+off,len);
}

static int bin_load(bin_t ** pbin, const char * path,
                    FILE *f, uint_t size, uint_t max)
{
  int ecode;

  if (!size) {
    ecode = my_fsize(f, path, &size);
    if (ecode)
      goto error;
  }
  if (max && size > max) {
    emsg("too large (load > %u) -- %s\n", max, path);
    ecode = E_ERR;
    goto error;
  }
  ecode = bin_alloc(pbin, path, size);
  if (ecode)
    goto error;
  ecode = bin_read(*pbin, path, f, 0, size);

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
  ecode = bin_load(&song->bin, path, f, size, SONG_MAX_SIZE);
  if (ecode)
    goto error;
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
    case 'S': case 'R': case 'l': case 'L': case 'V':
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



static int vset_parse(vset_t *vset, const char *path, FILE *f,
                      uint8_t *hd, uint_t size)
{
  int ecode = E_SET, i;

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
  ecode = bin_load(&vset->bin, path, f, size, VSET_MAX_SIZE);
  if (ecode)
    goto error;

  for (i=0; i<vset->nbi; ++i) {
    int off = u32(&hd[142+4*i]);
    int o = off - 222 + 8;
    uint_t len, lpl;
    uint8_t * pcm;

    pcm = vset->bin->data+o;
    len = u32(pcm-4);
    lpl = u32(pcm-8);

    assert (! (len & 0xFFFF ));
    if (lpl == 0xFFFFFFFF)
      lpl = 0;
    assert ( ! (lpl & 0xFFFF )) ;
    len >>= 16;
    lpl >>= 16;

    dmsg("I#%02u \"%7s\" [$%05X:$%05X:$%05X] [$%05X:$%05X:$%05X]\n",
         i+1, hd+2+7*i,
         0, len-lpl, len,
         o, o+len-lpl, o+len);

    assert(lpl <= len);
    assert(o+len <= vset->bin->size);

    if (lpl > len) {
      emsg("I#%02d invalid loop %u > %u\n",i+1,lpl,len);
      goto error;
    }
    if (len > vset->bin->size) {
      emsg("I#%02d sample out of data range by %u byte(s)\n",
           i+1,len-vset->bin->size);
      goto error;
    }
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }

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
  /* info (ignoring errorS) */
  if (!bin_load(&play.info.bin, path, f, infsize, INFO_MAX_SIZE)
      && play.info.bin->size > 1) {
    play.info.comment = (char *) play.info.bin->data;
    dmsg("Comments:\n%s\n", play.info.comment);
  }
#endif

error:
  return ecode;
}


static int zz_play(play_t * P)
{
  const uint_t maxtick = 60*60*opt_tickrate;
  int k;
  assert(P);

  /* Setup player */
  memset(P->chan,0,sizeof(P->chan));
  for (k=0; k<4; ++k) {
    chan_t * const C = P->chan+k;
    C->seq = P->song.seq[k];
    C->cur = P->chan[k].seq;
  }

  for (;;) {
    int started = 0;

    ++P->tick;
    if (maxtick && P->tick > maxtick) {
      wmsg("unable to detect song end. Aborting.\n");
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

        switch (cmd) {

        case 'F':                       /* End-Voice */
          seq = C->seq;
          P->has_loop |= 1<<k;
          C->has_loop++;
          C->loop_level = 0;            /* Safety net */
          dmsg("%c: [%c%c%c%c] end @%u +%u\n",
               'A'+k,
               ".A"[!!(1&P->has_loop)],
               ".B"[!!(2&P->has_loop)],
               ".C"[!!(4&P->has_loop)],
               ".D"[!!(8&P->has_loop)],
               P->tick,
               C->has_loop);
          break;

        case 'V':                       /* Voice-Change */
          C->curi = par >> 2;
          break;

        case 'P':                       /* Play-Note */
          C->mix.stp = C->pta.aim = stp;
          C->pta.stp = 0;
          C->wait = len;

          /* Copy current instrument to channel */
          C->mix.idx = 0;
          C->mix.pcm = P->vset.inst[C->curi].pcm;
          C->mix.len = P->vset.inst[C->curi].len << 16;
          C->mix.lpl = P->vset.inst[C->curi].lpl << 16;

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
            if (C->loop[l].seq == C->seq && u16(seq->cmd) == 'F')
              C->loop[l].cnt = 1;
            else
              C->loop[l].cnt = (par >> 16) + 1;
          }

          if (--C->loop[l].cnt)
            seq = C->loop[l].seq;
          else
            --C->loop_level;
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
      int n;
      unsigned stp[4];

#ifndef WITHOUT_LIBAO
      if (P->ao.info && P->ao.info->type == AO_TYPE_LIVE) {
        FILE * const out = stdout_or_stderr();
        const uint_t s = P->tick / opt_tickrate;
        fprintf(out,"\r|> %02u:%02u", s / 60u, s % 60u);
        newline = 0;
        fflush(out);
      }
#endif

      for (k=0; k<4; k++) {
        chan_t * const C = P->chan+k;
        stp[k] = C->mix.stp * P->song.khz * 10u / (P->spr/100u);
      }
      for (n=0; n<P->pcm_per_tick; ++n) {
        /* GB: $$$ Teribad mix loop $$$ */
        int k;
        unsigned int v = 0;
        for (k=0; k<4; ++k) {
          chan_t * const C = P->chan+k;

          if (C->mix.pcm) {
            v += C->mix.pcm[  C->mix.idx >> 16 ];
            C->mix.idx += stp[k];
            if (C->mix.idx >= C->mix.len) {
              C->mix.idx -= C->mix.lpl;
              if (!C->mix.lpl)
                C->mix.pcm = 0;
            }
          } else
            v += 0x80;
        }
        P->mix_buf[n] = 0x8000 ^ (v << 6);
      }

      if (P->ao.dev) {
#ifndef WITHOUT_LIBAO
        if (!ao_play(P->ao.dev, (void*)P->mix_buf, n<<1)) {
          emsg("libao: failed to play buffer #%d \"%s\"\n",
               P->ao.id, P->ao.info->short_name);
          return E_OUT;
        }
#endif
      } else if ( (n<<1) != fwrite(P->mix_buf,1,n<<1,P->outfp) ) {
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
  puts(
    "Usage: zingzong [OPTIONS] <inst.set> <song.4v>" OUTWAV "\n"
    "       zingzong [OPTIONS] <music.q4>" OUTWAV "\n"
    "\n"
    "  A simple /|\\ Atari ST /|\\ quartet music file player\n"
    "\n"
    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz)\n"
    " -r --rate=HZ       Set sampling rate (default is 48kHz)\n"
    " -c --stdout        Output raw sample to stdout\n"
#ifndef WITHOUT_LIBAO
    " -w --wav           Generated a .wav file (implicit if output is set)\n"
    "\n"
    "OUTPUT:\n"
    " If output is set it creates a .wav file of this name (implies `-w').\n"
    " Else with `-w' alone the .wav file is the song file stripped of its\n"
    " path with its extension replaced by .wav.\n"
    " If output exists the program will refuse to create the file unless\n"
    " it is already a RIFF file (just a \"RIFF\" 4cc test)"
#else
    "\n"
    "IMPORTANT:\n"
    " This version of zingzong has been built without libao support.\n"
    " Therefore it can not produce audio output nor RIFF .wav file."
#endif
    );
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
#endif
    { "stdout",  0, 0, 'c' },
    { "tick=",   1, 0, 't' },
    { "rate=",   1, 0, 'r' },
    { 0 }
  };
  int c, cant_do_wav, ecode = E_ERR;
  FILE * f = 0;
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
#endif
    case 'c': opt_stdout = 1; break;
    case 'r':
      if (-1 == (opt_sampling = uint_arg(optarg,"rate",4000,96000)))
        RETURN (E_ARG);
      break;
    case 't':
      if (-1 == (opt_tickrate = uint_arg(optarg,"tick",25,800)))
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
  cant_do_wav = !!opt_stdout;
#else
  cant_do_wav = 1;
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
    if (optind+1-cant_do_wav < argc)
      RETURN (too_many_arguments());
    ecode = q4_load(f, play.setpath, &play, sngsize, setsize, infsize);
    my_fclose(&f,play.setpath);
  }
  else if (optind >= argc)
    RETURN(too_few_arguments());
  else if (optind+2-cant_do_wav < argc)
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

  if (play.wavpath) {
    f = fopen(play.wavpath,"rb");
    if (f) {
      if (4 != fread(hd,1,4,f) || memcmp(hd,"RIFF",4)) {
        ecode = E_OUT;
        emsg("output file exists and is not a RIFF wav -- %s\n", play.wavpath);
        goto error_exit;
      }
      my_fclose(&f,play.wavpath);
    }
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
