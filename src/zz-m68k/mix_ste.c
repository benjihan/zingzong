/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA).
 */

#include "../zz_private.h"

#undef FP
#define FP 16

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
  int8_t * wptr;
  uint16_t scl;
  uint8_t dma_mode;
};

static mix_ste_t g_ste;
static int8_t  mixtbl[1024];            /* mix 4 voices together */
static int8_t  mixbuf[2048];            /* >2002 (50066/50*2) */
static int16_t temp[1024];              /* $$$ Only for C test */

/* ---------------------------------------------------------------------- */

#define DMA ((volatile uint8_t *)0xFFFF8900)

enum {
  /* STE Sound DMA registers map. */

  DMA_CNTL  = 0x01,
  /**/
  DMA_ADR   = 0x03,
  DMA_ADR_H = 0x03,
  DMA_ADR_M = 0x05,
  DMA_ADR_L = 0x07,
  /**/
  DMA_CNT   = 0x09,
  DMA_CNT_H = 0x09,
  DMA_CNT_M = 0x0B,
  DMA_CNT_L = 0x0D,
  /**/
  DMA_END   = 0x0F,
  DMA_END_H = 0x0F,
  DMA_END_M = 0x11,
  DMA_END_L = 0x13,
  /**/
  DMA_MODE  = 0x21,

  /* Constants */
  DMA_CNTL_ON   = 0x01,
  DMA_CNTL_LOOP = 0x02,
  DMA_MODE_MONO = 0x80,
};

static void * dma_position(void)
{
  void * pos;
  asm volatile(
    "   lea 0x8909.w,%%a0     \n\t"
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

static void write_dma_ptr(volatile uint8_t * reg, const void * adr)
{
  asm volatile(
    " swap %[adr]              \n\t"
    " move.b %[adr],(%[hw])    \n\t"
    " swap %[adr]              \n\t"
    " movep.w %[adr],2(%[hw])  \n\t"
    :
    : [hw] "a" (reg), [adr] "d" (adr)
    : "cc");
}

static void stop_dma(void)
{
  DMA[DMA_CNTL] = 0;
}

static void start_dma(void * adr, void * end, uint8_t mode)
{
  stop_dma();
  write_dma_ptr(DMA+DMA_ADR, adr);
  write_dma_ptr(DMA+DMA_END, end);
  DMA[DMA_MODE] = mode;
  DMA[DMA_CNTL] = 3;                    /* run in loop */
}

static void never_inline init_dma(play_t * P)
{
  stop_dma();
}

/* Create the 4 voices mix table.
 *
 * All 4 voice samples are added to form an index to this table. The
 * table maximizes the dynamic range to about a third of its size.
 */
static void never_inline init_mix(play_t * P)
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

/* Unroll instrument loops (samples stay in u8 format).
 */
static void never_inline init_spl(play_t * P)
{
  u8_t k;
  for (k=0; k<256; ++k)
    P->tohw[k] = k;
  vset_unroll(&P->vset,P->tohw);
}

static void
fast_ste(const int8_t mixtbl[], int8_t * d, int16_t * temp,
         mix_fast_t * fast, int16_t n)
{
  mix_fast_t * const tsaf = fast+4;
  int16_t * const pmet = temp + n;

  zz_assert( n > 0 );
  if ( n <= 0 ) return;

  zz_memclr(temp,n<<1);

  for (; fast < tsaf; ++fast) {
    int32_t acu;
    if (fast->cur) {
      acu = fast->dec;

      do {
        zz_assert(fast->cur < fast->end);

        *temp++ += *fast->cur;

        acu += fast->xtp;
        fast->cur += acu >> FP;
        acu &= (1l<<FP) - 1;

        if (fast->cur >= fast->end) {
          if (!fast->lpl) {
            acu = 0;
            fast->cur = 0;
            break;
          } else {
            int ovf = fast->cur - fast->end;
            if (ovf > fast->lpl)
              ovf = m68k_modu(ovf,fast->lpl);
            fast->cur = fast->end - fast->lpl + ovf;
          }

        }
      } while (temp < pmet);
    }

    do {
      *temp++ += 128;
    } while (temp < pmet);

    fast->dec = acu;
    temp = pmet - n;
  }

  do {
    *d++ = mixtbl[*temp++];
  } while (--n);

}


static inline u32_t xstep(u32_t stp, uint16_t f12)
{
  return mulu(stp>>4, f12) >> (24-FP);
}


static i16_t push_ste(play_t * const P, void *pcm, i16_t n)
{
  mix_ste_t * const M = (mix_ste_t *)P->mixer_data;
  int k;
  i16_t n1, n2;
  int8_t *r1, *r2;
  int8_t * const r0 = mixbuf, * const re = r0+sizeof(mixbuf);

  zz_assert( P );
  zz_assert( M );
  zz_assert( n > 0 );
  zz_assert( n < 1024 );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_fast_t * const K = M->fast+k;
    chan_t     * const C = P->chan+k;
    const u8_t trig = C->trig;

    C->trig = TRIG_NOP;
    switch (trig) {
    case TRIG_NOTE:
      zz_assert(C->note.ins);
      K->cur = C->note.ins->pcm;
      K->end = K->cur + C->note.ins->len;
      K->dec = 0;
      K->lpl = C->note.ins->lpl;

    case TRIG_SLIDE:
      K->xtp = xstep(C->note.cur, M->scl);
      break;
    case TRIG_STOP: K->cur = 0;
    case TRIG_NOP:  break;
    default:
      zz_assert(!"wtf");
      return -1;
    }
  }

  if ( ! M->wptr ) {
    zz_assert ( ! ( DMA[DMA_CNTL] & DMA_CNTL_ON ) );
    stop_dma();
    r1 = r0;
    M->wptr = r2 = r1 + (n1=n);
    n2 = 0;
  } else {
    int8_t * rptr;
    if ( ! ( DMA[DMA_CNTL] & DMA_CNTL_ON ) ) {
      start_dma(mixbuf, mixbuf+sizeof(mixbuf), M->dma_mode);
      rptr = mixbuf;
    } else {
      rptr = dma_position();
    }

    r1 = M->wptr;
    if (rptr > r1) {
      if (n1 = rptr-r1, n1 > n)
        n1 = n;
      M->wptr = r2 = r1+n1;
      n2 = 0;
    } else if (n1 = re-r1, n1 > n) {
      n1 = n;
      M->wptr = r2 = r1+n1;
      n2 = 0;
    } else {
      n2 = n - n1;
      r2 = r0;
      M->wptr = r2+n2;
    }
    if (M->wptr >= re)
      M->wptr = r0 + (M->wptr-re);
  }

  fast_ste(mixtbl, r1, temp, M->fast, n1);
  fast_ste(mixtbl, r2, temp, M->fast, n2);

  return n;
}

static zz_err_t init_ste(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_t * const M = &g_ste;
  uint32_t refspr;

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

  switch (spr) {
  case ZZ_LQ: spr =  6258; M->dma_mode = 0|DMA_MODE_MONO; break;
  case ZZ_FQ: spr = 12517; M->dma_mode = 1|DMA_MODE_MONO; break;
  case ZZ_HQ: spr = 50066; M->dma_mode = 3|DMA_MODE_MONO; break;
  default:    spr = 25033; M->dma_mode = 2|DMA_MODE_MONO; break;
  }
  P->spr = spr;
  M->scl = ( divu(refspr<<13,spr) + 1 ) >> 1;

  init_dma(P);
  init_mix(P);
  init_spl(P);

  return ecode;
}

static void free_ste(play_t * const P)
{
  stop_dma();
  P->mixer_data = 0;
}
