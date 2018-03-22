/**
 * @file   mix_ste_dnr.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari mono 8-bit DMA with boosted dynamic range.
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

#define fast_mix(R,N) fast_dnr(mixtbl, R, _temp, M->ata.fast, N)
typedef int8_t spl_t;

ZZ_STATIC zz_err_t init_ste_dnr(core_t * const, u32_t);
ZZ_STATIC void     free_ste_dnr(core_t * const);
ZZ_STATIC i16_t    push_ste_dnr(core_t * const, void *, i16_t);

mixer_t * mixer_ste_dnr(mixer_t * const M)
{
  M->name = "dma8:mono-dnr";
  M->desc = "boosted DNR mono 8-bit DMA";
  M->init = init_ste_dnr;
  M->free = free_ste_dnr;
  M->push = push_ste_dnr;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_ste_dnr_s mix_ste_dnr_t;

struct mix_ste_dnr_s {
  ata_t    ata;                         /* generic atari mixer  */
  uint16_t dma;                         /* STe sound DMA mode   */
};

static mix_ste_dnr_t g_ste_dnr;                 /* STE mixer instance */
static int8_t    mixtbl[1024];          /* mix 4 voices together */
static int16_t  _temp[TEMPMAX];         /* intermediate mix buffer */
static spl_t    _fifo[FIFOMAX];         /* FIFO buffer */

ZZ_EXTERN_C
void fast_dnr(int8_t  * Tmix, int8_t     * dest,
              int16_t * temp, mix_fast_t * voices,
              int32_t n);

/* ---------------------------------------------------------------------- */

static int16_t pb_play(void)
{
  zz_assert( g_ste_dnr.ata.fifo.sz == FIFOMAX );

  if ( ! dma8_running() ) {
    zz_assert( g_ste_dnr.ata.fifo.ro == 0 );
    zz_assert( g_ste_dnr.ata.fifo.wp >  0 );
    zz_assert( g_ste_dnr.ata.fifo.rp == 0 );

    dma8_start();

    dmsg("DMA cntl:%02hx mode:%02hx/%02hx %p\n",
         HU(DMA[DMA_CNTL]), HU(DMA[DMA_MODE]), HU(g_ste_dnr.dma), dma8_position());
    return 0;
  } else {
    spl_t * const pos = dma8_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_ste_dnr.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma8_stop();
}

static void init_dma(uint16_t mode)
{
  dma8_setup(_fifo, &_fifo[g_ste_dnr.ata.fifo.sz], mode);
}

/* Create the 4 voices mix table.
 *
 * All 4 voice samples are added to form an index to this table. The
 * table maximizes the dynamic range to about a third of its size.
 */
static void never_inline init_mix(core_t * P)
{
  if (!mixtbl[0]) {
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
  }
}

/* Unroll instrument loops.
 */
static void never_inline init_spl(core_t * P)
{
  /* mono: samples stay in u8 format */
  vset_unroll(&P->vset,0);
}

ZZ_STATIC i16_t
push_ste_dnr(core_t * const P, void *pcm, i16_t n)
{
  mix_ste_dnr_t * const M = (mix_ste_dnr_t *)P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_ste_dnr );
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

ZZ_STATIC zz_err_t
init_ste_dnr(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_dnr_t * const M = &g_ste_dnr;
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
  case ZZ_LQ: spr =  6258; M->dma = 0|DMA_MODE_MONO; break;
  case ZZ_FQ: spr = 12517; M->dma = 1|DMA_MODE_MONO; break;
  case ZZ_HQ: /* spr = 50066; M->dma = 3|DMA_MODE_MONO; break; */
  default:    spr = 25033; M->dma = 2|DMA_MODE_MONO; break;
  }

  P->spr = spr;
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  init_mix(P);
  init_spl(P);
  init_ata(FIFOMAX,scale);
  init_dma(M->dma);

  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale) );

  return ecode;
}

ZZ_STATIC void
free_ste_dnr(core_t * const P)
{
  if (P->data) {
    mix_ste_dnr_t * const M = (mix_ste_dnr_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_ste_dnr );
      stop_ata(&M->ata);
    }
    P->data = 0;
  }
}
