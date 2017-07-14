/**
 * @file   mix_soxr.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High quality mixer using libsoxr.
 */

#ifndef NO_SOXR

#include "zz_private.h"
#include <string.h>
#include <soxr.h>
#include <math.h>

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
  double   stp;

  int      ilen;
  int      imax;
  float    iflt[FLIMAX];

  int      omax;
  float    oflt[FLOMAX];
};

struct mix_data_s {
  soxr_quality_spec_t qspec;
  soxr_io_spec_t      ispec;

  double   irate, orate, rate_min, rate_max;
  mix_chan_t chan[4];

  float flt_buf[1];                     /* /!\ always last /!\ */
};

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

#if 0
SOXR_QQ   /* 'Quick' cubic interpolation. */
SOXR_LQ   /* 'Low' 16-bit with larger rolloff. */
SOXR_MQ   /* 'Medium' 16-bit with medium rolloff. */
SOXR_HQ   /* 'High quality'. */
SOXR_VHQ  /* 'Very high quality'. */
#endif

#define USER_SUPPLY 0

#if 0
soxr_error_t soxr_clear(soxr_t); /* Ready for fresh signal, same config. */
void         soxr_delete(soxr_t);  /* Free resources. */
soxr_error_t soxr_set_io_ratio(soxr_t, double io_ratio, size_t slew_len);
soxr_error_t soxr_set_input_fn(/* Set (or reset) an input function.*/
  soxr_t resampler,            /* As returned by soxr_create. */
  soxr_input_fn_t,             /* Function to supply data to be resampled.*/
  void * input_fn_state,       /* If needed by the input function. */
  size_t max_ilen);            /* Maximum value for input fn. requested_len.*/
typedef size_t /* data_len */
(* soxr_input_fn_t)(         /* Supply data to be resampled. */
  void * input_fn_state,     /* As given to soxr_set_input_fn (below). */
  soxr_in_t * data,          /* Returned data; see below. N.B. ptr to ptr(s)*/
  size_t requested_len);     /* Samples per channel, >= returned data_len.

  data_len  *data     Indicates    Meaning
   ------- -------   ------------  -------------------------
     !=0     !=0       Success     *data contains data to be
                                   input to the resampler.
      0    !=0 (or   End-of-input  No data is available nor
           not set)                shall be available.
      0       0        Failure     An error occurred whilst trying to
                                   source data to be input to the resampler.  */

/* then repeatedly call: */
size_t /*odone*/ soxr_output(/* Resample and output a block of data.*/
  soxr_t resampler,            /* As returned by soxr_create. */
  soxr_out_t data,             /* App-supplied buffer(s) for resampled data.*/
  size_t olen);                /* Amount of data to output; >= odone. */
#endif

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */


/* ----------------------------------------------------------------------

   float <-> int conversion

   ---------------------------------------------------------------------- */

static float i8tofl_lut[256];

/** Initialize int8_t to float PCM look up table. */
static void i8tofl_init(void)
{
  if (i8tofl_lut[128] == 0.0) {
    const float sc = 3.0 / (4.0*4.0*128.0);
    int i;
    for (i=-128; i<128; ++i)
      i8tofl_lut[i&0xFF] = sc * (float)i;
  }
}

/* Convert int8_t PCM buffer to float PCM */
static void i8tofl(float * const d, const uint8_t * const s, const int n)
{
  int i;
  for (i=0; i<n; ++i) {
    d[i] = i8tofl_lut[s[i]];
  }
}

/* Convert normalized float PCM buffer to in16_t PCM */
static void fltoi16(int16_t * const d, const float * const s, const int n)
{
  const float sc = 32768.0; int i;

  /* $$$ Slow conversion. Need some improvement once everything work */
  for (i=0; i<n; ++i) {
    const float f = s[i] * sc;
    /* const */ int   v = (int) f;

    if (v < -32768) {
      /* dmsg("[%d] %.3f %d\n",i,f,v); */
      v = -32768;
    } else if (v >= 32768) {
      /* dmsg("[%d] %.3f %d\n", i, f, v); */
      v = 32767;
    }

    assert (v >= -32768 );
    assert (v <   32768 );

    d[i] = v;
  }
}

/* ----------------------------------------------------------------------

   Read input PCM

   ---------------------------------------------------------------------- */


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

/* ----------------------------------------------------------------------

   soxr helpers

   ---------------------------------------------------------------------- */


static int
emsg_soxr(const mix_chan_t * const K, soxr_error_t err)
{
  emsg("soxr: %c: %s\n", K->id, soxr_strerror(err));
  return E_MIX;
}

