/**
 * @file   mix_stf.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STf mixer (YM-2149).
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define HALF_TONE 1		      /* Use lowpass filter trick */

#define mix_align(N) (N)

#undef SPR_MIN
#define SPR_MIN 4000

#undef SPR_MAX
#define SPR_MAX 14000 /* 15650 */

#define FRQMAX 20000			/*  */
#define MIXMAX (FRQMAX/200u+2)

#define YML ((volatile uint32_t *)0xFFFF8800)
#define YMB ((volatile uint8_t *)0xFFFF8800)
#define MFP ((volatile uint8_t *)0xFFFFFA00)
#define VEC (*(trout_t * volatile *)0x134)
/* #define BGC(X) (*(volatile uint16_t *)0xFFFF8240) = (X) */

static zz_err_t init_stf(core_t * const P, u32_t spr);
static void	free_stf(core_t * const P);
static i16_t	push_stf(core_t * const P, void *pcm, i16_t npcm);

mixer_t * mixer_stf(mixer_t * const M)
{
  M->name = "ym2149";
  M->desc = "Atari ST via YM-2149 and timer-A";
  M->init = init_stf;
  M->free = free_stf;
  M->push = push_stf;
  return M;
}

/* ---------------------------------------------------------------------- */

typedef struct mix_stf_s mix_stf_t;
struct mix_stf_s {
  ata_t	  ata;				/* generic atari mixer  */
  uint8_t tcr, tdr;			/* timer parameters */
};

typedef struct timer_rout_s trout_t;
struct timer_rout_s {
  struct  {
    uint16_t opc;			/* 0x21fc */
    uint8_t  reg,x,val,y;
    uint16_t adr;
  } movel[3];
  uint16_t opc;				/* 0x31fc */
  uint16_t imm;
  uint16_t adr;
  uint16_t rte;
};

#include "ym10_pack.h"

static mix_stf_t g_stf;
static int16_t * temp;			/* intermediat mix buffer */
static trout_t * rout;			/* first timer routine */
static trout_t * tuor;			/* last  timer routine */

static uint8_t stf_buf[32*4*MIXMAX+16];
static uint8_t Tpcm[1024*4];

ZZ_EXTERN_C
void fast_stf(uint8_t * Tpcm, trout_t * routs,
	      int16_t * temp, fast_t   * voices,
	      int32_t n);

/* ---------------------------------------------------------------------- */

static void /* never_inline */
fill_timer_routines(trout_t * rout, uint16_t n)
{
  static const uint16_t tpl[16] = {
    0x21fc,0x0800, /* d1 */    0x0000,0x8800, /* d2 */
    0x21fc,0x0900, /* d3 */    0x0000,0x8800, /* d4 */
    0x21fc,0x0A00, /* d5 */    0x0000,0x8800, /* d6 */
    0x31fc,0x0000, /* d7 */    0x0136,0x4e73, /* a0 */
  };

  asm volatile (
    "   movem.l  %[tpl],%%d1-%%a0  \n"
    "   movea.l  %[buf],%%a1       \n"
    "   move.w   %%a1,%%d7         \n"
    "   moveq    #0,%%d0           \n"
    "   move.w   %[cnt],%%d0       \n"
    "   lsl.l    #5,%%d0           \n"
    "   adda.l   %%d0,%%a1         \n"
    "   move.w   %[cnt],%%d0       \n"
    "   subq.w   #1,%%d0           \n"
    "0: movem.l  %%d1-%%a0,-(%%a1) \n"
    "   move.w   %%a1,%%d7         \n"
    "   dbf      %%d0,0b           \n\t"
    :
    : [cnt] "g" (n), [buf] "g" (rout), [tpl] "m" (tpl)
    : "%cc","memory",
      "%d0","%d1","%d2","%d3","%d4","%d5","%d6","%d7",
      "%a0","%a1","%a2"
    );
}


static void never_inline
init_timer_routines(void)
{
  const intptr_t beg = (intptr_t) stf_buf;
  const intptr_t end = beg+sizeof(stf_buf);
  const intptr_t seg = end & 0xFFFF0000;

  zz_assert( sizeof( trout_t) == 32 );

  /* default setup */
  rout = (trout_t *) beg;
  temp = (int16_t *) end - MIXMAX;

  if (seg > beg) {
    /* end is on a different segment */
    const uint16_t len0 = seg-beg;	/* => -beg */
    const uint16_t len1 = end-seg;	/* =>  end */
    if (len0 < len1) {
      rout = (trout_t *) seg;
      if ( (len0>>1) >= MIXMAX ) {
	temp = (int16_t *) stf_buf;
      }
    }
  }

  tuor = rout+MIXMAX*2;

  dmsg("rout: %p  tuor: %p  max: %lu\n", rout, tuor, LU(tuor-rout));
  zz_assert ( (intptr_t)rout >> 16 == (intptr_t)tuor >> 16 );

  fill_timer_routines(rout, MIXMAX*2);
}


