/**
 * @file   mix_srate.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using samplerate.
 */

#if WITH_SRATE == 1

#define ZZ_DBG_PREFIX "(mix-sinc) "
#define ZZ_ERR_PREFIX "(mix-sinc) "
#include "zz_private.h"
#include <string.h>
#include <samplerate.h>

#ifndef SRATE_USER_SUPPLY
#define SRATE_USER_SUPPLY 1            /* 0:no user supply function */
#endif

#define RATIO(X) (1.0/(double)(X))

#define F32MAX (MIXBLK*8)
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)

typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int id;

  SRC_STATE * st;
#if !SRATE_USER_SUPPLY
  SRC_DATA    sd;
#endif
  double   rate;

  uint8_t *ptr;
  uint8_t *ptl;
  uint8_t *pte;
  uint8_t *end;

  int      ilen;
  int      imax;
  float    iflt[FLIMAX];

  int      omax;
  float    oflt[FLOMAX];
};

struct mix_data_s {
  int        quality;
  double     rate, irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];

  /***********************
   * !!! ALWAYS LAST !!! *
   ***********************/
  int        flt_max;
  float      flt_buf[1];
};

/* ----------------------------------------------------------------------

   samplerate helpers

   ---------------------------------------------------------------------- */

static int
emsg_srate(const mix_chan_t * const K, int err)
{
  emsg("src: %c: %s\n", K->id, src_strerror(err));
  return E_MIX;
}

static inline double
iorate(const u32_t fp16, const double irate, const double orate)
{
  /* return ldexp(fp16,-16) * irate / orate; */
  return (double)fp16 * irate / (65536.0*orate);
}

static inline double
rate_of_fp16(const u32_t fp16, const double rate) {
  return (double)fp16 * rate;
}

static void chan_flread(float * const d, mix_chan_t * const K, const int n)
{
  if (!n) return;
  zz_assert( n > 0 );
  zz_assert( n < VSET_UNROLL );

  if (!K->ptr)
    zz_memclr(d, n*sizeof(float));
  else {
    zz_assert( K->ptr < K->pte );
    i8tofl(d, K->ptr, n);

    if ( (K->ptr += n) >= K->pte ) {
      if (!K->ptl) {
        K->ptr = 0;
      } else {
        zz_assert( K->ptl < K->pte );
        K->ptr = &K->ptl[ ( K->ptr - K->pte ) % ( K->pte - K->ptl ) ];
        zz_assert( K->ptr >= K->ptl );
        zz_assert( K->ptr <  K->pte );
      }
    }
    zz_assert( K->ptr < K->pte );
  }
}

#if SRATE_USER_SUPPLY
static long
fill_cb(void *_K, float **data)
{
  mix_chan_t * const restrict K = _K;

  zz_assert( K->imax == FLIMAX );
  K->ilen = K->imax;
  chan_flread(K->iflt,K,K->ilen);
  *data = K->iflt;
  return K->ilen;
}
#endif

static int
restart_chan(mix_chan_t * const K)
{
  int err = 0;

  K->ilen = 0;
  K->imax = FLIMAX;
  K->omax = FLOMAX;
  err = src_reset (K->st);
  return err ? emsg_srate(K,err) : E_OK;
}

/* ----------------------------------------------------------------------

   Mixer Push

   ---------------------------------------------------------------------- */

static void *
push_cb(play_t * const P, void * pcm, i16_t N)
{
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  int k;

  zz_assert( P );
  zz_assert( M );
  zz_assert( N <= M->flt_max );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;

    switch (C->trig) {

    case TRIG_NOTE:
      zz_assert( C->note.ins );

      K->ptr = C->note.ins->pcm;
      K->pte = K->ptr + C->note.ins->len;
      K->ptl = C->note.ins->lpl ? (K->pte - C->note.ins->lpl) : 0;
      K->end = K->ptr + C->note.ins->end;
      C->note.ins = 0;
      if (restart_chan(K))
        return 0;

    case TRIG_SLIDE:
      K->rate = rate_of_fp16(C->note.cur, M->rate);

      /* if ( K->rate < M->rate_min || K->rate > M->rate_max ) */
      /*   dmsg("rate out of range ! %f < %f < %f\n", */
      /*        M->rate_min, K->rate, M->rate_max); */

      /* GB: Add/Sub a small amount because these asserts are
       *     sometimes triggered (eg. i686-mingw) by rounding errors
       *     of sort. Weirdly enough adding the conditional debug
       *     message above removed the assert triggering !
       */
      zz_assert( K->rate >= M->rate_min-1E6 );
      zz_assert( K->rate <= M->rate_max+1E6 );
#if !SRATE_USER_SUPPLY
      K->sd.src_ratio = RATIO(K->rate);
#endif
      break;
    case TRIG_STOP: K->ptr = 0;
    case TRIG_NOP:  break;
    default:
      emsg("INTERNAL ERROR: %c: invalid trigger -- %d\n", 'A'+k, C->trig);
      zz_assert( !"wtf" );
      return 0;
    }
  }

  /* Mix float channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    float * restrict flt = M->flt_buf;
    int need = N, zero = 0;

    if (!K->ptr) {
      /* Sound stopped. If it's channel A cleat the buffer else simply
       * skip.
       */
      if (k==0)
        zz_memclr(flt,sizeof(float)*N);
      continue;
    }

    while (need > 0) {
      int i, idone, odone, want;

      zz_assert( K->omax == FLOMAX );
      zz_assert( K->imax == FLIMAX );

      if ( (want=need) > K->omax)
        want = K->omax;

#if !SRATE_USER_SUPPLY

      /* Refill if input is empty. */
      if (!K->ilen) {
        chan_flread(K->iflt, K, K->imax);
        K->sd.input_frames = K->ilen = K->imax;
        K->sd.data_in = K->iflt;
      }

      K->sd.data_out = K->oflt;
      K->sd.output_frames = want;
      if (src_process (K->st, &K->sd))
        return emsg_srate(K,src_error(K->st));
      idone = K->sd.input_frames_used;
      odone = K->sd.output_frames_gen;
      K->sd.data_in += idone;
      K->sd.input_frames -= idone;

#else
      idone = 0;
      odone = src_callback_read(K->st, RATIO(K->rate), want, K->oflt);
      if (odone < 0) {
        emsg_srate(K,src_error(K->st));
        return 0;
      }
#endif

      if ( (idone+odone) > 0) {
        zero = 0;
      } else {
        if (++zero > 7) {
          emsg("%c: too many loop without data -- %u\n",K->id, zero);
          return 0;
        }
      }

      need -= odone;
      if (k == 0)
        for (i=0; i<odone; ++i)
          *flt++ = K->oflt[i];
      else
        for (i=0; i<odone; ++i)
          *flt++ += K->oflt[i];

    }
    zz_assert( need == 0 );
    zz_assert( flt-N == M->flt_buf );
  }
  /* Convert back to s16 */
  fltoi16(pcm, M->flt_buf, N);

  return (int16_t *)pcm+N;
}

