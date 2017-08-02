/**
 * @file   mix_common.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  common parts for mix_qerp.c and mix_none.c
 */

#ifndef NAME
#error NAME should be defined
#endif

#ifndef DESC
#error DESC should be defined
#endif

#ifndef MIXBLK
# error MIXBLK should be defined
#endif

#if !defined (FP) || (FP > 16) || (FP < 7)
# error undefined of invalid FP define
#endif

/* #define LSH(V,S)      ( (S) >= 0 ? (V) << (S) : (V) >> -(S) ) */
/* #define RSH(V,S)      ( (S) >= 0 ? (V) >> (S) : (V) << -(S) ) */

#define xtr(X) str(X)
#define str(X) #X

#include <string.h>

typedef struct mix_fp_s mix_fp_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  int8_t *pcm, *end;
  uint_t  idx, lpl, len, xtp;
};

struct mix_fp_s {
  mix_chan_t chan[4];
};

static inline void
mix_gen(mix_fp_t * const M, const int k, int16_t * restrict b, int n)
{
  mix_chan_t * const restrict K = M->chan+k;
  const int8_t * const pcm = (const int8_t *)K->pcm;

  assert(n > 0 && n <= MIXBLK);
  assert(k >= 0 && k < 4);

  if (K->xtp) {
    uint_t idx = K->idx;
    const uint_t stp = K->xtp;

    assert(K->pcm);
    assert(K->end > K->pcm);
    assert(idx >= 0 && idx < K->len);

    /* Add N PCM */
    do {
      ADDPCM();
    } while(--n);

    assert ( K->pcm+(idx>>FP) <= K->end );

    /* Have reach end ? */
    if (idx >= K->len) {
      /* Have loop ? */
      if (!K->lpl) {
        idx = K->xtp = 0;
      }
      else {
        const uint_t ovf = (idx - K->len) % K->lpl;
        idx = K->len - K->lpl + ovf;
        assert( idx >= K->len-K->lpl && idx < K->len );
      }
    }

    assert(idx >= 0 && idx < K->len);
    K->idx = idx;
  }
}

static void
mix_add1(mix_fp_t * const M, const int k, int16_t * b)
{
  mix_gen(M,k,b,MIXBLK);
}

static void mix_addN(mix_fp_t * const M, const int k, int16_t * b, const int n)
{
  mix_gen(M,k,b,n);
}


static uint_t xstep(uint_t stp, uint_t ikhz, uint_t ohz)
{
  /* stp is fixed-point 16
   * res is fixed-point FP
   * 1000 = 2^3 * 5^3
   */
  uint64_t tmp = (FP > 12)
    ? (uint64_t) stp * ikhz * (1000u >> (16-FP)) / ohz
    : (uint64_t) stp * ikhz * (1000u >> 3) / (ohz << (16-FP-3))
    ;
  uint_t res = tmp;
  assert( (uint64_t)res == tmp); /* check overflow */
  assert( res );
  return res;
}

static int
push_cb(play_t * const P)
{
  mix_fp_t * const M = (mix_fp_t *)P->mixer_data;
  int16_t  * restrict b;
  int k, n;

  assert(P);
  assert(M);

  /* Clear mix buffer */
  zz_memclr(P->mix_buf,P->pcm_per_tick<<1);

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;

    switch (C->trig) {
    case TRIG_NOTE:

      assert(C->note.ins);
      K->idx = 0;
      K->pcm = (int8_t *) P->vset.inst[C->curi].pcm;
      K->len = C->note.ins->len << FP;
      K->lpl = C->note.ins->lpl << FP;
      K->end = C->note.ins->end + K->pcm;
      C->note.ins = 0;
    case TRIG_SLIDE:
      K->xtp = xstep(C->note.cur, P->song.khz, P->spr);
      break;
    case TRIG_STOP: K->xtp = 0;
    case TRIG_NOP:
      break;
    default:
      assert(!"wtf");
      return E_MIX;
    }
  }

  /* Mix per block of MIXBLK samples */
  for (b=P->mix_buf, n=P->pcm_per_tick;
       n >= MIXBLK;
       b += MIXBLK, n -= MIXBLK)
    for (k=0; k<4; ++k)
      mix_add1(M, k, b);

  if (n > 0)
    for (k=0; k<4; ++k)
      mix_addN(M, k, b, n);

  return E_OK;
}

static int init_cb(play_t * const P)
{
  int ecode = E_SYS;
  mix_fp_t * const M = zz_calloc("mixer-data", sizeof(mix_fp_t));

  if (M) {
    P->mixer_data = M;
    ecode = E_OK;
  }
  return ecode;
}

static void free_cb(play_t * const P)
{
  zz_free("mixer-data",&P->mixer_data);
}

mixer_t SYMB =
{
  NAME ":" METH, DESC, init_cb, free_cb, push_cb
};
