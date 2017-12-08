/**
 * @file   mix_stf.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STf mixer (YM-2149).
 */

#include "../zz_private.h"

#undef FP
#define FP 16

#undef SPR_MIN
#define SPR_MIN 4000

#undef SPR_MAX
#define SPR_MAX 20000

#define MIX_MAX (SPR_MAX/200u+2)

#define YML ((volatile uint32_t *)0xFFFF8800)
#define YMB ((volatile uint8_t *)0xFFFF8800)
#define MFP ((volatile uint8_t *)0xFFFFFA00)
#define VEC (*(timer_rout_t * volatile *)0x134)

static zz_err_t init_stf(play_t * const P, u32_t spr);
static void     free_stf(play_t * const P);
static i16_t    push_stf(play_t * const P, void *pcm, i16_t npcm);

mixer_t * mixer_stf(mixer_t * const M)
{
  M->name = "ym2149";
  M->desc = "Atari ST via YM-2149";
  M->init = init_stf;
  M->free = free_stf;
  M->push = push_stf;
  return M;
}

#include "ym10_pack.h"

typedef struct timer_rout_s timer_rout_t;

/* !!! change in m68k_stf.S as well !!! */
struct mix_fast_s {
  uint8_t *end;                         /*  0 */
  uint8_t *cur;                         /*  4 */
  uint32_t xtp;                         /*  8 */
  uint16_t dec;                         /* 12 */
  uint16_t lpl;                         /* 14 */
};                                      /* 16 bytes */

typedef struct mix_fast_s mix_fast_t;

ZZ_EXTERN_C
void fast_stf(uint8_t * Tpcm, timer_rout_t * routs,
              int16_t * temp, mix_fast_t   * voices,
              int32_t n);


typedef struct mix_chan_s mix_chan_t;

struct mix_stf_s {
  mix_fast_t fast[4];
  timer_rout_t * wptr;
  uint16_t scl;
  uint8_t tcr, tdr;
};
typedef struct mix_stf_s mix_stf_t;

struct timer_rout_s {
  struct  {
    uint16_t opc;                       /* 0x21fc */
    uint8_t  reg,x,val,y;
    uint16_t adr;
  } movel[3];
  uint16_t opc;                         /* 0x31fc */
  uint16_t imm;
  uint16_t adr;
  uint16_t rte;
};

static mix_stf_t      g_stf;
static int16_t      * temp;             /* intermediat mix buffer */
static timer_rout_t * rout;             /* first timer routine */
static timer_rout_t * tuor;             /* last  timer routine */

static uint8_t stf_buf[32*4*MIX_MAX+16];
static uint8_t Tpcm[1024*4];


static void /* never_inline */
fill_timer_routines(timer_rout_t * rout, uint16_t n)
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
    "   move.w   %[cnt],%%d0       \n"
    "   lsl.w    #5,%%d0           \n"
    "   adda.w   %%d0,%%a1         \n"
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

static void never_inline init_timer_routines(void)
{
  const intptr_t beg = (intptr_t) stf_buf;
  const intptr_t end = beg+sizeof(stf_buf);
  const intptr_t seg = end & 0xFFFF0000;

  /* default setup */
  rout = (timer_rout_t *) beg;
  temp = (int16_t *) end - MIX_MAX;

  if (seg > beg) {
    /* end is on a different segment */
    const uint16_t len0 = seg-beg;      /* => -beg */
    const uint16_t len1 = end-seg;      /* =>  end */
    if (len0 < len1) {
      rout = (timer_rout_t *) seg;
      if ( (len0>>1) >= MIX_MAX ) {
        temp = (int16_t *) stf_buf;
      }
    }
  }
  tuor = rout+MIX_MAX*2;
  fill_timer_routines(rout, MIX_MAX*2);
}