/* ---------------------------------------------------------------------- */

static void free_cb(play_t * const P)
{
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  if (M) {
    int k;
    zz_assert( M == P->mixer_data );
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->st)
        src_delete (K->st);
      K->st = 0;
    }
    zz_free(&P->mixer_data);
  }
}

/* ---------------------------------------------------------------------- */

static zz_err_t init_srate(play_t * const P, u32_t spr, const int quality)
{
  zz_err_t ecode = E_SYS;
  int k;
  u32_t N, size;
  zz_assert( !P->mixer_data );
  zz_assert( sizeof(float) == 4 );

  switch (spr) {
  case 0:
  case ZZ_LQ: spr = mulu(P->song.khz,1000u); break;
  case ZZ_MQ: spr = SPR_DEF; break;
  case ZZ_FQ: spr = SPR_MIN; break;
  case ZZ_HQ: spr = SPR_MAX; break;
  }
  if (spr < SPR_MIN) spr = SPR_MIN;
  if (spr > SPR_MAX) spr = SPR_MAX;
  P->spr = spr;

  /* +1 float already allocated in mix_data_t struct */
  N = P->spr/P->rate;
  size = sizeof(mix_data_t) + sizeof(float)*N;
  ecode = zz_calloc(&P->mixer_data, size);
  if (likely(E_OK == ecode)) {
    mix_data_t * M = P->mixer_data;
    zz_assert( M );
    M->flt_max  = N+1;
    M->quality  = quality;
    M->irate    = (double) P->song.khz * 1000.0;
    M->orate    = (double) P->spr;
    M->rate     = iorate(1, M->irate, M->orate);
    M->rate_min = rate_of_fp16(P->song.stepmin, M->rate);
    M->rate_max = rate_of_fp16(P->song.stepmax, M->rate);

    dmsg("iorates : %.3lf .. %.3lf\n", M->rate_min, M->rate_max);
    dmsg("irate   : %.3lf\n", M->irate);
    dmsg("orate   : %.3lf\n", M->orate);
    dmsg("rate    : %.3lf\n", M->rate);

    for (k=0, ecode=E_OK; ecode == E_OK && k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      int err = 0;
      K->id = 'A'+k;

#if SRATE_USER_SUPPLY
      K->st = src_callback_new(fill_cb, quality, 1, &err, K);
#else
      K->st = src_new(quality, 1, &err);
#endif
      ecode = (!K->st || err)
        ? emsg_srate(K,err)
        : restart_chan(K)
        ;
    }
  }

  if (ecode)
    free_cb(P);
  else
    i8tofl(0,0,0);                      /* trick to pre-init LUT */

  return ecode;
}

/* ---------------------------------------------------------------------- */

#define XONXAT(A,B) A##B
#define CONCAT(A,B) XONXAT(A,B)
#define XTR(A) #A
#define STR(A) XTR(A)

#define DECL_SRATE_MIXER(Q,QQ,D)                          \
  static zz_err_t init_##Q(play_t * const P, u32_t spr)   \
  {                                                       \
    return init_srate(P, spr, SRC_##QQ);                  \
  }                                                       \
  mixer_t mixer_srate_##Q =                               \
  {                                                       \
    "sinc:" XTR(Q), D, init_##Q, free_cb, push_cb         \
  }

DECL_SRATE_MIXER(best,SINC_BEST_QUALITY,
                 "band limited sinc (best quality)");
DECL_SRATE_MIXER(medium,SINC_MEDIUM_QUALITY,
                 "band limited sinc (medium quality)");
DECL_SRATE_MIXER(fast,SINC_FASTEST,
                 "band limited sinc (fastest)");

#endif /* WITH_SRATE == 1 */