/* Create 1024 x 4 bytes {A,B,C,X} entry from the packed table. */
static void never_inline
init_recore_table(void)
{
  if (!Tpcm[4*1023]) {
    const uint8_t * pack = ym10_pack;
    uint8_t * table = Tpcm;
    do {
      uint8_t XY;
      XY = *pack++;
      *table++ = XY >> 4;
      *table++ = XY & 15;
      XY = *pack++;
      *table++ = XY >> 4;
      ++ table;
      *table++ = XY & 15;
      XY = *pack++;
      *table++ = XY >> 4;
      *table++ = XY & 15;
      ++ table;
    } while (pack < ym10_pack+sizeof(ym10_pack));
    zz_assert( table == &Tpcm[sizeof(Tpcm)] );
  }
}


static void clear_ym_regs(void)
{
  YML[0] = 0x00000000;	YML[0] = 0x01000000;
  YML[0] = 0x02000000;	YML[0] = 0x03000000;
  YML[0] = 0x04000000;	YML[0] = 0x05000000;
  YML[0] = 0x08000000;	YML[0] = 0x09000000;  YML[0] = 0x0A000000;
}


static void never_inline stop_sound(void)
{
  int16_t ipl = enter_critical();
  YMB[0] = 7;
  YMB[2] = YMB[0] | 0077;
  leave_critical(ipl);
  clear_ym_regs();
}


/* GB: current Hatari does not handle lowpass trick. */
static void never_inline prepare_sound(void)
{
#if HALF_TONE
  int16_t ipl = enter_critical();
  YMB[0] = 7;
  YMB[2] = (YMB[0] & 0300) | 0070;
  leave_critical(ipl);
  clear_ym_regs();
#else
  stop_sound();
#endif
}

static void init_spl(core_t * P)
{
  vset_unroll(&P->vset,0);
}

static void stop_timer(void)
{
  MFP[0x19]  = 0;			/* Stop timer-A */
  MFP[0x17] |= 8;			/* SEI */
}

static uint16_t never_inline set_sampling(core_t * const P, uint16_t spr)
{
  mix_stf_t * const M = (mix_stf_t *) P->data;
  M->tdr = (divu(2457600>>1, spr) + 1) >> 1;
  M->tcr = 1;
  return P->spr = (divu(2457600>>1, M->tdr) + 1) >> 1;
}

static void start_timer(void)
{
  MFP[0x19]  = 0;			/* Stop timer-A */
  MFP[0x07] |= 0x20;			/* IER */
  MFP[0x13] |= 0x20;			/* IMR */
  MFP[0x17] &= ~8;			/* AEI */
  VEC = rout;
  MFP[0x1F] = g_stf.tdr;
  MFP[0x19] = g_stf.tcr;
}

static int16_t pb_play(void)
{
  zz_assert( g_stf.ata.fifo.sz == MIXMAX*2 );

  if ( ! (MFP[0x19]&7) ) {
    zz_assert( g_stf.ata.fifo.ro == 0 );
    zz_assert( g_stf.ata.fifo.wp >  0 );
    zz_assert( g_stf.ata.fifo.rp == 0 );
    start_timer();
    prepare_sound();
    return 0;
  } else {
    trout_t * const pos = VEC;
    zz_assert( pos >= rout );
    zz_assert( pos <  tuor );
    return mix_align( pos - rout );
  }
}

static void pb_stop(void)
{
  stop_timer();
  stop_sound();
}

static i16_t push_stf(core_t * const P, void * pcm, i16_t n)
{
  mix_stf_t * const M = (mix_stf_t *)P->data;
  i16_t ret = n;

  const int16_t tmax = MIXMAX;
  const int16_t bias = 2;     /* last thing we want is to under run */

  zz_assert( P );
  zz_assert( M == &g_stf );
  zz_assert( mix_align(tmax) == tmax );

  n += bias;
  if (n > tmax)
    n = tmax;
  n = mix_align(n+bias) - mix_align(-1);
  zz_assert( mix_align(n) == n );
  zz_assert( n > 0 );
  zz_assert( n < MIXMAX );

  play_ata(&M->ata, P->chan, n);

#define fast_mix(N) \
  fast_stf(Tpcm, rout+M->ata.fifo.i##N, temp, M->ata.fast, M->ata.fifo.n##N)

  fast_mix(1);
  fast_mix(2);

  return ret;

}


static zz_err_t init_stf(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_stf_t * const M = &g_stf;
  uint32_t refspr;
  uint16_t scale;

  init_timer_routines();
  init_recore_table();
  P->data = M;
  zz_memclr(M,sizeof(*M));
  refspr = mulu(P->song.khz,1000u);

  switch (spr) {
  case 0:     spr = refspr; break;
  case ZZ_LQ: spr = 8000; break;
  case ZZ_MQ: spr = 12000; break;
  case ZZ_FQ: spr = SPR_MIN; break;
  case ZZ_HQ: spr = SPR_MAX; break;
  }
  if (spr < SPR_MIN) spr = SPR_MIN;
  if (spr > SPR_MAX) spr = SPR_MAX;

  P->spr = spr = set_sampling(P,spr);
  scale	 = ( divu( refspr<<13, spr) + 1 ) >> 1;

  init_spl(P);
  init_ata(MIXMAX*2,scale);

  return ecode;
}


static void free_stf(core_t * const P)
{
  if (P->data) {
    mix_stf_t * const M = (mix_stf_t *)P->data;
    if ( M ) {
      zz_assert( M == &g_stf );
      stop_ata(&M->ata);
    }
    P->data = 0;
    stop_timer();
    stop_sound();
  }
}
