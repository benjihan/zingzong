/**
 * @file   mix_common.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  common parts for none, lerp and qerp mixer.
 */

#ifndef NAME
#error NAME should be defined
#endif

#ifndef DESC
#error DESC should be defined
#endif

#if !defined (FP) || (FP > 16) || (FP < 7)
# error undefined of invalid FP
#endif

#define xtr(X) str(X)
#define str(X) #X

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

#define SETPCM() OPEPCM(=);
#define ADDPCM() OPEPCM(+=);

static inline void
mix_add1(mix_chan_t * const restrict K, int16_t * restrict b, int n)
{
  const uint8_t * const pcm = (const uint8_t *)K->pcm;
  const u32_t stp = K->xtp;
  u32_t idx = K->idx;

  if (n <= 0 || !K->pcm)
    return;

  zz_assert( K->pcm );
  zz_assert( K->end > K->pcm );
  zz_assert( K->xtp > 0 );
  zz_assert( K->idx >= 0 && K->idx < K->len );

  if (!K->lpl) {
    /* Instrument does not loop */
    do {
      ADDPCM();
      /* Have reach end ? */
      if (idx >= K->len) {
        K->pcm = 0;                     /* This only is mandatory */
        break;
      }
    } while(--n);
  } else {
    const u32_t off = K->len - K->lpl;  /* loop start index */
    /* Instrument does loop */
    do {
      ADDPCM();
      /* Have reach end ? */
      if (idx >= K->len) {
        u32_t ovf = idx - K->len;
        if (ovf >= K->lpl) ovf %= K->lpl;
        /* dmsg("loop at %lx -> %lx\n", idx>>FP, (off+ovf)>>FP); */
        idx = off+ovf;
        zz_assert( idx >= off && idx < K->len );
      }
    } while(--n);
  }
  K->idx = idx;
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
  return res;
}

static i16_t
push_cb(core_t * const P, void * restrict pcm, i16_t N)
{
  mix_fp_t * const M = (mix_fp_t *)P->data;
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

    /* if (trig != TRIG_NOP) */
    /*   dmsg("%c %s trig:%d\n",'A'+k,NAME,trig); */

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
    mix_add1(M->chan+k, pcm, N);

  return N;
}

static void * local_calloc(u32_t size, zz_err_t * err)
{
  void * ptr = 0;
  *err =  zz_memnew(&ptr,size,1);
  return ptr;
}

static zz_err_t init_cb(core_t * const P, u32_t spr)
{
  zz_err_t ecode = E_OK;
  mix_fp_t * M = local_calloc(sizeof(mix_fp_t), &ecode);
  if (likely(M)) {
    zz_assert( !P->data );
    P->data = M;
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

    ecode = init_meth(P);
  }

  return ecode;
}

static void free_cb(core_t * const P)
{
  zz_memdel(&P->data);
}

mixer_t SYMB =
{
  NAME ":" METH, DESC, init_cb, free_cb, push_cb
};
