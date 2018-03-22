/**
 * @file   mix_ste_lrb.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-03-12
 * @brief  Atari stereo 8-bit DMA with L/R blending.
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define MIXALIGN      1
#define TICKMAX       256
#define FIFOMAX       (TICKMAX*3)
#define TEMPMAX       TICKMAX
#define ALIGN(X)      ((X)&-(MIXALIGN))
#define ULIGN(X)      ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X)   ((X) == ALIGN(X))
#define init_spl(P)   init_spl(P)

#define fast_mix(R,N) fast_lrb(Tmix[0], R, M->ata.fast, N)
typedef int16_t spl_t;

ZZ_STATIC zz_err_t init_ste_lrb(core_t * const, u32_t);
ZZ_STATIC void     free_ste_lrb(core_t * const);
ZZ_STATIC i16_t    push_ste_lrb(core_t * const, void *, i16_t);

mixer_t * mixer_ste_lrb(mixer_t * const M)
{
  M->name = "dma8:blend-stereo";
  M->desc = "blended stereo 8-bit DMA";
  M->init = init_ste_lrb;
  M->free = free_ste_lrb;
  M->push = push_ste_lrb;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_ste_lrb_s mix_ste_lrb_t;

struct mix_ste_lrb_s {
  ata_t    ata;                         /* generic atari mixer  */
  uint16_t dma;                         /* STe sound DMA mode   */
};

static mix_ste_lrb_t g_ste_lrb;                 /* STE mixer instance */
static spl_t    _fifo[FIFOMAX];         /* FIFO buffer */
static spl_t     Tmix[2][510];          /* Blending tables */
static uint16_t  Tmix_lr8;              /* Tmix current setup */

ZZ_EXTERN_C
void fast_lrb(spl_t * Tmix, spl_t * dest, mix_fast_t * voices, int32_t n);

/* ---------------------------------------------------------------------- */

static int16_t pb_play(void)
{
  zz_assert( g_ste_lrb.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    zz_assert( g_ste_lrb.ata.fifo.ro == 0 );
    zz_assert( g_ste_lrb.ata.fifo.wp >  0 );
    zz_assert( g_ste_lrb.ata.fifo.rp == 0 );
    dma8_start();
    return 0;
  } else {
    spl_t * const pos = dma8_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_ste_lrb.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma8_stop();
}

static void init_dma(uint16_t mode)
{
  dma8_setup(_fifo, &_fifo[g_ste_lrb.ata.fifo.sz], mode);
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
  int32_t v9 = muls(-255,lr8);
  int16_t w8 = -255 << 7;

  /* 2 U8 voices added => {0..510} */
  for (x=0; x<510; ++x, v9 += lr8, w8 += 128) {
    int16_t v = v9 >> 1;
    int16_t w = w8 - v;
    asm (
      "move.w %[v],-(%%sp) \n\t"
      "move.b (%%sp)+,%[w] \n\t"
      "move.w %[w],-(%%sp) \n\t"
      "move.b (%%sp)+,%[v] \n\t"
      : [v] "+d" (v), [w] "+d" (w) : : "cc");
    Tmix[0][x] = w;                     /* L-pair table */
    Tmix[1][x] = v;                     /* R-pair table */

#ifndef NDEBUG
    dmsg("L/R[%03hx]=%04hx/%04hx\n", HU(x), HU(Tmix[0][x]), HU(Tmix[1][x]));
    zz_assert( (uint8_t) (w>>8) == (uint8_t)v );
    zz_assert( (uint8_t) (v>>8) == (uint8_t)w );
    if (1) {
      int16_t r;
      r = (int16_t)(int8_t)v + (int16_t)(int8_t)w;
      zz_assert( r >= -128 && r <=  127 );
    }
#endif
  }

  Tmix_lr8 = lr8;
}

ZZ_STATIC i16_t
push_ste_lrb(core_t * const P, void *pcm, i16_t n)
{
  mix_ste_lrb_t * const M = (mix_ste_lrb_t *)P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_ste_lrb );
  zz_assert( IS_ALIGN(FIFOMAX) );
  zz_assert( IS_ALIGN(TEMPMAX) );

  if (P->lr8 != Tmix_lr8)
    init_mix(P->lr8);

  n = ULIGN(n+bias);
  if (n > TEMPMAX)
    n = TEMPMAX;
  play_ata(&M->ata, P->chan, n);

  fast_mix(&_fifo[M->ata.fifo.i1], M->ata.fifo.n1);
  fast_mix(&_fifo[M->ata.fifo.i2], M->ata.fifo.n2);

  return ret;
}

ZZ_STATIC zz_err_t
init_ste_lrb(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_lrb_t * const M = &g_ste_lrb;
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
    else if (spr < 35000)
      spr = ZZ_MQ;
    else
      spr = ZZ_HQ;
  }

  switch (spr) {
  case ZZ_LQ: spr =  6258; M->dma = 0; break;
  case ZZ_FQ: spr = 12517; M->dma = 1; break;
  case ZZ_HQ: /* spr = 50066; M->dma = 3; break; */
  default:    spr = 25033; M->dma = 2; break;
  }

  P->spr = spr;
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  init_mix(P->lr8);
  init_spl(P);
  init_ata(FIFOMAX,scale);
  init_dma(M->dma);

  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale));

  return ecode;
}

ZZ_STATIC void
free_ste_lrb(core_t * const P)
{
  if (P->data) {
    mix_ste_lrb_t * const M = (mix_ste_lrb_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_ste_lrb );
      stop_ata(&M->ata);
    }
    P->data = 0;
  }
}
