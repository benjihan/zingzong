/**
 * @file   mix_fal.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari Falcon mixer (16-bit DMA).
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define WITH_LOCKSND 0

#define TICKMAX 256
#define FIFOMAX (TICKMAX*3)
#define TEMPMAX TICKMAX

#define ALIGN(X)      ((X)&-(MIXALIGN))
#define ULIGN(X)      ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X)   ((X) == ALIGN(X))
#define init_spl(P)   init_spl(P)
#define fast_mix(R,N) fast_fal(Tmix,R,M->ata.fast,N)
#define MIXALIGN      1
#define FAL_DMA_MODE  (DMA_MODE_16BIT)
typedef int32_t spl_t;

static zz_err_t init_fal(core_t * const, u32_t);
static void     free_fal(core_t * const);
static i16_t    push_fal(core_t * const, void *, i16_t);

mixer_t * mixer_fal(mixer_t * const M)
{
  M->name = "dma16:blend-stereo";
  M->desc = "blended stereo 16-bit DMA";
  M->init = init_fal;
  M->free = free_fal;
  M->push = push_fal;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_fast_s mix_fast_t;
typedef struct mix_fal_s mix_fal_t;

struct mix_fal_s {
  ata_t   ata;                         /* generic atari mixer  */
  int16_t psc;                         /* sampling rate prescaler */
};

static mix_fal_t g_fal;                 /* Falcon mixer instance */
static spl_t     Tmix[510];             /* Blending table */
static spl_t    _fifo[FIFOMAX];         /* FIFO buffer */
static uint16_t  Tmix_lr8;              /* Tmix current setup */

ZZ_EXTERN_C
void fast_fal(spl_t * Tmix, spl_t * dest, mix_fast_t * voices, int32_t n);

/* Falcon prescalers (unused ATM)
 *
 * Hz = CLK(25.175MHZ/256/(prescaler+1)
 *
 */
static uint16_t prescalers[12] = {
  /*STE      1      2      3 */
  00000, 49170, 32780, 24585,
  /*  4      5      6      7 */
  19668, 16390,     0, 12292,
  /*  8      9     10     11 */
      0,  9834,     0,  8195,
};

/*
  ----------------------------------------------------------------------

  Sound system calls

  ----------------------------------------------------------------------

  Because we are not using frame pointer it can be a bit tricky to use
  "g" or "m" constraints as it might generate addressing mode
  referring to a7.

  ----------------------------------------------------------------------
*/

#define USE_XBIOS 1

static inline int32_t
xbios_0(int16_t func)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  move.l  %d0,%[ret]      \n"
    "  addq.w  #2,%%a7         \n\t"
    : [ret]  "=d" (ret)
    : [func] "i"  (func)
    : "cc","d0");
  return ret;
}

static inline int32_t
xbios_w(int16_t func, int16_t parm)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[parm],-(%%a7) \n"
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  move.l  %d0,%[ret]      \n"
    "  addq.w  #4,%%a7         \n\t"
    : [ret]  "=d" (ret)
    : [func] "i"  (func),
      [parm] "ig" (parm)
    : "cc","d0");
  return ret;
}

static inline int32_t
xbios_ww(int16_t func, int16_t par1, int16_t par2)
{
  int32_t ret;
  asm volatile (
    "  move.w  %[par2],-(%%a7) \n"
    "  move.w  %[par1],-(%%a7) \n"
    "  move.w  %[func],-(%%a7) \n"
    "  trap    #14             \n"
    "  move.l  %d0,%[ret]      \n"
    "  addq.w  #6,%%a7         \n\t"
    : [ret]  "=d" (ret)
    : [func] "i"  (func),
      [par1] "ir" (par1),
      [par2] "ig" (par2)
    : "cc","d0");
  return ret;
}

#define xbios_decl(TYPE) static inline TYPE

xbios_decl(int32_t)
/**
 * Lock sound system for other applications.
 * @retval    1 success
 * @retval -129 already locked
 */
xbios_locksnd(void)
{
#if WITH_LOCKSND
  return xbios_0(128);
#else
  return 1;
#endif
}

xbios_decl(int32_t)
/**
 * Unlock sound system for use by other applications.
 * @retval     0 success
 * @retval  -128 not locked
 */
xbios_unlocksnd(void)
{
#if WITH_LOCKSND
  return xbios_0(129);
#else
  return 0;
#endif
}