/* Create 1024 x 4 bytes {A,B,C,X} entry from the packed table. */
static void never_inline
init_replay_table(void)
{
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

static void clear_ym_regs(void)
{
  YML[0] = 0x00000000;  YML[0] = 0x01000000;
  YML[0] = 0x02000000;  YML[0] = 0x03000000;
  YML[0] = 0x04000000;  YML[0] = 0x05000000;
  YML[0] = 0x08000000;  YML[0] = 0x09000000;  YML[0] = 0x0A000000;
}

static void never_inline stop_sound(void)
{
  int16_t savesr;
  asm volatile ("move.w %%sr,%[savesr] \n\t" : [savesr] "=m" (savesr) );
  YMB[0] = 7;
  YMB[2] = YMB[0] | 0077;
  asm volatile ("move.w %%sr,%[savesr] \n\t" : : [savesr] "m" (savesr) );
  clear_ym_regs();
}


static void never_inline prepare_sound(void)
{
  int16_t savesr;
  asm volatile ("move.w %%sr,%[savesr] \n\t" : [savesr] "=m" (savesr) );
  YMB[0] = 7;
  /* YMB[2] = YMB[0] | 0077; */
  YMB[2] = (YMB[0] & 0370) | 0070;
  asm volatile ("move.w %%sr,%[savesr] \n\t" : : [savesr] "m" (savesr) );
  clear_ym_regs();                 /* A98-xx54-3210 */
}

/* GB: unsigned samples: !!! SLOW ASS METHOD !!! */
static void never_inline prepare_insts(inst_t * inst, u8_t n)
{
  u8_t i;
  for (i=0; i<n; ++i, ++inst)
    if (inst->len) {
      uint8_t * pcm = inst->pcm, * const end = pcm+inst->end;
      while (pcm < end)
        *pcm++ ^= 0x80;
    }
}

static void stop_timer(play_t * const P)
{
  MFP[0x19] = 0;                        /* Stop timer-A */
}

static uint16_t never_inline set_sampling(play_t * const P, uint16_t spr)
{
  mix_stf_t * const M = (mix_stf_t *) P->mixer_data;
  M->tdr = (divu(2457600>>1, spr) + 1) >> 1;
  M->tcr = 1;
  return P->spr = (divu(2457600>>1, M->tdr) + 1) >> 1;
}

static void prepare_timer(void)
{
  MFP[0x19]  = 0;                       /* Stop timer-A */
  MFP[0x07] |= 0x20;                    /* IER */
  MFP[0x13] |= 0x20;                    /* IMR */
  MFP[0x17] |= 8;                       /* AEI */
  VEC = (timer_rout_t *)&tuor[-1].rte;
}

static inline u32_t xstep(u32_t stp, uint16_t f12)
{
  return mulu(stp>>4, f12) >> (24-FP);
}

static i16_t push_stf(play_t * const P, void * pcm, i16_t n)
{
  mix_stf_t * const M = (mix_stf_t *)P->mixer_data;
  int k;
  i16_t n1, n2;
  timer_rout_t *r1, *r2;

  zz_assert( P );
  zz_assert( M );
  zz_assert( n > 0 );
  zz_assert( n < MIX_MAX );

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
    case TRIG_STOP: K->xtp = 0;
    case TRIG_NOP:
      break;
    default:
      zz_assert(!"wtf");
      return -1;
    }
  }

  if (!M->wptr) {
    MFP[0x19] = 0;
    VEC = r1 = rout;
    M->wptr = r2 = r1 + (n1=n);
    n2 = 0;
  }
  else {
    timer_rout_t * const rptr = VEC;
    MFP[0x1f] = M->tdr;
    MFP[0x19] = M->tcr;

    r1 = M->wptr;
    if (rptr > r1) {
      if (n1 = rptr-r1, n1 > n)
        n1 = n;
      M->wptr = r2 = r1+n1;
      n2 = 0;
    } else if (n1 = tuor-r1, n1 > n) {
      n1 = n;
      M->wptr = r2 = r1+n1;
      n2 = 0;
    } else {
      n2 = n - n1;
      r2 = rout;
      M->wptr = r2+n2;
    }
    if (M->wptr >= tuor)
      M->wptr = rout + (M->wptr-tuor);
  }

  fast_stf(Tpcm, r1, temp, M->fast, n1);
  fast_stf(Tpcm, r2, temp, M->fast, n2);

  return n;
}


static zz_err_t init_stf(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_stf_t * const M = &g_stf;
  uint32_t refspr;

  init_timer_routines();
  init_replay_table();
  P->mixer_data = M;
  P->spr = mulu(P->song.khz,1000u);
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

  /* spr = SPR_MAX; */
  spr = set_sampling(P,spr);
  M->scl = ( divu(refspr<<13,spr) + 1 ) >> 1;

  prepare_timer();
  prepare_sound();
  prepare_insts(P->vset.inst, P->vset.nbi);

  return ecode;
}


static void free_stf(play_t * const P)
{
  stop_timer(P);
  stop_sound();
  P->mixer_data = 0;
}
