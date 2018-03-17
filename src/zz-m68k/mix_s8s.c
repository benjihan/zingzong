/**
 * @file   mix_s8s.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-03-12
 * @brief  Atari STe mixer (8-bit DMA stereo).
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define MIXALIGN      1
#define TICKMAX       256
#define FIFOMAX       ((TICKMAX*3)>>1)
#define TEMPMAX       TICKMAX
#define ALIGN(X)      ((X)&-(MIXALIGN))
#define ULIGN(X)      ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X)   ((X) == ALIGN(X))
#define init_spl(P)   init_spl(P)

#define fast_mix(R,N) fast_s8s(Tmix[0], R, M->ata.fast, N)
#define STE_DMA_MODE 0
typedef int16_t spl_t;

static zz_err_t init_s8s(core_t * const, u32_t);
static void     free_s8s(core_t * const);
static i16_t    push_s8s(core_t * const, void *, i16_t);

mixer_t * mixer_s8s(mixer_t * const M)
{
  M->name = "stdma8:stereo";
  M->desc = "Atari ST via stereo 8-bit DMA";
  M->init = init_s8s;
  M->free = free_s8s;
  M->push = push_s8s;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_s8s_s mix_s8s_t;

struct mix_s8s_s {
  ata_t    ata;                         /* generic atari mixer  */
  uint16_t dma;                         /* STe sound DMA mode   */
};

static mix_s8s_t g_s8s;                 /* STE mixer instance */
static spl_t    _fifo[FIFOMAX];         /* FIFO buffer */
static spl_t     Tmix[2][510];          /* Blending tables */
static uint16_t  Tmix_lr8;              /* Tmix current setup */

ZZ_EXTERN_C
void fast_s8s(spl_t * Tmix, spl_t * dest, mix_fast_t * voices, int32_t n);

/* ---------------------------------------------------------------------- */

static int16_t pb_play(void)
{
  zz_assert( g_s8s.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    dma_start(_fifo, &_fifo[g_s8s.ata.fifo.sz], g_s8s.dma);
    dmsg("DMA cntl:%02hx mode:%02hx/%02hx %p\n",
         HU(DMA[DMA_CNTL]), HU(DMA[DMA_MODE]),
         HU(g_s8s.dma), dma_position());
    return 0;
  } else {
    spl_t * const pos = dma_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_s8s.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma_stop();
}

static void never_inline init_dma(core_t * P)
{
  dma_stop();
}

/* Unroll instrument loops.
 */
static void never_inline init_spl(core_t * P)
{
  vset_unroll(&P->vset,0);
}

static void never_inline init_mix(uint16_t lr8)
{
  int16_t x;

  /* 2 U8 voices added => {0..510} */
  for (x=0; x<510; ++x) {
    int16_t v = x-255;                  /* -255 .. 255 */
    int16_t w = v << 7;

    asm (
      "muls   %[f],%[v]  \n\t"
      "asr.l  #1,%[v]      \n\t"
      "sub.w  %[v],%[w]    \n\t"
      "move.w %[v],-(%%sp) \n\t"
      "move.b (%%sp)+,%[w] \n\t"
      "move.w %[w],-(%%sp) \n\t"
      "move.b (%%sp)+,%[v] \n\t"
      : [v] "+d" (v), [w] "+d" (w)
      : [f] "iSd" (lr8)
      : "cc" );
    Tmix[0][x] = v;
    Tmix[1][x] = w;
  }
  Tmix_lr8 = lr8;
}

static i16_t push_s8s(core_t * const P, void *pcm, i16_t n)
{
  mix_s8s_t * const M = (mix_s8s_t *)P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_s8s );
  zz_assert( IS_ALIGN(FIFOMAX) );
  zz_assert( IS_ALIGN(TEMPMAX) );

  if (P->lr8 != Tmix_lr8)
    init_mix(P->lr8);

  n = ULIGN(n+bias);
  if (n > TEMPMAX)
    n = TEMPMAX;
  play_ata(&M->ata, n);

  fast_mix(&_fifo[M->ata.fifo.i1], M->ata.fifo.n1);
  fast_mix(&_fifo[M->ata.fifo.i2], M->ata.fifo.n2);

  return ret;
}

static zz_err_t init_s8s(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_s8s_t * const M = &g_s8s;
  uint32_t refspr;
  uint16_t scale;

  P->data = M;
  zz_memclr(M,sizeof(*M));
  refspr = mulu(P->song.khz,1000u);

  if (spr == 0)
    spr = refspr;

  if (spr > ZZ_HQ) {
    if (spr < 7000)
      spr = ZZ_LQ;
    else if (spr < 14000)
      spr = ZZ_FQ;
    else if (spr < 28000)
      spr = ZZ_MQ;
    else
      spr = ZZ_HQ;
  }

  switch (spr) {
  case ZZ_LQ: spr =  6258; M->dma = 0|STE_DMA_MODE; break;
  case ZZ_FQ: spr = 12517; M->dma = 1|STE_DMA_MODE; break;
  case ZZ_HQ: spr = 50066; M->dma = 3|STE_DMA_MODE; break;
  default:    spr = 25033; M->dma = 2|STE_DMA_MODE; break;
  }

  P->spr = spr;
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  init_dma(P);
  init_mix(P->lr8);
  init_spl(P);
  init_ata(FIFOMAX,scale,2);
  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale) );

  return ecode;
}

static void free_s8s(core_t * const P)
{
  if (P->data) {
    mix_s8s_t * const M = (mix_s8s_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_s8s );
      stop_ata(&M->ata);
    }
    P->data = 0;
  }
}
