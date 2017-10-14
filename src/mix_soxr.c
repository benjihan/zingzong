/**
 * @file   mix_soxr.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using soxr.
 */

#if WITH_SOXR == 1

#define ZZ_DBG_PREFIX "(mix-soxr) "
#define ZZ_ERR_PREFIX "(mix-soxr) "
#include "zz_private.h"
#include <string.h>
#include <soxr.h>

#ifndef SOXR_USER_SUPPLY
#define SOXR_USER_SUPPLY 1             /* 0:no user supply function */
#endif

#define F32MAX (MIXBLK*8)
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)

typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int id;
  soxr_t   soxr;
  double   rate;

  uint8_t *ptr;
  uint8_t *ptl;
  uint8_t *pte;
  uint8_t *end;

  float   *icur;
  int      ilen;
  int      imax;
  float    iflt[FLIMAX];

  int      omax;
  float    oflt[FLOMAX];
};

struct mix_data_s {
  soxr_quality_spec_t qspec;
  soxr_io_spec_t      ispec;

  double   rate, irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];

  /***********************
   * !!! ALWAYS LAST !!! *
   ***********************/
  int   flt_max;
  float flt_buf[1];
};

/* ----------------------------------------------------------------------

   Read input PCM

   ---------------------------------------------------------------------- */

