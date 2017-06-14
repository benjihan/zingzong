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
  "Copyright (c) 2017 Bejamin Gerard AKA Ben/OVR";
static const char licence[] = \
  "Licenced under MIT licence";
static const char bugreport[] =                                 \
  "Report bugs to <https://github.com/benjihan/zingzong>";

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* libao */
#include <ao/ao.h>

/* stdc */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>

#if !defined(DEBUG) && !defined(NDEBUG)
#define NDEBUG 1
#endif

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "zingzong"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION __DATE__
#endif

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

static char me[] = PACKAGE_NAME;
static int opt_sampling = 48000, opt_tickrate = 200, opt_wav;

#define VSET_MAX_SIZE (1<<21) /* arbitrary .set max size */
#define SONG_MAX_SIZE (1<<18) /* arbitrary .4v max size  */
#define MAX_LOOP 67           /* max loop depth (singsong.prg) */

enum {
  E_OK, E_ERR, E_ARG, E_SYS, E_SET, E_SNG, E_OUT, E_PLA,
  E_666 = 66
};

/* ----------------------------------------------------------------------
 *  Messages
 * ----------------------------------------------------------------------
 */

static void emsg(const char * fmt, ...)
{
  va_list list;
  va_start(list,fmt);
  fprintf(stderr,"%s: ", me);
  vfprintf(stderr,fmt,list);
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
  fprintf(stderr,"WARNING: ");
  vfprintf(stderr,fmt,list);
  fflush(stderr);
  va_end(list);
}

static void dmsg(const char *fmt, ...)
{
#ifdef DEBUG
  va_list list;
  va_start(list,fmt);
  vfprintf(stdout,fmt,list);
  fflush(stdout);
  va_end(list);
#endif
}

static void imsg(const char *fmt, ...)
{
  va_list list;
  va_start(list,fmt);
  vfprintf(stdout,fmt,list);
  fflush(stdout);
  va_end(list);
}

/* ----------------------------------------------------------------------
 *  Types definitions
 * ----------------------------------------------------------------------
 */

typedef unsigned int   uint_t;
typedef unsigned short ushort_t;
typedef struct vset_s  vset_t;
typedef struct song_s  song_t;
typedef struct sequ_s  sequ_t;
typedef struct bin_s   bin_t;

typedef struct play_s play_t;
typedef struct chan_s chan_t;

struct bin_s {
  uint_t   size;                  /* data size */
  uint_t   xtra;                  /* extra bytes allocated */
  uint8_t  data[1];               /* data pointer */
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
  char * vsetpath;
  char * songpath;
  char * wavepath;

  vset_t vset;
  song_t song;
  uint_t tick;

  uint_t spr;
  int16_t * mix_buf;
  int pcm_per_tick;
  int has_loop;

