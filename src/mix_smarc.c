/**
 * @file   mix_smarc.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using smarc.
 */

#if WITH_SMARC == 1

#include "zz_private.h"
#include <string.h>
#include <smarc.h>

#define F32MAX (MIXBLK*8)
#define FLIMAX (F32MAX)
#define FLOMAX (F32MAX)

typedef struct mix_data_s mix_data_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int id;

  struct PState  * state;
  struct PFilter * filter;

  double   rate;

  uint8_t *ptr;
  uint8_t *ptl;
  uint8_t *pte;
  uint8_t *end;

  int      ilen;
  int      imax;
  double   iflt[FLIMAX];

  int      omax;
  double   oflt[FLOMAX];
};

struct mix_data_s {
  double   rate, irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];
  float flt_buf[1];                     /* /!\ always last /!\ */
};

/* ----------------------------------------------------------------------

   Read input PCM

   ---------------------------------------------------------------------- */

static void chan_flread(double * const d, mix_chan_t * const K, const int n)
{
  if (!n) return;
  assert(n > 0);
  assert(n < VSET_UNROLL);

  /* dmsg("flread(%c,%i)\n", K->id, n); */

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

   smarc helpers

   ---------------------------------------------------------------------- */


static int
emsg_smarc(const mix_chan_t * const K, smarc_error_t err)
{
  emsg("smarc: %c: %s\n", K->id, smarc_strerror(err));
  return E_MIX;
}

#if USER_SUPPLY
/* Supply data to be resampled. */
static size_t
fill_cb(void * _K, smarc_in_t * data, size_t reqlen)
{
  mix_chan_t * const K = _K;
  int len = reqlen;

  assert(len <= K->imax);              /* Limited by set_input_fb() */
  assert(K->imax == FLIMAX);           /* For now keep it simple */
  /* assert(K->ilen == 0);                /\* For now keep it simple *\/ */

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
  smarc_error_t err = 0;

  K->ilen = 0;
  K->imax = FLIMAX;
  K->omax = FLOMAX;

  smarc_reset_pstate(K->state, K->filter);

#if USER_SUPPLY
  if (!err)
    err = smarc_set_input_fn(K->smarc, fill_cb, K, K->imax);
#endif

  return err ? emsg_smarc(K,err) : E_OK;
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
  smarc_error_t err;
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
      err = smarc_set_io_ratio(K->smarc, K->rate, slew+slew_val);

      dmsg("%c: trig=%d stp=%08X %.3lf\n",
           C->id, C->trig, C->note.cur, K->rate);

      if (err)
        return emsg_smarc(K,err);
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
      int i, done;
      size_t want = need, idone, odone;

      assert( K->omax == FLOMAX );
      assert( K->imax == FLIMAX );

      if (want > K->omax)
        want = K->omax;


#if USER_SUPPLY

      idone = 0;
      odone =
        smarc_output(
          K->smarc,
          K->oflt,
          want);
      err = smarc_error(K->smarc);
      if (err || odone < 0)
        return emsg_smarc(K,err);
      done = odone;

#else

      /* Fill input when it's empty */
      if (!K->ilen) {
        K->ilen = K->imax;
        chan_flread(K->iflt, K, K->ilen);
      }

      err = smarc_process(
        K->smarc,
        K->iflt + K->imax - K->ilen,
        K->ilen,
        &idone,

        K->oflt,
        want,
        &odone);

      /* dmsg("mix(%c need:%u i:%u/%u o:%u/%u) -> %s\n", */
      /*      K->id, (uint_t) need, (uint_t) idone, (uint_t) K->ilen, */
      /*      (uint_t) odone, (uint_t) want, smarc_strerror(err)); */

      if (err)
        return emsg_smarc(K,err);

      done = odone;
      K->ilen -= idone;

      assert( K->ilen >= 0 && K->ilen <= K->imax );
      assert( done >= 0 && done <= K->omax );

#endif
      if (idone+odone != 0) {
        zero = 0;
      } else {
        dmsg("mix(%c(%u) need:%u i:%u/%u/%u o:%u/%u/%u) -> %s\n",
             K->id, P->tick,
             (uint_t) need,
             (uint_t) idone, (uint_t) K->ilen, (uint_t) K->imax,
             (uint_t) odone, (uint_t) want, (uint_t) K->omax,
             smarc_strerror(err));
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
      if (K->filter) {
        smarc_destroy_pfilter(K->filter);
        K->filter = 0;
      }

      if (K->state) {
        smarc_destroy_pstate(K->state);
        K->state = 0;
      }

    }

    zz_free("smarc-data",&P->mixer_data);
  }
}

/* ---------------------------------------------------------------------- */

static int init_smarc(play_t * const P, const int quality)
{
  int k, ecode = E_SYS;

  smarc_error_t err;
  mix_data_t * M;
  const uint_t size = sizeof(mix_data_t) + sizeof(float)*P->pcm_per_tick;
  assert(!P->mixer_data);
  P->mixer_data = M = zz_calloc("smarc-data", size);
  if (M) {
    M->qspec    = smarc_quality_spec(quality, SMARC_VR);
    M->ispec    = smarc_io_spec(SMARC_FLOAT32_I,SMARC_FLOAT32_I);
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
      K->smarc = smarc_create(
        M->rate_max, /* input sampling rate */
        1.0,         /* output sampling rate */
        1,           /* num channels */
        &err,        /* error */
        &M->ispec,   /* io specs */
        &M->qspec,   /* quality spec */
        NULL);       /* runtime spec */


/**
 * Build a PFilter with given parameter. Given pointer must be free by
 * calling destroy_pfilter function
 *
 * - fsin (IN) : frequency samplerate of input signal
 * - fsout (IN) : desired frequency samplerate for output signal
 *
 * - bandwidth (IN) : bandwidth to keep, real in 0..1. For example,
 *                    0.95 means that 95% of maximum possible
 *                    bandwidth is kept. Too high value may results in
 *                    impossible filter to design.
 * - rp (IN) : Passband ripple factor, in dB.
 * - rs (IN) : Stopband ripple factor, in dB.
 *
 * - tol (IN) : samplerate conversion error tolerance. Usual value for
 *              tol is 0.000001. Usual frequencies conversion has
 *              exact samplerate conversion, but for unusual
 *              frequencies, in particular when fsin or fsout are
 *              large prime numbers, the user should accept a
 *              tolerance otherwise the filter may be impossible to
 *              design.
 *
 * - userratios (IN) : ratios to use to build multistage samplerate converter, in following format :
 *                     'L1/M2 L2/M2 ...'. This parameter is optional.
 *
 * - searchfastconversion (IN) : if 1 try to search fastest conversion
 *                               stages. This may fail.  if 0 use safe
 *                               default conversion stages.
 */
      K->filter =
        smarc_init_pfilter(
          int fsin,
          const int fsout,
          double bandwidth, double rp, double rs,
          double tol, const char* userratios, int searchfastconversion);


      K->state  = smarc_init_pstate(K->filter);

      if (K->smarc) {
        /* Set a iorate in the middle. It should matter much as no
         * note is played anyway. */
        K->rate = .5 * ( M->rate_min + M->rate_max );
        err = smarc_set_io_ratio(K->smarc, K->rate, 0);
      }

      ecode = err
        ? emsg_smarc(K,err)
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

mixer_t mixer_smarc =
{
  "smarc", "polyphase filter", init_cb, free_cb, push_cb, pull_cb
}


#endif /* WITH_SMARC == 1 */
