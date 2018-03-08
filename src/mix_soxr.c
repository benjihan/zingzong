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

#define F32MAX 48
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)

typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int8_t   id;                          /* { 'A','B','C','D' } */
  int8_t   run;                         /* 0:voice stopped */

  soxr_t   soxr;
  double   rate;

  uint8_t *pta;                         /* base address */
  uint8_t *ptr;                         /* current address */
  uint8_t *ptl;                         /* loop address (0=no loop) */
  uint8_t *pte;                         /* end address */

  float    iflt[FLIMAX];
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

   soxr helpers

   ---------------------------------------------------------------------- */

static inline void
emsg_soxr(const mix_chan_t * const K, soxr_error_t err)
{
  emsg("soxr: %c: %s\n", K->id, soxr_strerror(err));
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

   Read input PCM

   ---------------------------------------------------------------------- */

static void
chan_flread(float * restrict d, mix_chan_t * const K, int n)
{
  zz_assert( d );
  zz_assert( K );
  zz_assert( n > 0 );

  while ( K->ptr && n ) {
    uint8_t * const ptr = K->ptr;
    int l = K->pte - K->ptr;

    if (l > n)
      K->ptr += ( l = n );
    else
      K->ptr = K->ptl;

    i8tofl(d, ptr, l);
    d += l;
    n -= l;
  }
  while (n--)
    *d++ = 0;
}

static size_t
fill_cb(void * _K, soxr_in_t * data, size_t reqlen)
{
  mix_chan_t * const K = _K;
  int len;

  *data = K->iflt;
  len = !K->ptr ? 0 : reqlen < FLIMAX ? reqlen : FLIMAX;
  if (len > 0)
    chan_flread(K->iflt, K, len);
  return len;
}

static zz_err_t
restart_chan(mix_chan_t * const K)
{
  soxr_error_t err = soxr_clear(K->soxr);
  if (!err)
    err = soxr_set_input_fn(K->soxr, fill_cb, K, FLIMAX);
  if (err) {
    emsg_soxr(K,err);
    K->ptr = 0;
    K->run = 0;
    return E_MIX;
  }
  K->run = !! ( K->ptr = K->pta );
  return E_OK;
}


/* ----------------------------------------------------------------------

   Mixer Push

   ---------------------------------------------------------------------- */

static i16_t
push_soxr(core_t * const P, void * pcm, i16_t N)
{
  soxr_error_t err;
  mix_data_t * const M = (mix_data_t *) P->data;
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
      zz_assert( C->note.ins->len > 0 );

      K->pta = C->note.ins->pcm;
      K->pte = K->pta + C->note.ins->len;
      K->ptl = C->note.ins->lpl ? (K->pte - C->note.ins->lpl) : 0;
      C->note.ins = 0;
      if (restart_chan(K))
        return -1;
      slew = -slew_val;                /* -X+X => 0 on note trigger */

    case TRIG_SLIDE:
      K->rate = rate_of_fp16(C->note.cur, M->rate);
      err = soxr_set_io_ratio(K->soxr, K->rate, slew+slew_val);
      if (err) {
        emsg_soxr(K,err);
        return -1;
      }
      break;
    case TRIG_STOP: K->run = 0;
    case TRIG_NOP:  break;
    default:
      emsg("INTERNAL ERROR: %c: invalid trigger -- %d\n", 'A'+k, C->trig);
      zz_assert( !"wtf" );
      return -1;
    }
  }

  /* Mix float channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    float * restrict flt = M->flt_buf;
    int need;

    for ( need = N; need > 0; ) {
      int i, odone, want;

      if ( ! K->run ) {
        if (k == 0)
          zz_memclr(flt, need * sizeof(*flt));
        break;
      }

      if ( (want = need) > FLOMAX )
        want = FLOMAX;

      odone = soxr_output(K->soxr, K->oflt, want);
      if (odone < 0) {
        emsg_soxr(K,soxr_error(K->soxr));
        return -1;
      }
      zz_assert ( odone <= want );

      if (odone < want) {
        zz_assert ( !K->ptr );
        if (!K->ptr)
          K->run = 0;
      }
      need -= odone;
      if (k == 0)
        for (i=0; i<odone; ++i)
          *flt++ = K->oflt[i];
      else
        for (i=0; i<odone; ++i)
          *flt++ += K->oflt[i];
    }
  }
  /* Convert back to s16 */
  fltoi16(pcm, M->flt_buf, N);

  return N;
}

/* ---------------------------------------------------------------------- */

static void free_soxr(core_t * const P)
{
  mix_data_t * const M = (mix_data_t *) P->data;
  if (M) {
    int k;
    zz_assert( M == P->data );
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->soxr) {
        soxr_delete(K->soxr);
        K->soxr = 0;
      }
    }
    zz_free(&P->data);
  }
}

/* ---------------------------------------------------------------------- */

static zz_err_t init_soxr(core_t * const P, u32_t spr)
{
  zz_err_t ecode = E_SYS;
  int k;
  soxr_error_t err;
  u32_t size, N;

  zz_assert( !P->data );
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
  N = (P->spr + RATE_MIN - 1) / RATE_MIN;
  size = sizeof(mix_data_t) + sizeof(float)*N;
  ecode = zz_calloc(&P->data,size);
  if (likely(E_OK == ecode)) {
    mix_data_t * M = P->data;
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
        /* Set a iorate in the middle. It should not matter much as no
         * note is played anyway. */
        K->rate = .5 * ( M->rate_min + M->rate_max );
        err = soxr_set_io_ratio(K->soxr, K->rate, 0);
      }

      if (err) {
        emsg_soxr(K,err);
        ecode = E_MIX;
      } else {
        ecode = restart_chan(K);
      }
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