  struct {
    int              id;
    ao_sample_format fmt;
    ao_device       *dev;
    ao_info         *info;
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

/* ----------------------------------------------------------------------
 * Load binaries
 * ----------------------------------------------------------------------
 */

static void free_bin(bin_t * bin) {
  if (bin) free(bin);
}

static bin_t *alloc_bin(const char * path, uint_t len, uint_t xtra)
{
  bin_t * bin;

  bin = malloc( bin->data - (uint8_t *)bin + len + xtra);
  if (!bin) {
    sysmsg(path,"alloc");
  } else {
    bin->size = len;
    bin->xtra = xtra;
  }
  return bin;
}

static bin_t *load_bin(FILE * f, const char * path, uint_t max, uint_t xtra)
{
  size_t tell, size;
  bin_t *bin = 0;

  if (-1 == (tell = ftell(f)) ||
      -1 == fseek(f,0,SEEK_END) ||
      -1 == (size = ftell(f)) ||
      -1 == fseek(f,tell,SEEK_SET) ||
      size < tell) {
    sysmsg(path,"get size");
    goto error;
  }
  size -= tell;
  dmsg("%s: @%u %u+%u extra => %u bytes\n",
       path, (uint_t)tell, (uint_t)size, xtra, (uint_t)size+xtra);

  if (max && size > max) {
    emsg("too large -- %s", path);
    goto error;
  }

  bin = alloc_bin(path, size, xtra);
  if (!bin)
    goto error;

  size = fread(bin->data,1,bin->size,f);
  if (size != bin->size) {
    sysmsg(path,"read");
    goto error;
  }
  return bin;

error:
  free_bin(bin);
  return 0;
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

/**
 * Load .4v sonf file.
 */
static int song_load_file(song_t *song, const char * fname)
{
  int ecode = E_SNG, has_note = 0;
  uint8_t hd[16];
  uint_t off, k;
  FILE * f = 0;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"in */
  };

  assert(song);
  assert(fname);
  memset(song,0,sizeof(*song));

  /* Open song (.4v) file */
  if (f = fopen(fname,"rb"), !f) {
    sysmsg(fname,"open");
    goto error;
  }

  /* Read song header */
  if (16 != fread(hd,1,16,f)) {
    sysmsg(fname,"read");
    goto error;
  }

  /* Parse song header */
  song->khz   = u16(hd+0);
  song->barm  = u16(hd+2);
  song->tempo = u16(hd+4);
  song->sigm  = hd[6];
  song->sigd  = hd[7];
  dmsg("rate: %ukHz, bar:%u, tempo:%u, signature:%u/%u\n",
       song->khz, song->barm, song->tempo, song->sigm, song->sigd );

  song->bin = load_bin(f,fname,SONG_MAX_SIZE, 0);
  if (!song->bin)
    goto error;

  off = song->bin->size % 12u;
  if (off) {
    wmsg("song data is not a multiple of 12.\n");
    song->bin->size -= off;
  }

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  for (k=off=0; k<4 && off<song->bin->size; off+=12) {
    sequ_t * const seq = (sequ_t *)(song->bin->data+off);
    uint_t   const cmd = u16(seq->cmd);

    if (!song->seq[k])
      song->seq[k] = seq;               /* Sequence */

    switch (cmd) {
    case 'F':                           /* End-Voice */
      if (!has_note)
        song->seq[k] = (sequ_t *)nullseq;
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
  if (f) fclose(f);
  if (ecode) {
    free_bin(song->bin);
    memset(song,0,sizeof(*song));
  }
  return ecode;
}

/**
 * Load .set voice set file.
 */
static int vset_load_file(vset_t * vset, const char * fname)
{
  int i, ecode = E_SET;
  uint8_t hd[222], *end, *pcm;
  FILE * f = 0;

  memset(vset,0,sizeof(*vset));

  /* Open voiceset (.set) file */
  if (f = fopen(fname,"rb"), !f) {
    sysmsg(fname,"open");
    goto error;
  }

  /* Load voiceset header */
  if (222 != fread(hd,1,222,f)) {
    sysmsg(fname,"read");
    goto error;
  }

  /* Check sampling rate */
  vset->khz = hd[0];
  if (vset->khz < 4u || vset->khz > 20u) {
    emsg("%s -- sampling rate out of range -- %u\n", fname, vset->khz);
    goto error;
  }

  /* Check instrument number */
  vset->nbi = hd[1]-1;
  dmsg("vset instruments: %u\n", vset->nbi);
  if (vset->nbi < 1 || vset->nbi > 20) {
    emsg("vset instrument number out of range -- %u\n", vset->nbi);
    goto error;
  }

  /* Load data */
  vset->bin = load_bin(f,fname,VSET_MAX_SIZE,0);
  if (!vset->bin)
    goto error;
  /* end = vset->bin->data + vset->bin->size; */

  for (i=0; i<vset->nbi; ++i) {
    int off = u32(&hd[142+4*i]);
    int o = off - 222 + 8;
    uint_t len, lpl;

    dmsg("---\nI#%02u \"%s\" +$%x +$%x\n",i+1, hd+2+7*i, off, off -222 +8 );

    pcm = vset->bin->data+o;
    len = u32(pcm-4);
    lpl = u32(pcm-8);

    assert (! (len & 0xFFFF ));
    if (lpl == 0xFFFFFFFF)
      lpl = 0;
    assert ( ! (lpl & 0xFFFF )) ;
    len >>= 16;
    lpl >>= 16;
    assert(lpl <= len);
    assert(pcm+len <= end);

    if (lpl > len) {
      emsg("I#%02d invalid loop %u > %u\n",i+1,lpl,len);
      goto error;
    }
    if (len > vset->bin->size) {
      emsg("I#%02d sample out of data range by %u byte(s)\n",
           i+1,len-vset->bin->size);
      goto error;
    }
    dmsg("I#%02u ******* [$%05X:$%05X:$%05X] [$%05X:$%05X:$%05X]\n",
         i+1,
         0, len-lpl, len,
         o, o+len-lpl, o+len);
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }

  ecode = E_OK;
error:
  if (f)
    fclose(f);
  if (ecode) {
    free_bin(vset->bin);
    memset(vset,0,sizeof(*vset));
  }
  return ecode;
}

static int zz_play(play_t * P)
{
  int k;
  assert(P);

  /* Setup player */
  memset(P->chan,0,sizeof(P->chan));
  for (k=0; k<4; ++k) {
      chan_t * const C = P->chan+k;
      C->seq = P->song.seq[k];
      C->cur = P->chan[k].seq;
  }

  dmsg("Start playing.\n");
  for (;;) {
    int started = 0;

    ++P->tick;

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
          C->loop_level = 0;
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

          if (!C->loop[l].cnt)
            C->loop[l].cnt = (par >> 16) + 1;

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

    if (P->has_loop == 15) {
      if (P->ao.dev && P->ao.info->type == AO_TYPE_LIVE)
        puts("");
      break;
    }

    /* audio output */
    if (P->ao.dev) {
      int n;
      unsigned stp[4];

      if (P->ao.info->type == AO_TYPE_LIVE) {
        uint_t ms = P->tick * 1000u / opt_tickrate;
        uint_t m  = ms / 60000;
        uint_t s  = ms / 1000 % 60u;
        ms %= 1000u;
        printf("\r%02u:%02u,%03u", m, s, ms);
        fflush(stdout);
      }

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

      if (!ao_play(P->ao.dev, (void*)P->mix_buf, 2*n)) {
        emsg("libao: could not play buffer #%d \"%s\"\n",
             P->ao.id, P->ao.info->short_name);
        return E_OUT;
      }
    }
  } /* loop ! */
  dmsg("Stop playing.\n");
  return E_OK;
}

static int zing_zong(char * setname, char * sngname, char * wavname)
{
  int ecode;
  static play_t play;

  memset(&play,0,sizeof(play));

  ecode = E_SNG;
  if (song_load_file(&play.song, sngname))
    goto error_no_ao;

  ecode = E_SET;
  if (vset_load_file(&play.vset, setname))
    goto error_no_ao;

  imsg("zinging that zong at %uhz with %u ticks per second\n"
       "vset: \"%s\" (%ukHz, %u sound)\n"
       "song: \"%s\" (%ukHz, %u, %u, %u:%u)\n",
       opt_sampling, opt_tickrate,
       basename(setname), play.vset.khz, play.vset.nbi,
       basename(sngname), play.song.khz, play.song.barm,
       play.song.tempo, play.song.sigm, play.song.sigd);
  if (wavname)
    imsg("wave: \"%s\"\n", wavname);

  /* Init libao */
  ecode = E_OUT;
  ao_initialize();
  memset(&play.ao.fmt,0,sizeof(play.ao.fmt));
  play.ao.fmt.bits = 16;
  play.ao.fmt.rate = opt_sampling;
  play.ao.fmt.channels = 1;
  play.ao.fmt.byte_format = AO_FMT_NATIVE;
  play.ao.id = wavname
    ? ao_driver_id("wav")
    : ao_default_driver_id()
    ;
  play.ao.info = ao_driver_info(play.ao.id);
  if (!play.ao.info) {
    emsg("libao: could not get driver #%d info\n", play.ao.id);
    goto error;
  }
  play.ao.dev = wavname
    ? ao_open_file(play.ao.id, wavname, 1, &play.ao.fmt, 0)
    : ao_open_live(play.ao.id, &play.ao.fmt, 0)
    ;
  if (!play.ao.dev) {
    emsg("libao: could not initialize audio driver #%d \"%s\"\n",
         play.ao.id, play.ao.info->short_name);
    goto error;
  }

  dmsg("ao %s device \"%s\" is open\n",
       play.ao.info->type == AO_TYPE_LIVE ? "live" : "file",
       play.ao.info->short_name);

  play.spr = play.ao.fmt.rate;
  play.pcm_per_tick = (play.spr + (opt_tickrate>>1)) / opt_tickrate;
  play.mix_buf = (int16_t *) malloc ( 2 * play.pcm_per_tick );
  if (!play.mix_buf) {
    ecode = sysmsg("audio", "alloc");
    goto error;
  }

  ecode = zz_play(&play);
error:
  if (play.ao.dev && !ao_close(play.ao.dev) && ecode == E_OK) {
    sysmsg("audio","close");
    ecode = E_OUT;
  }
  ao_shutdown();

error_no_ao:
  free(play.mix_buf);
  free_bin(play.vset.bin);
  free_bin(play.song.bin);

  return ecode;
}


/**
 * Print usage message.
 */
static void print_usage(void)
{
  puts(
    "Usage: zingzong [OPTIONS] <inst.set> <song.4v> [output.wav]\n"
    "\n"
    "  A simple /|\\ Atari ST /|\\ quartet music file player\n"
    "\n"
    "OPTIONS:\n"
    " -h --help --usage  Print this message and exit.\n"
    " -V --version       Print version and copyright and exit.\n"
    " -t --tick=HZ       Set player tick rate (default is 200hz)\n"
    " -r --rate=HZ       Set sampling rate (default is 48kHz)\n"
    " -w --wav           Generated a .wav file (implicit if output is set)\n"
    "\n"
    "OUTPUT:\n"
    " If output is set it creates a .wav file of this name (implies `-w').\n"
    " Else with `-w' the .wav filename is the song filename stripped\n"
    " of its path and its .4v extension replaced by .wav."
    );
    puts("");
    puts(copyright);
    puts(licence);
    puts(bugreport);
}

/**
 * Print version and copyright message.
 */
static void print_version(void)
{
  puts(PACKAGE_STRING "\n");
  puts(copyright);
  puts(licence);
}

/**
 * Create .wav filename from .4v
 */
static char * wav_filename(char * sngname)
{
  char * sngleaf = basename(sngname);
  int l = strlen(sngleaf);
  char * wavname = malloc(l+4);
  if (!wavname) {
    sysmsg(sngleaf,"alloc");
    return 0;
  }
  if (l >= 4 &&
      tolower(sngleaf[l-1]) == 'v' &&
      sngleaf[l-2] == '4' &&
      sngleaf[l-3] == '.')
    l -= 3;
  memcpy(wavname,sngleaf,l);
  strcpy(wavname+l,".wav");
  return wavname;
}

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

#define RETURN(V) do { ecode = V; goto error_exit; } while(0)

int main(int argc, char *argv[])
{
  static char sopts[] = "hV" "w" "r:t:";
  static struct option lopts[] = {
    { "help",    0, 0, 'h' },
    { "usage",   0, 0, 'h' },
    { "version", 0, 0, 'V' },
    /**/
    { "wav",     0, 0, 'w' },
    { "tick=",   1, 0, 't' },
    { "rate=",   1, 0, 'r' },
    { 0 }
  };
  int c, ecode = E_ERR;
  char *sngname = 0,  *setname = 0, *wavname = 0;

  assert( sizeof(songhd_t) ==  16);
  assert( sizeof(sequ_t)   ==  12);

  opterr = 0;
  while ((c = getopt_long (argc, argv, sopts, lopts, 0)) != -1) {
    switch (c) {
    case 'h': print_usage(); return E_OK;
    case 'V': print_version(); return E_OK;
    case 'w': opt_wav = 1; break;
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

  if (optind+2 > argc) {
    emsg("Missing argument. Try --help.\n");
    RETURN (E_ARG);
  }
  setname = argv[optind++];
  sngname = argv[optind++];
  if (optind < argc) {
    wavname = argv[optind++];
    opt_wav = 1;
  }
  if (optind != argc) {
    emsg("Too many arguments. Try --help.\n");
    RETURN (E_ARG);
  }

  if (opt_wav && !wavname) {
    wavname = wav_filename(sngname);
    if (!wavname)
      RETURN (E_SYS);
    opt_wav = 2;                  /* to know it's been allocated */
  }

  ecode = zing_zong(setname, sngname, wavname);

error_exit:
  /* clean exit */
  if (opt_wav == 2)
    free(wavname);
  return ecode;
}
