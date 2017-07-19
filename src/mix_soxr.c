/**
 * @file   mix_soxr.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using soxr.
 */

#if WITH_SOXR == 1

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
  float flt_buf[1];
};

/* ----------------------------------------------------------------------

   Read input PCM

   ---------------------------------------------------------------------- */

static void
chan_flread(float * const d, mix_chan_t * const K, const int n)
{
  if (!n) return;
  assert(n > 0);
  assert(n < VSET_UNROLL);

  if (!K->ptr)
    memset(d, 0, n*sizeof(*d));
  else {
    assert(K->ptr < K->pte);
    i8tofl(d, K->ptr, n);

    if ( (K->ptr += n) >= K->pte ) {
      if (!K->ptl) {
        K->ptr = 0;
      } else {
        assert(K->ptl < K->pte);
        K->ptr = &K->ptl[ ( K->ptr - K->pte ) % ( K->pte - K->ptl ) ];
        assert(K->ptr >= K->ptl);
        assert(K->ptr <  K->pte);
      }
    }
    assert(K->ptr < K->pte);
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

  assert(len <= K->imax);              /* Limited by set_input_fb() */
  assert(K->imax == FLIMAX);           /* For now keep it simple */
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
iorate(const uint_t fp16, const double irate, const double orate)
{
  /* return ldexp(fp16,-16) * irate / orate; */
  return (double)fp16 * irate / (65536.0*orate);
}

static inline double
rate_of_fp16(const uint_t fp16, const double rate) {
  return (double)fp16 * rate;
}

/* ----------------------------------------------------------------------

   Mixer Push

   ---------------------------------------------------------------------- */

static int
push_cb(play_t * const P)
{
  soxr_error_t err;
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  const int N = P->pcm_per_tick;
  int k;

  assert(P);
  assert(M);

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;
    int slew = 0;
    const int slew_val = N>>1;

    switch (C->trig) {

    case TRIG_NOTE:
      assert(C->note.ins);

      K->ptr = C->note.ins->pcm;
      K->pte = K->ptr + C->note.ins->len;
      K->ptl = C->note.ins->lpl ? (K->pte - C->note.ins->lpl) : 0;
      K->end = K->ptr + C->note.ins->end;
      C->note.ins = 0;
      if (restart_chan(K))
        return E_MIX;
      slew = -slew_val;                /* -X+X == 0 on note trigger */

    case TRIG_SLIDE:
      K->rate = rate_of_fp16(C->note.cur, M->rate);
      assert(K->rate >= M->rate_min);
      assert(K->rate <= M->rate_max);
      err = soxr_set_io_ratio(K->soxr, K->rate, slew+slew_val);

      /* dmsg("%c: trig=%d stp=%08X %.3lf\n", */
      /*      C->id, C->trig, C->note.cur, K->rate); */

      if (err)
        return emsg_soxr(K,err);
      break;
    case TRIG_STOP: K->ptr = 0;
    case TRIG_NOP:  break;
    default:
      emsg("INTERNAL ERROR: %c: invalid trigger -- %d\n", 'A'+k, C->trig);
      assert(!"wtf");
      return E_MIX;
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
        memset(flt,0,sizeof(float)*N);
      continue;
    }

    while (need > 0) {
      int i;
      size_t want = need, idone, odone;

      assert( K->omax == FLOMAX );
      assert( K->imax == FLIMAX );

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
      if (err || odone < 0)
        return emsg_soxr(K,err);
#else
      /* Fill input when it's empty */
      if (!K->ilen)
        chan_flread(K->icur = K->iflt, K, K->ilen = K->imax);
      assert( K->ilen > 0 );
      assert( K->ilen <= K->omax );
      assert( &K->iflt[K->omax] == &K->icur[K->ilen] );
      err = soxr_process(
        K->soxr,
        K->icur, K->ilen, &idone,
        K->oflt, want, &odone);

      if (err)
        return emsg_soxr(K,err);

      K->ilen -= idone;
      K->icur += idone;

      assert( K->ilen >= 0 && K->ilen <= K->imax );
      assert( odone >= 0 && odone <= want && odone <= K->omax );
#endif

      if (idone+odone != 0) {
        zero = 0;
      } else {
        dmsg("mix(%c(%u) need:%u i:%u/%u/%u o:%u/%u/%u) -> %s\n",
             K->id, P->tick,
             (uint_t) need,
             (uint_t) idone, (uint_t) K->ilen, (uint_t) K->imax,
             (uint_t) odone, (uint_t) want, (uint_t) K->omax,
             soxr_strerror(err));
        if (++zero > 7) {
          emsg("%c: too many loop without data -- %u\n",K->id, zero);
          return E_MIX;
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
    assert( need == 0 );
    assert( flt-N == M->flt_buf );
  }

  /* Convert back to s16 */
  fltoi16(P->mix_buf, M->flt_buf, N);

  return E_OK;
}

/* ---------------------------------------------------------------------- */

static int pull_cb(play_t * const P, int n)
{
  return E_666;
}

/* ---------------------------------------------------------------------- */

static void free_cb(play_t * const P)
{
  mix_data_t * const M = (mix_data_t *) P->mixer_data;
  if (M) {
    int k;
    assert(M == P->mixer_data);
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->soxr) {
        soxr_delete(K->soxr);
        K->soxr = 0;
      }
    }

    zz_free("soxr-data",&P->mixer_data);
  }
}

/* ---------------------------------------------------------------------- */

static int init_soxr(play_t * const P, const int quality)
{
  int k, ecode = E_SYS;
  const int N = P->pcm_per_tick;
  soxr_error_t err;
  mix_data_t * M;
  const uint_t size = sizeof(mix_data_t) + sizeof(float)*N;
  assert(!P->mixer_data);
  assert(N > 0);
  assert(sizeof(float) == 4);
  P->mixer_data = M = zz_calloc("soxr-data", size);
  if (M) {
    M->qspec    = soxr_quality_spec(quality, SOXR_VR);
    M->ispec    = soxr_io_spec(SOXR_FLOAT32_I,SOXR_FLOAT32_I);
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
    free_cb(P);
  else
    i8tofl(0,0,0);                    /* trick to pre-init LUT */

  return ecode;
}

/* ---------------------------------------------------------------------- */

#define XONXAT(A,B) A##B
#define CONCAT(A,B) XONXAT(A,B)
#define XTR(A) #A
#define STR(A) XTR(A)

#define DECL_SOXR_MIXER(Q,QQ, D) \
  static int init_##Q(play_t * const P)\
  {\
    return init_soxr(P, SOXR_##QQ);\
  }\
  mixer_t mixer_soxr_##Q =\
  {\
    "soxr" , D, init_##Q, free_cb, push_cb, pull_cb\
  }

/* GB: when using variable rate soxr ignore quality. */
DECL_SOXR_MIXER(hq,HQ,"high quality variable rate");

#endif /* WITH_SOXR == 1 */
