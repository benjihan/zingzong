/**
 * @file   mix_fal.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari Falcon mixer (16-bit DMA).
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define METHOD  0

#define TICKMAX 256
#define FIFOMAX (TICKMAX*3)
#define TEMPMAX TICKMAX

#define ALIGN(X)    ((X)&-(MIXALIGN))
#define ULIGN(X)    ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X) ( (X) == ALIGN(X) )

#if METHOD == 1

# define init_spl(P)   init_spl8(P,0x80)
# define fast_mix(R,N) fast_4x8(R,M->ata.fast,N)
# define MIXALIGN 1
# define FAL_DMA_MODE  (0x300|DMA_MODE_MONO)
typedef int32_t spl_t;

#else

# define init_spl(P)   init_spl8(P,0x00)
# define fast_mix(R,N) fast_fal(R,M->ata.fast,N)
# define MIXALIGN 1
# define FAL_DMA_MODE  (DMA_MODE_16BIT)
typedef int32_t spl_t;

#endif

static zz_err_t init_fal(play_t * const, u32_t);
static void     free_fal(play_t * const);
static i16_t    push_fal(play_t * const, void *, i16_t);

mixer_t * mixer_fal(mixer_t * const M)
{
  M->name = "stdma16";
  M->desc = "Atari ST via 16-bit DMA";
  M->init = init_fal;
  M->free = free_fal;
  M->push = push_fal;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_fal_s mix_fal_t;

struct mix_fal_s {
  ata_t   ata;                          /* generic atari mixer  */
  uint16_t dma;                         /* Track/sound DMA mode   */
};

static mix_fal_t g_fal;                 /* Falcon mixer instance */
static spl_t _fifo[FIFOMAX];            /* FIFO buffer */

ZZ_EXTERN_C
void fast_fal(spl_t * dest, mix_fast_t * voices, int32_t n);

ZZ_EXTERN_C
void fast_4x8(spl_t * dest, mix_fast_t * voices, int32_t n);

static inline
/* @retval     1 success
 * @retval  -129 already locked
 */
int32_t Locksnd(void)
{
  int32_t ret;
  asm volatile(
    "move.w  #0x80,-(%%a7) \n\t"
    "trap    #14           \n\t"
    "addq.w  #2,%%a7       \n\t"
    : [ret] "=d" (ret));
  return ret;
}

static inline
/* @retval     0 success
 * @retval  -128 not locked
 */
int32_t Unlocksnd(void)
{
  int32_t ret;
  asm volatile(
    "move.w  #0x81,-(%%a7) \n\t"
    "trap    #14           \n\t"
    "addq.w  #2,%%a7       \n\t"
    : [ret] "=d" (ret));
  return ret;
}

#if 0
/* Falcon prescalers (unused ATM) */
static uint16_t prescale[16] = {
  0 /*STe*/, 49170, 32880, 24585, 19668, 16390,
  0/*14049*/, 12292, 0/*10927 */, 9834, 0/*8940*/,
  8195, 0/*7565*/, 0/*7024*/, 0/*6556*/, 0/*6146*/
};
#endif

static int16_t pb_play(void)
{
  zz_assert( g_fal.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    dma_start(_fifo, &_fifo[g_fal.ata.fifo.sz], g_fal.dma);
    dmsg("DMA cntl:%04hx mode:%04hx/%04hx %p\n",
         HU(DMAW(DMAW_CNTL)), HU(DMAW(DMAW_MODE)),
         HU(g_fal.dma), dma_position());
    return 0;
  } else {
    spl_t * const pos = dma_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_fal.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma_stop();
}

static void never_inline init_dma(play_t * P)
{
  dma_stop();
}

/* Unroll instrument loops (samples stay in u8 format).
 */
static void never_inline init_spl8(play_t * P, const u8_t sign)
{
  int16_t k;
  for (k=0; k<256; ++k)
    P->tohw[k] = k^sign;
  vset_unroll(&P->vset,P->tohw);
}


static i16_t push_fal(play_t * const P, void *pcm, i16_t n)
{
  mix_fal_t * const M = (mix_fal_t *)P->mixer_data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M );
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

static zz_err_t init_fal(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_fal_t * M = &g_fal;
  uint32_t refspr;
  uint16_t scale;

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
  case ZZ_LQ:                     /* Falcon does not have 6Khz mode */
  case ZZ_FQ: spr = 12517; M->dma = 1|FAL_DMA_MODE; break;
  case ZZ_HQ: spr = 50066; M->dma = 3|FAL_DMA_MODE; break;
  default:    spr = 25033; M->dma = 2|FAL_DMA_MODE; break;
  }

  P->spr = spr;
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  if ( Locksnd() != 1 ) {
    M = 0;
    ecode = E_MIX;
  } else {
    init_dma(P);
    init_spl(P);
    init_ata(FIFOMAX,scale);
    dmsg("spr:%lu dma:%02hx scale:%lx\n",
         LU(spr), HU(M->dma), LU(scale) );
  }
  P->mixer_data = M;
  return ecode;
}

static void free_fal(play_t * const P)
{
  if (P->mixer_data) {
    mix_fal_t * const M = (mix_fal_t *)P->mixer_data;
    if ( M ) {
      zz_assert( M == &g_fal );
      stop_ata(&M->ata);
      Unlocksnd();
    }
    P->mixer_data = 0;
  }
}
