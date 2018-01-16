/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA).
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define METHOD  0
#define TICKMAX 256
#define FIFOMAX (TICKMAX*2)
#define TEMPMAX TICKMAX

#define ALIGN(X)    ((X)&-(MIXALIGN))
#define ULIGN(X)    ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X) ( (X) == ALIGN(X) )


#if METHOD == 1

# define init_spl(P)   init_spl8(P)
# define fast_mix(R,N) slow_ste(mixtbl, R, _temp, M->ata.fast, N)
# define MIXALIGN      1

#elif METHOD == 2

# define init_spl(P)   init_spl6(P)
# define fast_mix(R,N) fast_ste_6bit(R, &M->ata.fast, N)
# define MIXALIGN      8

#else

# define init_spl(P)   init_spl8(P)
# define fast_mix(R,N) fast_ste(mixtbl, R, _temp, M->ata.fast, N)
# define MIXALIGN      1

#endif

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

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_ste_s mix_ste_t;

struct mix_ste_s {
  ata_t   ata;                          /* generic atari mixer  */
  uint8_t dma;                          /* STe sound DMA mode   */
};

static mix_ste_t g_ste;                 /* STE mixer instance */
static int8_t    mixtbl[1024];          /* mix 4 voices together */

/* $$$ TEMP REDUCE BUFFER */
static int16_t  _temp[TEMPMAX];         /* intermediat mix buffer */
static int8_t   _fifo[FIFOMAX];         /* FIFO buffer */

ZZ_EXTERN_C
void fast_ste(int8_t  * Tmix, int8_t     * dest,
              int16_t * temp, mix_fast_t * voices,
              int32_t n);

ZZ_EXTERN_C
void fast_ste_6bit(int8_t * dest, mix_fast_t * voices,
                   int32_t n);

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

  DMA_MODE_MONO   = 0x80,
  DMA_MODE_16BIT  = 0x40,
};

static int8_t * dma_position(void)
{
  void * pos;
  asm volatile(
    "   lea 0x8909.w,%%a0      \n\t"
    "   moveq #-1,%[pos]       \n\t"
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
  DMA[DMA_CNTL] &= ~(DMA_CNTL_ON|DMA_CNTL_LOOP);
}

static void start_dma(void * adr, void * end, uint8_t mode)
{
  stop_dma();
  write_dma_ptr(DMA+DMA_ADR, adr);
  write_dma_ptr(DMA+DMA_END, end);
  DMA[DMA_MODE] = mode;
  DMA[DMA_CNTL] |= DMA_CNTL_ON|DMA_CNTL_LOOP;
}

static int16_t pb_play(void)
{
  zz_assert( g_ste.ata.fifo.sz == FIFOMAX );

  if ( ! (DMA[DMA_CNTL] & DMA_CNTL_ON) ) {
    start_dma(_fifo, &_fifo[g_ste.ata.fifo.sz], g_ste.dma);
    dmsg("DMA cntl:%02hx mode:%02hx/%02hx %p\n",
         HU(DMA[DMA_CNTL]), HU(DMA[DMA_MODE]), HU(g_ste.dma), dma_position());
    return 0;

  } else {
    int8_t * const pos = dma_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos <  _fifo+g_ste.ata.fifo.sz );
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  stop_dma();
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

#if METHOD != 2

/* Unroll instrument loops (samples stay in u8 format).
 */
static void never_inline init_spl8(play_t * P)
{
  u8_t k;
  for (k=0; k<256; ++k)
    P->tohw[k] = k;
  vset_unroll(&P->vset,P->tohw);
}

#else

static void never_inline init_spl6(play_t * P)
{
  u8_t k;
  for (k=0; k<256; ++k)
    P->tohw[k] = k>>2;
  vset_unroll(&P->vset,P->tohw);
}

#endif

#if METHOD == 1

static void
slow_ste(const int8_t mixtbl[], int8_t * d, int16_t * temp,
         mix_fast_t * fast, int16_t n)
{
  mix_fast_t * const tsaf = fast+4;
  int16_t * const pmet = temp + n;

  zz_assert( n >= 0 );
  zz_assert( n <= MIXMAX );
  if ( n <= 0 ) return;

  zz_memclr(temp,n<<1);

  for (; fast < tsaf; ++fast) {
    int32_t acu = 0;
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
#endif

static i16_t push_ste(play_t * const P, void *pcm, i16_t n)
{
  mix_ste_t * const M = (mix_ste_t *)P->mixer_data;
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

static zz_err_t init_ste(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_ste_t * const M = &g_ste;
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

  switch (spr) {
  case ZZ_LQ: spr =  6258; M->dma = 0|DMA_MODE_MONO; break;
  case ZZ_FQ: spr = 12517; M->dma = 1|DMA_MODE_MONO; break;
  case ZZ_HQ: spr = 50066; M->dma = 3|DMA_MODE_MONO; break;
  default:    spr = 25033; M->dma = 2|DMA_MODE_MONO; break;
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

static void free_ste(play_t * const P)
{
  if (P->mixer_data) {
    mix_ste_t * const M = (mix_ste_t *)P->mixer_data;
    if ( M ) {
      zz_assert( M == &g_ste );
      stop_ata(&M->ata);
    }
    P->mixer_data = 0;
  }
}