static void
chan_flread(float * const d, mix_chan_t * const K, const int n)
{
  if (!n) return;
  zz_assert( n > 0 );
  zz_assert( n < VSET_UNROLL );

  if (!K->ptr)
    zz_memclr(d,n*sizeof(*d));
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

/* ----------------------------------------------------------------------

   soxr helpers

   ---------------------------------------------------------------------- */

static int
emsg_soxr(const mix_chan_t * const K, soxr_error_t err)
{
  emsg("soxr: %c: %s\n", K->id, soxr_strerror(err));
  return E_MIX;
}

#if SOXR_USER_SUPPLY
/* Supply data to be resampled. */
static size_t
fill_cb(void * _K, soxr_in_t * data, size_t reqlen)
{
  mix_chan_t * const K = _K;
  int len = reqlen;

  zz_assert( len <= K->imax );         /* Limited by set_input_fb() */
  zz_assert( K->imax == FLIMAX );      /* For now keep it simple */
  if (len > K->imax)
    len = K->imax;
  *data = K->iflt;
  K->ilen = len;
  chan_flread(K->iflt, K, len);

  return len;
}
#endif

static int
restart_chan(mix_chan_t * const K)
{
  soxr_error_t err = 0;

  K->icur = 0;
  K->ilen = 0;
  K->imax = FLIMAX;
  K->omax = FLOMAX;
  err = soxr_clear(K->soxr);

#if SOXR_USER_SUPPLY
  if (!err)
    err = soxr_set_input_fn(K->soxr, fill_cb, K, K->imax);
#endif

  return err ? emsg_soxr(K,err) : E_OK;
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

/* ----------------------------------------------------------------------

   Mixer Push

   ---------------------------------------------------------------------- */

static void *
push_soxr(play_t * const P, void * pcm, i16_t N)
{
  soxr_error_t err;
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  int k;

  zz_assert( P );
  zz_assert( M );
  zz_assert( pcm );
  zz_assert( N != 0 );
  zz_assert( N > 0 );
  zz_assert( N <= M->flt_max );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;
    int slew = 0;
    const int slew_val = N>>1;

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
      slew = -slew_val;                /* -X+X == 0 on note trigger */

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
      err = soxr_set_io_ratio(K->soxr, K->rate, slew+slew_val);

      /* dmsg("%c: trig=%d stp=%08X %.3lf\n", */
      /*      C->id, C->trig, C->note.cur, K->rate); */

      if (err) {
        emsg_soxr(K,err);
        return 0;
      }
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
      if (!k)
        zz_memclr(flt,sizeof(float)*N);
      continue;
    }

    while (need > 0) {
      int i;
      size_t want = need, idone, odone;

      zz_assert( K->omax == FLOMAX );
      zz_assert( K->imax == FLIMAX );

      if (want > K->omax)
        want = K->omax;


#if SOXR_USER_SUPPLY
      idone = 0;
      odone =
        soxr_output(
          K->soxr,
          K->oflt,
          want);
      err = soxr_error(K->soxr);
      if (err || odone < 0) {
        emsg_soxr(K,err);
        return 0;
      }

#else
      /* Fill input when it's empty */
      if (!K->ilen)
        chan_flread(K->icur = K->iflt, K, K->ilen = K->imax);
      zz_assert( K->ilen > 0 );
      zz_assert( K->ilen <= K->omax );
      zz_assert( &K->iflt[K->omax] == &K->icur[K->ilen] );
      err = soxr_process(
        K->soxr,
        K->icur, K->ilen, &idone,
        K->oflt, want, &odone);

      if (err)
        return emsg_soxr(K,err);

      K->ilen -= idone;
      K->icur += idone;

      zz_assert( K->ilen >= 0 && K->ilen <= K->imax );
      zz_assert( odone >= 0 && odone <= want && odone <= K->omax );
#endif

      if (idone+odone != 0) {
        zero = 0;
      } else {
        dmsg("mix(%c(%lu) need:%lu i:%lu/%lu/%lu o:%lu/%lu/%lu) -> %s\n",
             K->id, LU(P->tick),
             LU(need),
             LU(idone), LU(K->ilen), LU(K->imax),
             LU(odone), LU(want), LU(K->omax),
             soxr_strerror(err));
        if (++zero > 7) {
          emsg("%c: too many loop without data -- %hu\n",K->id, HU(zero));
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

    /* $$$ TEST */
    if (need != 0)
      wmsg("need:%d\n", need);

    zz_assert( need == 0 );
    zz_assert( flt-N == M->flt_buf );
  }

  /* Convert back to s16 */
  fltoi16(pcm, M->flt_buf, N);

  return (int16_t *)pcm + N;
}

/* ---------------------------------------------------------------------- */

static void free_soxr(play_t * const P)
{
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  if (M) {
    int k;
    zz_assert( M == P->mixer_data );
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->soxr) {
        soxr_delete(K->soxr);
        K->soxr = 0;
      }
    }
    zz_free(&P->mixer_data);
  }
}

/* ---------------------------------------------------------------------- */

static zz_err_t init_soxr(play_t * const P, u32_t spr)
{
  zz_err_t ecode = E_SYS;
  int k;
  soxr_error_t err;
  u32_t size, N;

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
  ecode = zz_calloc(&P->mixer_data,size);
  if (likely(E_OK == ecode)) {
    mix_data_t * M = P->mixer_data;
    zz_assert( M );
    M->flt_max  = N+1;
    M->qspec    = soxr_quality_spec(SOXR_HQ, SOXR_VR);
    M->ispec    = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
    M->irate    = (double) P->song.khz * 1000.0;
    M->orate    = (double) P->spr;
    M->rate     = iorate(1, M->irate, M->orate);
    M->rate_min = rate_of_fp16(P->song.stepmin, M->rate);
    M->rate_max = rate_of_fp16(P->song.stepmax, M->rate);

    dmsg("iorates : %.3lf .. %.3lf\n", M->rate_min, M->rate_max);
    dmsg("irate   : %.3lf\n", M->irate);
    dmsg("orate   : %.3lf\n", M->orate);
    dmsg("rate    : %.3lf\n", M->rate * 65536.0);

    for (k=0, ecode=E_OK; ecode == E_OK && k<4; ++k) {
      mix_chan_t * const K = M->chan+k;

      K->id = 'A'+k;
      K->soxr = soxr_create(
        M->rate_max, /* input sampling rate */
        1.0,         /* output sampling rate */
        1,           /* num channels */
        &err,        /* error */
        &M->ispec,   /* io specs */
        &M->qspec,   /* quality spec */
        NULL);       /* runtime spec */

      if (K->soxr) {
        /* Set a iorate in the middle. It should matter much as no
         * note is played anyway. */
        K->rate = .5 * ( M->rate_min + M->rate_max );
        err = soxr_set_io_ratio(K->soxr, K->rate, 0);
      }

      ecode = err
        ? emsg_soxr(K,err)
        : restart_chan(K)
        ;
    }
  }

  if (ecode)
    free_soxr(P);
  else
    i8tofl(0,0,0);                    /* trick to pre-init LUT */

  return ecode;
}

/* ---------------------------------------------------------------------- */

mixer_t mixer_soxr = {
  "soxr",
  "high quality variable rate",
  init_soxr,
  free_soxr,
  push_soxr
};

#endif /* WITH_SOXR == 1 */
