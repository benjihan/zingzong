/**
 * @file   mix_fal.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari Falcon mixer (16-bit DMA).
 */

#include "../zz_private.h"
#include "mix_ata.h"
#include "zz_tos.h"

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

/* Falcon sampling rates:
 *
 * Hz = CLK(25.175MHZ/256/(psc+1)
 * or STE compatible Hz = 50066 >> (3-(psc>>8))
 */
static const struct {
  uint16_t spr, psc;
} prescalers[] = {
  {  8195, 0x00B }, {  9834, 0x009 }, { 12292, 0x007 },
  { 12517, 0x100 }, { 16390, 0x005 }, { 19668, 0x004 },
  { 24585, 0x003 }, { 25033, 0x200 }, { 32780, 0x002 },
  { 49170, 0x001 }, { 50066, 0x300 },
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

static inline int16_t locksnd()
{
#if WITH_LOCKSND
  return xbios_sndlock() == 1;
#else
  return 1;
#endif
}

static inline int16_t unlocksnd()
{
#if WITH_LOCKSND
  return xbios_unsndlock() == 0;
#else
  return 1;
#endif
}

static inline spl_t * dma_read_position(void)
{
#ifdef USE_XBIOS
  return xbios_playptr();
#else
  spl_t * pos;
  int ipl = enter_critical();
  DMAB(DMA_CNTL) &= 0x7F;                /* select playback register */
  pos = dma8_position();
  leave_critical(ipl);
  return pos;
#endif
}

static int16_t dma_running(void)
{
#ifdef USE_XBIOS
  return (xbios_buffoper(-1) & 3) == 3;
#else
  return (DMAB(DMA_CNTL) & 3) == 3;
#endif
}

static void dma_start(void)
{
#ifdef USE_XBIOS
  xbios_buffoper( xbios_buffoper(-1) | 3 );
#else
  DMAB(DMA_CNTL) |= 3;
#endif
}

static void never_inline dma_stop(void)
{
#ifdef USE_XBIOS
  xbios_buffoper( xbios_buffoper(-1) & 0xC );
#else
  DMAB(DMA_CNTL) &= ~3;
#endif
}

static int never_inline dma_setup(uint16_t psc)
{
#ifdef USE_XBIOS
  /* Stop playback */
  dma_stop();

  /* Reinitialized DAC/ADC */
  xbios_sndstatus(1);

  /* 1 playback track, don't care about recording */
  xbios_settracks(0,-1);

  /* Monitor track #1 */
  // xbios_setmontracks(0);
  /* 16 Bit stereo */
  xbios_setmode(1);

  /* playback buffer */
  xbios_setbuffer(0, _fifo, _fifo+FIFOMAX);

  /* Disable timer-A interrupt */
  xbios_setinterrupt(0,0);

  /* Disable gpio-7 interrupt */
  xbios_setinterrupt(1,0);

  /* Disconnect DSP */
  xbios_dsptristate(0,0);

  if ( (uint8_t)psc == 0 )
    xbios_soundcmd(6, psc>>8); /* set STE compatible sampling rate. */

  /* Connect source DMA to DAC output at given sampling rate w/o
   * handshake */
  xbios_devconnect(0,8,0,(uint8_t)psc,1);
#else
  int16_t ipl = enter_critical();

  /* Stop playback */
  dma_stop();

  DMAB(0x00) = 0;                       /* no buffer interrupts */
  DMAB(0x01) &= 0x7C;                   /* off/select play register */

  dma8_write_ptr(DMA+0x03,_fifo);         /* start address  */
  dma8_write_ptr(DMA+0x0F,_fifo+FIFOMAX); /* end address */

  DMAB(0x20) = 0x00;                     /* DAC to track #0, play 1 track*/
  DMAB(0x21) = 0x40|(psc>>8);            /* 16-bit stereo + STe Hz */

#if 0
  /* Crossbar Source Controller */
  DMAW(0x30) = 0
    | ( (0x0) << 12 )                   /* disconnect A/D convertor */
    | ( (0x0) <<  8 )                   /* xternal input */
    | ( (0x0) <<  4 )                   /* DSP xmit */
    | ( (0x0) <<  0 )                   /* DMA playback: On */
    ;

  /* Crossbar Destination Controller */
  DMAW(0x32) = 0
    | ( (0x0) << 12 )               /* disconnect D/A convertor */
    | ( (0x0) <<  8 )               /* xternal output: DMA playback */
    | ( (0x0) <<  4 )               /* DSP Record */
    | ( (0x0) <<  0 )               /* DMA Record */
    ;

  DMAB(0x34) = psc;                     /* External clock divided */
  DMAB(0x35) = psc;                     /* Internal Sync divider */
#endif

  leave_critical(ipl);
  xbios_devconnect(0,8,0,(uint8_t)psc,1);

#endif
  return 0;
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
  dma_stop();
}

static void init_dma(uint16_t psc)
{
  dma_setup(psc);
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

  switch (spr) {
  case ZZ_LQ: spr =  8195; break;
  case ZZ_FQ: spr = 12292; break;
  case ZZ_MQ: spr = 24585; break;
  case ZZ_HQ: spr = 32780; break;
  }

  if (1) {
    const int16_t maxi = sizeof(prescalers) / sizeof(*prescalers) - 1;
    const uint16_t sprmax = spr+(spr>>3), sprmin = spr-(spr>>3);
    int16_t i;

    for ( i=0; i<maxi; ++i ) {
      if ( prescalers[i].spr == spr ) break;
      if ( prescalers[i].spr < sprmin ) continue;
      i += ( prescalers[i+1].spr <= sprmax );
      break;
    }
    M->psc = prescalers[i].psc;
    P->spr = spr = prescalers[i].spr;
  }

  scale  = ( divu( refspr<<13, spr) + 1 ) >> 1;

  if ( ! locksnd() ) {
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
      unlocksnd();
    }
    P->data = 0;
  }
}
