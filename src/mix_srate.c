/**
 * @file   mix_srate.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using samplerate.
 */

#ifndef NO_SRATE

#include "zz_private.h"
#include <string.h>
#include <samplerate.h>


#define USER_SUPPLY 1                  /* 0:no user supply function */
#define RATIO(X) (1.0/(double)(X))

#define F32MAX (MIXBLK*8)
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)


typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int id;

  SRC_STATE * st;
#if !USER_SUPPLY
  SRC_DATA    sd;
#endif
  double   rate;

  uint8_t *ptr;
  uint8_t *ptl;
  uint8_t *pte;
  uint8_t *end;
  /* double   stp; */

  int      ilen;
  int      imax;
  float    iflt[FLIMAX];

  int      omax;
  float    oflt[FLOMAX];

};

struct mix_data_s {
  int quality;

  double   rate, irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];

  float flt_buf[1];                     /* /!\ always last /!\ */
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
iorate(const uint_t fp16, const double irate, const double orate)
{
  /* return ldexp(fp16,-16) * irate / orate; */
  return (double)fp16 * irate / (65536.0*orate);
}

static inline double
rate_of_fp16(const uint_t fp16, const double rate) {
  return (double)fp16 * rate;
}

static void chan_flread(float * const d, mix_chan_t * const K, const int n)
{
  if (!n) return;
  assert(n > 0);
  assert(n < VSET_UNROLL);

  /* dmsg("flread(%c,%i)\n", K->id, n); */

  if (!K->ptr)
    memset(d, 0, n*sizeof(float));
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

#if USER_SUPPLY
static long
fill_cb(void *_K, float **data)
{
  mix_chan_t * const restrict K = _K;

  assert(K->imax == FLIMAX);
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

static int
push_cb(play_t * const P)
{
  int err;
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
    const int slew_val = 0;   /* GB: Nothing but 0 seems to work ! */

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
      slew -= -slew_val;               /* -X+X == 0 on note trigger */

    case TRIG_SLIDE:
      K->rate = rate_of_fp16(C->note.cur, M->rate);
      assert(K->rate >= M->rate_min);
      assert(K->rate <= M->rate_max);
#if !USER_SUPPLY
      K->sd.src_ratio = RATIO(K->rate);
#endif
      dmsg("%c: trig=%d stp=%08X %.3lf\n",
           C->id, C->trig, C->note.cur, K->rate);
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
      if (k==0)
        memset(flt,0,sizeof(float)*N);
      continue;
    }

    while (need > 0) {
      int i, done, want = need;

      assert( K->omax == FLOMAX );
      assert( K->imax == FLIMAX );

      if (want > K->omax)
        want = K->omax;

#if !USER_SUPPLY
      emsg("srate: only user supply supported ATM\n");
      return E_MIX;

#else
      done = src_callback_read(K->st, RATIO(K->rate), want, K->oflt);
      if (done < 0)
        return emsg_srate(K,src_error(K->st));

      if (done > 0) {
        zero = 0;
      } else {
        if (++zero > 31) {
          emsg("%c: too many loop without data -- %u\n",K->id, zero);
          return E_MIX;
        }
      }

      need -= done;
      if (k == 0)
        for (i=0; i<done; ++i)
          *flt++ = K->oflt[i];
      else
        for (i=0; i<done; ++i)
          *flt++ += K->oflt[i];
    }
    assert( need == 0 );
    assert( flt-N == M->flt_buf );
  }
  /* Convert back to s16 */
  fltoi16(P->mix_buf, M->flt_buf, N);

  return E_OK;
#endif
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
  dmsg("free_mixer_data %p\n",M);
  if (M) {
    int k;
    assert(M == P->mixer_data);
    for (k=0; k<4; ++k) {
      mix_chan_t * const K = M->chan+k;
      if (K->st)
        src_delete (K->st);
      K->st = 0;
    }
    zz_free("srate-data",&P->mixer_data);
  }
}

/* ---------------------------------------------------------------------- */

static int init_srate(play_t * const P, const int quality)
{
  int k, ecode = E_SYS;
  mix_data_t * M;
  const uint_t size = sizeof(mix_data_t) + sizeof(float)*P->pcm_per_tick;
  assert(!P->mixer_data);
  P->mixer_data = M = zz_calloc("srate-data", size);
  if (M) {
    M->quality = quality;

    dmsg("init %s: %s\n",
         src_get_name (quality),
         src_get_description(quality));

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

#if USER_SUPPLY
      K->st = src_callback_new(fill_cb, quality, 1, &err, K);
#else
      K->sd.data_in   = K->iflt;
      K->sd.data_out  = K->oflt;

      K->rate = rate_of_fp16((P->song.stepmin+P->song.stepmax)>>1, M->rate);
      K->sd.src_ratio = RATIO(K->rate);

      K->st = src_new (quality, 1, &err);
#endif
      if (!K->st)
        return emsg_srate(K,err);

      ecode = err
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

#define DECL_SRATE_MIXER(Q,QQ,D) \
  static int init_##Q(play_t * const P) \
  {\
    return init_srate(P, SRC_##QQ);\
  }\
  mixer_t mixer_srate_##Q = \
  {\
    "src:" XTR(Q), D, init_##Q, free_cb, push_cb, pull_cb\
  }

DECL_SRATE_MIXER(best,SINC_BEST_QUALITY,
                 "band limited sinc (best quality)");
DECL_SRATE_MIXER(medium,SINC_MEDIUM_QUALITY,
                 "band limited sinc (medium quality)");
DECL_SRATE_MIXER(fast,SINC_FASTEST,
                 "band limited sinc (fastest quality)");
DECL_SRATE_MIXER(zero,ZERO_ORDER_HOLD,
                 "zero order hold (very fast, LQ");
DECL_SRATE_MIXER(linear,LINEAR,
                 "linear (very fast, LQ)");

#endif /* #ifndef NO_SRATE */
