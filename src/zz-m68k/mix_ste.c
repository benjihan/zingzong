/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA mono).
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
#define init_spl(P)   init_spl_u8(P)

#define fast_mix(R,N) fast_ste(mixtbl, R, _temp, M->ata.fast, N)
#define STE_DMA_MODE DMA_MODE_MONO
typedef int8_t pcm_t;

static zz_err_t init_ste(core_t * const, u32_t);
static void     free_ste(core_t * const);
static i16_t    push_ste(core_t * const, void *, i16_t);

mixer_t * mixer_ste(mixer_t * const M)
{
  M->name = "stdma8:mono";
  M->desc = "Atari ST via mono 8-bit DMA";
  M->init = init_ste;
  M->free = free_ste;
  M->push = push_ste;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_ste_s mix_ste_t;

struct mix_ste_s {
  ata_t    ata;                         /* generic atari mixer  */
  uint16_t dma;                         /* STe sound DMA mode   */
};

static mix_ste_t g_ste;                 /* STE mixer instance */
static int8_t    mixtbl[1024];          /* mix 4 voices together */
static int16_t  _temp[TEMPMAX];         /* intermediate mix buffer */
static pcm_t    _fifo[FIFOMAX];         /* FIFO buffer */

ZZ_EXTERN_C
void fast_ste(int8_t  * Tmix, int8_t     * dest,
              int16_t * temp, mix_fast_t * voices,
              int32_t n);

/* ---------------------------------------------------------------------- */

static int16_t pb_play(void)
{
  zz_assert( g_ste.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    dma_start(_fifo, &_fifo[g_ste.ata.fifo.sz], g_ste.dma);
    dmsg("DMA cntl:%02hx mode:%02hx/%02hx %p\n",
         HU(DMA[DMA_CNTL]), HU(DMA[DMA_MODE]), HU(g_ste.dma), dma_position());
    return 0;
  } else {
    pcm_t * const pos = dma_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_ste.ata.fifo.sz );
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

/* Create the 4 voices mix table.
 *
 * All 4 voice samples are added to form an index to this table. The
 * table maximizes the dynamic range to about a third of its size.
 */
static void never_inline init_mix(core_t * P)
{
  if (!mixtbl[0]) {
#if 0
    int i;
    for (i=0; i<1024; ++i)
      mixtbl[i] = (i>>2) - 128;
#else
    int8_t * tbl = mixtbl+512;
    int16_t i, j, x, x0;
    /* Dynamic range */
    for (i=0; /* i<512 */; ++i) {
      x = (i*5)>>3;
      if (x >= 112) break;
      tbl[~i] = ~ (tbl[i] = x);
    }
    /* Saturated range */
    for (j=i, x0=x; i<512; ++i ) {
      x = x0 + (( (i-j)*3 >> 6));
      tbl[~i] = ~ (tbl[i] = x);
      zz_assert(x <= 127);
    }
    zz_assert(x == 127);
#endif
  }
}

/* Unroll instrument loops.
 */
static void never_inline init_spl_u8(core_t * P)
{
  /* mono: samples stay in u8 format */
  vset_unroll(&P->vset,0);
}

static i16_t push_ste(core_t * const P, void *pcm, i16_t n)
{
  mix_ste_t * const M = (mix_ste_t *)P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_ste );
  zz_assert( IS_ALIGN(FIFOMAX) );
  zz_assert( IS_ALIGN(TEMPMAX) );

  n = ULIGN(n+bias);
  if (n > TEMPMAX)
    n = TEMPMAX;
  play_ata(&M->ata, P->chan, n);

  fast_mix(&_fifo[M->ata.fifo.i1], M->ata.fifo.n1);
  fast_mix(&_fifo[M->ata.fifo.i2], M->ata.fifo.n2);

  return ret;
}

static zz_err_t init_ste(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_t * const M = &g_ste;
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
  init_mix(P);
  init_spl(P);
  init_ata(FIFOMAX,scale);

  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale) );

  return ecode;
}

static void free_ste(core_t * const P)
{
  if (P->data) {
    mix_ste_t * const M = (mix_ste_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_ste );
      stop_ata(&M->ata);
    }
    P->data = 0;
  }
}