static inline spl_t * dma_read_position(void)
{
  spl_t * pos;

#if 1
  uint16_t ipl = enter_critical();
  DMA[DMA_CNTL] &= 0x7F;                /* select replay register */
  pos = dma8_position();                /* read position  */
  leave_critical(ipl);
#else
  void * ptr[2];
  asm volatile (
    "  pea     (%[ptr])        \n"
    "  move.w  #141,-(%%a7)    \n"
    "  trap    #14             \n"
    "  addq.w  #6,%%a7         \n"
    "  move.l  %%d0,%[ret]     \n"
    "  beq     0f              \n"
    "  move.l  (%[ptr]),%[ret] \n\t"
    "0:\n"
    : [ret] "=d" (pos)
    : [ptr] "a"  (ptr)
    : "cc","d0");
#endif
  return pos;
}

xbios_decl(int32_t)
/**
 *
 */
xbios_devconnect(
  int16_t src,       /* 0:DMA out 1:DSP out, 2:xtern 3:ADC */
  int16_t dst,       /* 1:DMA in, 2:DSP in,  4:xtern, 8:DAC */
  int16_t srcclk,    /* 0:25.175MHz 1:xtern 2:32MHz */
  int16_t prescale,  /* 0:ste  */
  int16_t protocol)  /* 0:handshake */
{
  int32_t ret;
  asm volatile (
    "  move.w  %[ptc],-(%%a7) \n"
    "  move.w  %[sca],-(%%a7) \n"
    "  move.w  %[clk],-(%%a7) \n"
    "  move.w  %[dst],-(%%a7) \n"
    "  move.w  %[src],-(%%a7) \n"
    "  move.w  #139,-(%%a7)   \n"
    "  trap    #14            \n"
    "  move.l  %%d0,%[ret]    \n"
    "  lea     12(%%a7),%%a7  \n\t"
    : [ret] "=d" (ret)
    : [ptc] "i"  (protocol),
      [sca] "ir" (prescale),
      [clk] "i"  (srcclk),
      [dst] "i"  (dst),
      [src] "i"  (src)
    : "cc","d0");
  return ret;
}

xbios_decl(void)
/**
 * Set record/playback buffer buffer.
 * @param  reg  0:playback 1:recording
 * @param  beg  start address
 * @param  end  end address
 * @retval 0 on success
 */
dma_set_buffer(void * beg, void * end)
{
#if ! USE_XBIOS
  dma8_write_ptr(DMA+DMA_ADR, beg);
  dma8_write_ptr(DMA+DMA_END, end);
#else
  asm volatile (
    "  move.l  %[end],-(%%a7) \n"
    "  move.l  %[beg],-(%%a7) \n"
    "  clr.w   -(%%a7)        \n"
    "  move.w  #131,-(%%a7)   \n"
    "  trap    #14            \n"
    "  lea     12(%%a7),%%a7  \n\t"
    :
    : [reg] "i"  (0),
      [beg] "r"  (beg),
      [end] "g"  (end)
    : "cc","d0");
#endif
}


xbios_decl(int32_t)
/**
 * Select record/playback mode.
 * @param  mode 0:2x8bit 1:2x16bit 2:1x8bit
 * @retval 0 on success
 */
xbios_setmode(int16_t mode)
{
#if ! USE_XBIOS
  DMAW(DMAW_MODE) = 0x0040;
  return 0;
#else
  return xbios_w(132,mode);
#endif
}
#define dma_16bit_stereo() xbios_setmode(1)



xbios_decl(int32_t)
/**
 *
 */
xbios_sndstatus(int16_t reset)
{
  return xbios_w(140,reset);
}

xbios_decl(int32_t)
/**
 * Set interrupt at the end of recording/playback.
 *
 * @param  src_inter  0:timer-A 1:MFP-interrupt 7
 * @param  cause      0:No 1:on-end-pb 2:on-end-rec 4:on-end-any
 * @retval 0 on success
 */
xbios_setinterrupt(int16_t src_inter, int16_t cause)
{
  return xbios_ww(135,src_inter,cause);
}
#define no_audio_interrupt() xbios_setinterrupt(0,0)

xbios_decl(int32_t)
/**
 *
 */
soundcmd(int16_t mode, int16_t data)
{
  return xbios_ww(130,mode,data);
}

