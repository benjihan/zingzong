/**
 * @file   mix_ste_s7s.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-03-12
 * @brief  Atari stereo 8-bit DMA with full separation.
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
#define init_spl(P)   init_spl_s7(P)

#define fast_mix(R,N) fast_s7s(R, M->ata.fast, N)
typedef int16_t pcm_t;

ZZ_STATIC zz_err_t init_ste_s7s(core_t * const, u32_t);
ZZ_STATIC void     free_ste_s7s(core_t * const);
ZZ_STATIC i16_t    push_ste_s7s(core_t * const, void *, i16_t);

mixer_t * mixer_ste_s7s(mixer_t * const M)
{
  M->name = "dma8:2x7bit";
  M->desc = "full stereo 8-bit DMA";
  M->init = init_ste_s7s;
  M->free = free_ste_s7s;
  M->push = push_ste_s7s;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_ste_s7s_s mix_ste_s7s_t;

struct mix_ste_s7s_s {
  ata_t    ata;                         /* generic atari mixer  */
  uint16_t dma;                         /* STe sound DMA mode   */
};

static mix_ste_s7s_t g_ste_s7s;                 /* STE mixer instance */
static uint8_t   luttbl[256];           /* convert u8 to s7 */
static pcm_t    _fifo[FIFOMAX];         /* FIFO buffer */

ZZ_EXTERN_C
void fast_s7s(int16_t * dest, mix_fast_t * voices, int32_t n);

/* ---------------------------------------------------------------------- */

static int16_t pb_play(void)
{
  zz_assert( g_ste_s7s.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    zz_assert( g_ste_s7s.ata.fifo.ro == 0 );
    zz_assert( g_ste_s7s.ata.fifo.wp >  0 );
    zz_assert( g_ste_s7s.ata.fifo.rp == 0 );
    dma8_start();
    return 0;
  } else {
    pcm_t * const pos = dma8_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_ste_s7s.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma8_stop();
}

static void init_dma(uint16_t mode)
{
  dma8_setup(_fifo, &_fifo[g_ste_s7s.ata.fifo.sz], mode);
}

/* Create the 4 voices mix table.
 *
 * All 4 voice samples are added to form an index to this table. The
 * table maximizes the dynamic range to about a third of its size.
 */
static void never_inline init_mix(core_t * P)
{
  /* Not using a dynamic mixing table for now. */
}

/* Unroll instrument loops.
 */
static void never_inline init_spl_s7(core_t * P)
{
  /* stereo: samples stay in s7 format */
  if ( !*luttbl ) {
    int16_t i;
    for ( i=0; i<256; ++i )
      luttbl[i] = (i-128) >> 1;
  }
  vset_unroll(&P->vset,luttbl);
}

ZZ_STATIC i16_t
push_ste_s7s(core_t * const P, void *pcm, i16_t n)
{
  mix_ste_s7s_t * const M = (mix_ste_s7s_t *)P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_ste_s7s );
  zz_assert( IS_ALIGN(FIFOMAX) );
  zz_assert( IS_ALIGN(TEMPMAX) );

  n = ULIGN(n+bias);
  if (n > TEMPMAX)
    n = TEMPMAX;

  /* Reserved because s7s miser is reversed */
  M->ata.swap = (P->lr8 <= 128) << 1;
  play_ata(&M->ata, P->chan, n);

  fast_mix(&_fifo[M->ata.fifo.i1], M->ata.fifo.n1);
  fast_mix(&_fifo[M->ata.fifo.i2], M->ata.fifo.n2);

  return ret;
}

ZZ_STATIC zz_err_t
init_ste_s7s(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_s7s_t * const M = &g_ste_s7s;
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

  init_mix(P);
  init_spl(P);
  init_ata(FIFOMAX,scale);
  init_dma(M->dma);

  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale) );

  return ecode;
}

ZZ_STATIC void
free_ste_s7s(core_t * const P)
{
  if (P->data) {
    mix_ste_s7s_t * const M = (mix_ste_s7s_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_ste_s7s );
      stop_ata(&M->ata);
    }
    P->data = 0;
  }
}