#if USER_SUPPLY
/* Supply data to be resampled. */
static size_t
fill_cb(void * _K, soxr_in_t * data, size_t reqlen)
{
  mix_chan_t * const K = _K;
  int len = reqlen;

  assert(len <= K->imax);              /* Limited by set_input_fb() */
  assert(K->imax == FLIMAX);           /* For now keep it simple */
  assert(K->ilen == 0);                /* For now keep it simple */

  if (len > K->imax)
    len = K->imax;
  *data = K->iflt;
  K->ilen = len;
  chan_flread(K->iflt, K, len);

  /* dmsg("fill(%c,%u) => %u\n",K->id,(uint_t)reqlen, len); */

  return len;
}
#endif

static int
restart_chan(mix_chan_t * const K)
{
  soxr_error_t err = 0;

  K->ilen = 0;
  K->imax = FLIMAX;
  K->omax = FLOMAX;
  err = soxr_clear(K->soxr);

#if 0
  /* only with supplied data func */
  if (!err)
    err = soxr_set_input_fn(K->soxr, fill_cb, K, K->imax);
#endif

  return err ? emsg_soxr(K,err) : E_OK;
}

static inline double
iorate_of(const uint_t fp16, const double irate, const double orate)
{
  return ldexp(fp16,-16) * irate / orate;
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
      K->rate = iorate_of(C->note.cur, M->irate, M->orate);
      assert(K->rate >= M->rate_min);
      assert(K->rate <= M->rate_max);
      err = soxr_set_io_ratio(K->soxr, K->rate, slew+slew_val);

      dmsg("%c: trig=%d stp=%08X %.3lf\n",
           C->id, C->trig, C->note.cur, K->rate);

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

      /* Fill input when it's empty */
      if (!K->ilen) {
        K->ilen = K->imax;
        chan_flread(K->iflt, K, K->ilen);
      }

      err = soxr_process(
        K->soxr,
        K->iflt + K->imax - K->ilen,
        K->ilen,
        &idone,

        K->oflt,
        want,
        &odone);

      /* dmsg("mix(%c need:%u i:%u/%u o:%u/%u) -> %s\n", */
      /*      K->id, (uint_t) need, (uint_t) idone, (uint_t) K->ilen, */
      /*      (uint_t) odone, (uint_t) want, soxr_strerror(err)); */

      if (err)
        return emsg_soxr(K,err);

      done = odone;
      K->ilen -= idone;

      assert( K->ilen >= 0 && K->ilen <= K->imax );
      assert( done >= 0 && done <= K->omax );

      if (idone+odone != 0) {
        zero = 0;
      } else {
        dmsg("mix(%c(%u) need:%u i:%u/%u/%u o:%u/%u/%u) -> %s\n",
             K->id, P->tick,
             (uint_t) need,
             (uint_t) idone, (uint_t) K->ilen, (uint_t) K->imax,
             (uint_t) odone, (uint_t) want, (uint_t) K->omax,
             soxr_strerror(err));
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

  soxr_error_t err;
  mix_data_t * M;
  const uint_t size = sizeof(mix_data_t) + sizeof(float)*P->pcm_per_tick;
  assert(!P->mixer_data);
  P->mixer_data = M = zz_calloc("soxr-data", size);
  if (M) {
    M->qspec   = soxr_quality_spec(quality, SOXR_VR);
    M->ispec   = soxr_io_spec(SOXR_FLOAT32_I,SOXR_FLOAT32_I);
    M->irate   = (double) P->song.khz * 1000.0;
    M->orate   = (double) P->spr;
    M->rate_min = iorate_of(P->song.stepmin, M->irate, M->orate);
    M->rate_max = iorate_of(P->song.stepmax, M->irate, M->orate);

    dmsg("iorates : %.3lf .. %.3lf\n", M->rate_min, M->rate_max);
    dmsg("irate   : %.3lf\n", M->irate);
    dmsg("orate   : %.3lf\n", M->orate);

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
    i8tofl_init();

  return ecode;
}

/* ---------------------------------------------------------------------- */

#define XONXAT(A,B) A##B
#define CONCAT(A,B) XONXAT(A,B)
#define XTR(A) #A
#define STR(A) XTR(A)

#define DECL_SOXR_MIXER(Q,QQ, D) static int init_##Q(play_t * const P) { return init_soxr(P, SOXR_##QQ); } mixer_t mixer_soxr_##Q = { "soxr:" XTR(Q), D, init_##Q, free_cb, push_cb, pull_cb }

DECL_SOXR_MIXER(qq,QQ,"soxr 'Quick' cubic interpolation");
DECL_SOXR_MIXER(lq,LQ,"soxr 'Low' 16-bit with larger rolloff");
DECL_SOXR_MIXER(mq,MQ,"soxr 'Medium' 16-bit with medium rolloff");
DECL_SOXR_MIXER(hq,HQ,"soxr 'High quality'");
DECL_SOXR_MIXER(vhq,VHQ,"soxr 'Very High quality'");

#endif /* #ifndef NO_SOXR */