xbios_decl(int32_t)
/**
 * Set or read record/playback mode.
 *
 * @param  mode -1: Read status
 *              #0: DMA sound playback
 *              #1: Loop playback of sound currently playing
 *              #2: DMA sound recording
 *              #4: Loop recording of sound currently playing
 * @return 0 on success or current state
 */
xbios_buffoper(int16_t mode)
{
  return xbios_w(136,mode);
}
#define dma_stop_playback()  xbios_buffoper(0)
#define dma_loop_playback()  xbios_buffoper(3)
#define dma_status()         (xbios_buffoper(-1) & 3)

static int16_t dma_running(void)
{
  return dma_status() == 3;
}

static void dma_start(void)
{
  dma_set_buffer(_fifo, _fifo+g_fal.ata.fifo.sz);
  dma_loop_playback();
}

static int16_t pb_play(void)
{
  zz_assert( g_fal.ata.fifo.sz == FIFOMAX );

  if ( ! dma_running() ) {
    dma_start();
    return 0;
  } else {
    spl_t * const pos = dma_read_position();
    zz_assert( pos >= _fifo );
    zz_assert( pos < _fifo+g_fal.ata.fifo.sz);
    return ALIGN( pos - _fifo );
  }
}

static void pb_stop(void)
{
  dma_stop_playback();
//  xbios_sndstatus(1);                   /* Reset */
}

static void init_dma(int8_t psc)
{
  dma_stop_playback();
  dma_16bit_stereo();
  xbios_devconnect(0,8,0,psc,1);
}

/* Unroll instrument loops (samples stay in u8 format).
 */
static void init_spl(core_t * K)
{
  vset_unroll(&K->vset,0);
}

static void never_inline init_mix(uint16_t lr8)
{
  int32_t v9 = muls(-255,lr8);
  int16_t w8 = -255 << 7, x;

  /* 2 U8 voices added => {0..510} */
  for (x=0; x<510; ++x, v9 += lr8, w8 += 128) {
    int16_t v = v9 >> 1;
    int16_t w = w8 - v;
    Tmix[x] = ((int32_t)w<<16) | (uint16_t) v;
  }
  Tmix_lr8 = lr8;
}

static i16_t push_fal(core_t * const P, void *pcm, i16_t n)
{
  mix_fal_t * const M = (mix_fal_t *) P->data;
  i16_t ret = n;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_fal );
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

static zz_err_t init_fal(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_fal_t * M = &g_fal;
  uint32_t refspr;
  uint16_t scale;

  zz_memclr(M,sizeof(*M));
  refspr = mulu(P->song.khz,1000u);

  if (spr == 0)
    spr = refspr;

  /* STE      1      2      3
     00000, 49170, 32780, 24585,
       4      5     (6)     7
     19668, 16390, 14049, 12292,
      (8)     9     (10)   11
     10927,  9834,  8940,  8195,
  */

  switch (spr) {
  case ZZ_LQ: M->psc = 11; /*  8195-Hz */ break;
  case ZZ_FQ: M->psc =  7; /* 12292-Hz */ break;
  case ZZ_MQ: M->psc =  4; /* 19668-Hz */ break;
  case ZZ_HQ: M->psc =  2; /* 32780-Hz */ break;
  default:
    for (M->psc=11; M->psc>1; --M->psc)
      if (prescalers[M->psc] >= spr)
        break;
  }
  dmsg("prescaler: %hu-hz -> prescaler[%hu]=%hu-hz\n",
       HU(spr), HU(M->psc), HU(prescalers[M->psc]));

  zz_assert( prescalers[M->psc] );

  P->spr = spr = prescalers[M->psc];
  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  if ( xbios_locksnd() != 1 ) {
    M = 0;
    ecode = E_MIX;
  } else {
    init_mix(P->lr8);
    init_spl(P);
    init_ata(FIFOMAX,scale);
    init_dma(M->psc);
    dmsg("spr:%lu psc:%hu scale:%lx\n",
         LU(spr), HU(M->psc), LU(scale));
  }
  P->data = M;
  return ecode;
}

static void free_fal(core_t * const P)
{
  if (P->data) {
    mix_fal_t * const M = (mix_fal_t *) P->data;
    if ( M ) {
      zz_assert( M == &g_fal );
      stop_ata(&M->ata);
      xbios_unlocksnd();
    }
    P->data = 0;
  }
}
