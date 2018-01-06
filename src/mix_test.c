/**
 * @file   mix_none.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Low quality no interpolation mixer.
 */

#ifdef WITH_TEST

#define BLKSZ 16                        /* TEST 1 */
#define ZSTR(A) #A
#define XSTR(A) ZSTR(A)

#define NAME "int"
#define METH "test"
#define SYMB mixer_test

#if WITH_TEST == 1
#define DESC "testing pre-indexed block of " XSTR(BLKSZ)
#elif WITH_TEST == 2
#define DESC "testing interleaved voices"
#else
#error unknown WITH_TEST value
#endif

#define ZZ_DBG_PREFIX "(mix-" METH  ") "
#include "zz_private.h"

#include <string.h>

typedef struct mix_fp_s mix_fp_t;
typedef struct mix_chan_s mix_chan_t;

struct mix_chan_s {
  uint8_t *pcm, *end;
  u32_t idx, lpl, len, xtp;
};

struct mix_fp_s {
  mix_chan_t chan[4];
};



static void
mix_add1(mix_chan_t * const restrict K, u8_t k, int16_t * restrict b, int n)
{
  const uint8_t * const pcm = (const uint8_t *)K->pcm;

#if WITH_TEST == 1
  const u32_t rnd = 0; /* (1<<(FP-1))-1; */
  u32_t offset[BLKSZ];

#elif WITH_TEST == 2
  n >>= 2;

#endif

  if (n <= 0 || !K->pcm)
    return;

#if WITH_TEST == 1
  if (1) {
    int i;
    u32_t idx = 0;
    for (i=0; i<BLKSZ; ++i, idx += K->xtp)
      offset[i] = idx >> FP;
  }
  do {
    u32_t idx;
    int m,i;
    zz_assert( K->idx >= 0 && K->idx < K->len );

    m = n;
    if (m > BLKSZ)
      m = BLKSZ;
    n -= m;

    idx = (K->idx+rnd) >> FP;
    K->idx += K->xtp * m;
    for (i=0; i<m; ++i) {
      const u32_t off = offset[i];
      zz_assert( &pcm[idx+off] < K->end );
      *b++ += (pcm[idx+off]-128) << 6;
    }

    if (K->idx >= K->len) {
      if (!K->lpl) {
        /* Instrument does not loop */
        K->pcm = 0;                     /* This only is mandatory */
        break;
      }
      else {
        const u32_t off = K->len - K->lpl;  /* loop start index */
        u32_t ovf = K->idx - K->len;
        if (ovf >= K->lpl) ovf %= K->lpl;
        K->idx = off+ovf;
        zz_assert( K->idx >= off && K->idx < K->len );
      }
    }
  } while (n > 0);

#elif WITH_TEST == 2

  b += k;
  do {
    *b = (pcm[ K->idx >> FP ] - 128) << 8;
    b += 4;
    K->idx += K->xtp;
    if (K->idx >= K->len) {
      if (!K->lpl) {
        K->pcm = 0; break;
      }
      else {
        const u32_t off = K->len - K->lpl;  /* loop start index */
        u32_t ovf = K->idx - K->len;
        if (ovf >= K->lpl) ovf %= K->lpl;
        K->idx = off+ovf;
        zz_assert( K->idx >= off && K->idx < K->len );
      }
    }
  } while ( --n > 0 );

#else
#error invalid WITH_TEST value
#endif


}

static u32_t xstep(u32_t stp, u32_t ikhz, u32_t ohz)
{
  /* stp is fixed-point 16
   * res is fixed-point FP
   * 1000 = 2^3 * 5^3
   */
  u64_t const tmp = (FP > 12)
    ? (u64_t) stp * ikhz * (1000u >> (16-FP)) / ohz
    : (u64_t) stp * ikhz * (1000u >> 3) / (ohz << (16-FP-3))
    ;
  u32_t const res = tmp;
  zz_assert( (u64_t)res == tmp );       /* check overflow */
  zz_assert( res );

#if WITH_TEST == 2                      /* INTERLEAVED */
  return res << 2;
#else
  return res;
#endif
}

static i16_t
push_cb(play_t * const P, void * restrict pcm, i16_t N)
{
  mix_fp_t * const M = (mix_fp_t *)P->mixer_data;
  int k;

  zz_assert( P );
  zz_assert( M );
  zz_assert( pcm );
  zz_assert( N != 0 );
  zz_assert( N > 0 );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;
    const u8_t trig = C->trig;

    C->trig = TRIG_NOP;
    switch (trig) {
    case TRIG_NOTE:
      zz_assert( C->note.ins == P->vset.inst+C->curi );
      K->idx = 0;
      K->pcm = C->note.ins->pcm;
      K->len = C->note.ins->len << FP;
      K->lpl = C->note.ins->lpl << FP;
      K->end = C->note.ins->end + K->pcm;

    case TRIG_SLIDE:
      K->xtp = xstep(C->note.cur, P->song.khz, P->spr);
      break;

    case TRIG_STOP: K->pcm = 0;
    case TRIG_NOP:
      break;
    default:
      zz_assert( !"wtf" );
      return -1;
    }
  }

  /* Clear mix buffer */
  zz_memclr(pcm,N<<1);

  /* Add voices */
  for (k=0; k<4; ++k)
    mix_add1(M->chan+k, k, pcm, N);

#if WITH_TEST == 2                      /* INTERLEAVED */
  N = N & ~3;
#endif

  return N;
}

static void * local_calloc(u32_t size, zz_err_t * err)
{
  void * ptr = 0;
  *err =  zz_memnew(&ptr,size,1);
  return ptr;
}


static zz_err_t init_cb(play_t * const P, u32_t spr)
{
  zz_err_t ecode = E_OK;
  mix_fp_t * M = local_calloc(sizeof(mix_fp_t), &ecode);
  if (likely(M)) {
    int i;
    zz_assert( !P->mixer_data );
    P->mixer_data = M;
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

    for (i=0; i<256; ++i)
      P->tohw[i] = i;
    vset_unroll(&P->vset,P->tohw);
  }

  return ecode;
}

static void free_cb(play_t * const P)
{
  zz_memdel(&P->mixer_data);
}

mixer_t SYMB =
{
  NAME ":" METH, DESC, init_cb, free_cb, push_cb
};

#endif
