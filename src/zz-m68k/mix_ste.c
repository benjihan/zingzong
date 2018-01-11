/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA).
 */

#include "../zz_private.h"

static zz_err_t init_ste(play_t * const, u32_t);
static void     free_ste(play_t * const);
static i16_t    push_ste(play_t * const, void *, i16_t);
mixer_t * mixer_ste(mixer_t * const M)
{
  M->name = "stdma8";
  M->desc = "Atari ST via 8-bit DMA";
  M->init = init_ste;
  M->free = free_ste;
  M->push = push_ste;
  return M;
}

/* ---------------------------------------------------------------------- */

/* !!! change in m68k_ste.S as well !!! */
typedef struct mix_fast_s mix_fast_t;
struct mix_fast_s {
  uint8_t *end;                         /*  0 */
  uint8_t *cur;                         /*  4 */
  uint32_t xtp;                         /*  8 */
  uint16_t dec;                         /* 12 */
  uint16_t lpl;                         /* 14 */
};                                      /* 16 bytes */

typedef struct mix_ste_s mix_ste_t;
struct mix_ste_s {
  mix_fast_t fast[4];
  uint8_t * wptr;
  uint16_t scl;
  uint8_t dma_mode;
};

static mix_ste_t g_ste;
static int8_t mixtbl[1024];

/* ---------------------------------------------------------------------- */

#define DMA ((volatile uint8_t *)0xFFFF8900)


enum {
  /* STE Sound DMA registers map. */

  DMA_CNTL  = 0x00,
  /**/
  DMA_ADR   = 0x02,
  DMA_ADR_H = 0x02,
  DMA_ADR_M = 0x04,
  DMA_ADR_L = 0x06,
  /**/
  DMA_CNT   = 0x08,
  DMA_CNT_H = 0x08,
  DMA_CNT_M = 0x0A,
  DMA_CNT_L = 0x0C,
  /**/
  DMA_END   = 0x0E,
  DMA_END_H = 0x0E,
  DMA_END_M = 0x10,
  DMA_END_L = 0x12,
  /**/
  DMA_MODE  = 0x20,

  /* Constants */
  DMA_CNTL_ON   = 0x01,
  DMA_CNTL_LOOP = 0x02,
  DMA_MODE_MONO = 0x80,
};

/* static */ void * dma_position(void)
{
  void * pos;
  asm volatile(
    "   lea 0x8908.w,%%a0     \n\t"
    "   moveq #-1,%[pos]      \n\t"
    "0:"
    "   move.l %[pos],%%d1     \n\t"
    "   moveq #0,%[pos]        \n\t"
    "   move.b (%%a0),%[pos]   \n\t"
    "   swap %[pos]            \n\t"
    "   movep.w 2(%%a0),%[pos] \n\t"
    "   cmp.l %[pos],%%d1      \n\t"
    "   bne.s 0b               \n\t"
    : [pos] "=d" (pos)
    :
    : "cc","d1","a0");
    return pos;
}

/* static */ void write_dma_ptr(int16_t reg, const void * adr)
{
  asm volatile(
    " swap %[adr]              \n\t"
    " move.b (%[hw]),%[adr]    \n\t"
    " swap %[adr]              \n\t"
    " movep.w 2(%[hw]),%[adr]  \n\t"
    :
    : [hw] "a" (0x8900+reg), [adr] "d" (adr)
    : "cc");
}

/* static */ void stop_dma()
{
  DMA[DMA_CNTL] &= ~(DMA_CNTL_ON|DMA_CNTL_LOOP);
}

/* static */ void setup_dma(void * adr, void * end, uint8_t mode)
{
  stop_dma();
  write_dma_ptr(DMA_ADR, adr);
  write_dma_ptr(DMA_END, end);
  DMA[DMA_MODE]  = mode;
}

static never_inline void init_mix_table()
{
  /* GB: smooth saturation */
  if (!mixtbl[0]) {
    int8_t * tbl = mixtbl+512;
    int16_t i, j, x, x0;
    /* Do the linear part */
    for (i=0; /* i<512 */; ++i) {
      x = (i*5)>>3;
      if (x >= 112) break;
      tbl[~i] = ~ (tbl[i] = x);
    }
    for (j=i, x0=x; i<512; ++i ) {
      x = x0 + (( (i-j)*3 >> 6));
      tbl[~i] = ~ (tbl[i] = x);
      zz_assert(x <= 127);
    }
    zz_assert(x == 127);
  }
}

static i16_t push_ste(play_t * const P, void *pcm, i16_t n)
{
  zz_assert( 0 );
  return -1;
}

static zz_err_t init_ste(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_t * const M = &g_ste;
  uint32_t refspr;

  init_mix_table();

  P->mixer_data = M;
  zz_memclr(M,sizeof(*M));
  refspr = mulu(P->song.khz,1000u);

  if (spr == 0)
    spr = refspr;

  if (spr > ZZ_HQ) {
    if (spr < 8850)
      spr = ZZ_LQ;
    else if (spr < 17701)
      spr = ZZ_FQ;
    else if (spr < 35402)
      spr = ZZ_MQ;
    else
      spr = ZZ_HQ;
  }

  switch (spr) {
  case ZZ_LQ: spr =  6258; M->dma_mode = 0; break;
  case ZZ_FQ: spr = 12517; M->dma_mode = 1; break;
  case ZZ_HQ: spr = 50066; M->dma_mode = 3; break;
  default:    spr = 25033; M->dma_mode = 2; break;
  }
  P->spr = spr;
  M->scl = ( divu(refspr<<13,spr) + 1 ) >> 1;

  /* prepare_ste(); */
  /* prepare_insts(P); */

  return ecode;
}

static void free_ste(play_t * const P)
{
  stop_dma();
  P->mixer_data = 0;
}
