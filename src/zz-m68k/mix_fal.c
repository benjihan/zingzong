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
# define fast_mix(R,N) slow_fal(R,M->ata.fast,N)
# define MIXALIGN 8
# define FAL_DMA_MODE  (DMA_MODE_MONO|DMA_MODE_16BIT)
typedef int16_t spl_t;

#elif METHOD == 2

# define init_spl(P)   init_spl8(P,0x80)
# define fast_mix(R,N) ilace_fal(R,M->ata.fast,N)
# define MIXALIGN 8
# define FAL_DMA_MODE 0x0380
typedef int32_t spl_t;

#else

# define init_spl(P)   init_spl8(P,0x00)
# define fast_mix(R,N) fast_fal(R,M->ata.fast,N)
# define MIXALIGN 1
# define FAL_DMA_MODE  (DMA_MODE_MONO|DMA_MODE_16BIT)
typedef int16_t spl_t;

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
void fast_fal(int16_t * dest, mix_fast_t * voices, int32_t n);

/* ---------------------------------------------------------------------- */

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

#if METHOD == 1

static uint32_t add_blk( int16_t * restrict dst,
                         const uint8_t * restrict pcm,
                         const uint32_t stp,
                         uint32_t acu)
{
  asm (
    "  move   %[iacu],%%d5            \n"
    "  move   %[istp],%%a4            \n"
    "  swap   %[iacu]                 \n"
    "  swap   %[istp]                 \n"
    "  moveq  #7,%%d6                 \n"
    "0:\n"
    "  add.w  %%a4,%%d5               \n"
    "  addx.w %[istp],%[iacu]         \n"
    "  move.b (%[pcm],%[iacu].w),%%d7 \n"
    "  ext.w  %%d7                    \n"
    "  lsl.w  #6,%%d7                 \n"
    "  add.w  %%d7,(%[dst])+          \n"
    "  dbf    %%d6,0b                 \n"
    "  swap   %[iacu]                 \n"
    "  move.w %%d5,%[iacu]            \n\t"

    : [iacu] "+d" (acu)
    : [istp] "d"  (stp),
      [pcm]  "a"  (pcm),
      [dst]  "a"  (dst)
    : "cc","d7","d6","d5","a4"
    );
  return acu;
}


static void
slow_fal(int16_t * const _d, mix_fast_t * F, int16_t n)
{
  mix_fast_t * const E = F+4;

  zz_assert( IS_ALIGN(n) );
  zz_assert( MIXALIGN == 8 );

  zz_memclr(_d,n<<1);
  for (; F<E; ++F) {
    int16_t * d = _d;
    uint32_t acu = F->dec;

    for ( d = _d; F->cur && d < _d+n ; d += MIXALIGN ) {
      acu = add_blk(d, F->cur, F->xtp, acu);
      F->cur += acu >> 16;
      acu = (uint16_t) acu;

      if (F->cur >= F->end) {
        if (!F->lpl) {
          acu = 0;
          F->cur = 0;
        }
        else {
          uint16_t ovf = F->cur - F->end;
          while (ovf >= F->lpl)
            ovf -= F->lpl;
          F->cur = F->end - F->lpl + ovf;
        }
      }
    }
    F->dec = acu;
  }

}

#elif METHOD == 2

static uint32_t ilace_blk( uint8_t * restrict dst,
                           const uint8_t * restrict pcm,
                           const uint32_t stp,
                           uint32_t acu)
{
  asm (
    "  add.w  %[stp],%[acu]                \n"
    "  swap   %[acu]                       \n"
    "  swap   %[stp]                       \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),(%[dst])   \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),4(%[dst])  \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),8(%[dst])  \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),12(%[dst]) \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),16(%[dst]) \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),20(%[dst]) \n"
    "  addx.l %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),24(%[dst]) \n"
    "  addx.w %[stp],%[acu]                \n"
    "  move.b (%[pcm],%[acu].w),28(%[dst]) \n\t"

    : [acu] "+d" (acu)
    : [stp]  "d" (stp),
      [pcm]  "a" (pcm),
      [dst]  "a" (dst)
    : "cc"
    );
  return acu;
}

static void
ilace_fal(spl_t * _d, mix_fast_t * fast, const int16_t n)
{
  uint8_t k;

  zz_assert( IS_ALIGN(n) );
  zz_assert( MIXALIGN == 8 );

  for ( k=0; k<4; ++k ) {
    mix_fast_t * const F = fast+k;
    uint8_t * d = &k[(uint8_t *)_d];
    uint32_t acu = F->dec;
    int16_t i;

    for ( i = 0; F->cur && i < n ; i += MIXALIGN, d += MIXALIGN*4 ) {
      acu = ilace_blk(d, F->cur, F->xtp, acu);
      fast->cur += (int16_t) acu;
      acu >>= 16;

      if (F->cur >= F->end) {
        if (!F->lpl) {
          acu = 0;
          F->cur = 0;
        }
        else {
          uint16_t ovf = F->cur - F->end;
          while (ovf >= F->lpl)
            ovf -= F->lpl;
          F->cur = F->end - F->lpl + ovf;
        }
      }
    }
    F->dec = acu;
    for (; i < n; ++i, d += 4)
      *d = 0;
  }
}

#endif

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
  mix_fal_t * const M = &g_fal;
  uint32_t refspr;
  uint16_t scale;

  P->mixer_data = M;
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

  /* spr = ZZ_HQ; */

  switch (spr) {
  case ZZ_LQ:
  case ZZ_FQ: spr = 12517; M->dma = 1|FAL_DMA_MODE; break;
  case ZZ_HQ: spr = 50066; M->dma = 3|FAL_DMA_MODE; break;
  default:    spr = 25033; M->dma = 2|FAL_DMA_MODE; break;
  }

  P->spr = spr;
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  init_dma(P);
  init_spl(P);
  init_ata(FIFOMAX,scale);

  dmsg("spr:%lu dma:%02hx scale:%lx\n",
       LU(spr), HU(M->dma), LU(scale) );

  return ecode;
}

static void free_fal(play_t * const P)
{
  if (P->mixer_data) {
    mix_fal_t * const M = (mix_fal_t *)P->mixer_data;
    if ( M ) {
      zz_assert( M == &g_fal );
      stop_ata(&M->ata);
    }
    P->mixer_data = 0;
  }
}
