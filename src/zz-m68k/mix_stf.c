/**
 * @file   mix_stf.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STf mixer (YM-2149).
 */

#include "../zz_private.h"

#undef SPR_MIN
#define SPR_MIN 4000

#undef SPR_MAX
#define SPR_MAX 30000u

#define YML ((volatile uint32_t *)0xFFFF8800)
#define YMB ((volatile uint8_t *)0xFFFF8800)
#define MFP ((volatile uint8_t *)0xFFFFFA00)
#define VEC (*(volatile timer_f *)0x134)

static zz_err_t init_stf(play_t * const P, u32_t spr);
static void     free_stf(play_t * const P);
static void    *push_stf(play_t * const P, void *pcm, i16_t npcm);

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

typedef void (*timer_f)(void);

struct mix_chan_s {
  int8_t *pcm, *end;
  u32_t  idx, lpl, len, xtp;
};
typedef struct mix_chan_s mix_chan_t;

#define MIXMAX ((SPR_MAX/200)+4)
struct mix_stf_s {
  mix_chan_t chan[4];

  uint16_t scl;
  uint8_t tcr, tdr;
  volatile int16_t *cur;
  int16_t *phy, *log, mix[2][MIXMAX];
};
typedef struct mix_stf_s mix_stf_t;

static mix_stf_t g_stf;

static uint8_t Tpcm[1024*4];

static void never_inline init_replay_table(void)
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

static void __attribute__ ((interrupt)) timer()
{
  const uint8_t * pcm = &Tpcm[512*4] + ( *g_stf.cur++ << 2 ) ;

    /* if (pcm < Tpcm || pcm >= &Tpcm[sizeof(Tpcm)]) */
    /*   asm volatile("illegal\n\t"); */

  YMB[0] = 8;    YMB[2] = *pcm++;
  YMB[0] = 9;    YMB[2] = *pcm++;
  YMB[0] = 10;   YMB[2] = *pcm++;
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
  /* YMB[2] = YMB[0] | 0x3f; */
  YMB[2] = (YMB[0] & 0370) | 0070;
  asm volatile ("move.w %%sr,%[savesr] \n\t" : : [savesr] "m" (savesr) );
  clear_ym_regs();                 /* A98-xx54-3210 */
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
  VEC = timer;
}

#define OPEPCM(OP) do {                         \
    zz_assert( &pcm[idx>>FP] < K->end );        \
    *b++ OP (int8_t) pcm[idx>>FP];              \
    idx += stp;                                 \
  } while (0)
#define SETPCM() OPEPCM(=);
#define ADDPCM() OPEPCM(+=);

static void never_inline
mix_add1(mix_chan_t * const restrict K, int16_t * restrict b, int n)
{
  const int8_t * const pcm = (const int8_t *)K->pcm;
  u32_t idx = K->idx, stp = K->xtp;

  if (n <= 0 || !K->xtp)
    return;

  zz_assert( K->pcm );
  zz_assert( K->end > K->pcm );
  zz_assert( K->idx >= 0 && K->idx < K->len );

  if (!K->lpl) {
    /* Instrument does not loop */
    do {
      ADDPCM();
      /* Have reach end ? */
      if (idx >= K->len) {
        idx = stp = 0;
        break;
      }
    } while(--n);
  } else {
    const u32_t off = K->len - K->lpl;  /* loop start index */
    /* Instrument does loop */
    do {
      ADDPCM();
      /* Have reach end ? */
      if (idx >= K->len) {
        u32_t ovf = idx - K->len;
        if (ovf >= K->lpl) ovf = modu(ovf,K->lpl);
        idx = off+ovf;
        zz_assert( idx >= off && idx < K->len );
      }
    } while(--n);
  }
  K->idx = idx;
  K->xtp = stp;
}


static inline u32_t xstep(u32_t stp, uint16_t f12)
{
  return mulu(stp>>4, f12) >> (24-FP);
}

static void * push_stf(play_t * const P, void * pcm, i16_t n)
{
  mix_stf_t * const M = (mix_stf_t *)P->mixer_data;
  int k;

  zz_assert(P);
  zz_assert(M);
  zz_assert(n>0);
  zz_assert(n<MIXMAX);

  if (1) {
    int16_t a = P->pcm_per_tick;
    int16_t b = a+(!!P->pcm_err_tick);
    if (n != a && n != b)
      ILLEGAL;
  }

  n = P->pcm_per_tick + (!!P->pcm_err_tick);

  /* Swap buffers */
  if (1) {
    int16_t * cur = M->log;
    M->log = M->phy;
    M->phy = cur;
    M->cur = cur;
  } else {
    M->cur = M->log;
  }

  if (!MFP[0x19]) {
    MFP[0x1f] = M->tdr;
    MFP[0x19] = M->tcr;
  }

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;
    const u8_t trig = C->trig;

    C->trig = TRIG_NOP;
    switch (trig) {
    case TRIG_NOTE:

      zz_assert(C->note.ins);
      K->idx = 0;
      K->pcm = (int8_t *) C->note.ins->pcm;
      K->len = C->note.ins->len << FP;
      K->lpl = C->note.ins->lpl << FP;
      K->end = C->note.ins->end + K->pcm;

    case TRIG_SLIDE:
      K->xtp = xstep(C->note.cur, M->scl);
      break;
    case TRIG_STOP: K->xtp = 0;
    case TRIG_NOP:
      break;
    default:
      zz_assert(!"wtf");
      return 0;
    }
  }

  /* Clear mix buffer */
  zz_memclr(M->log,n<<1);

  /* Add voices */
  for (k=0; k<4; ++k)
    mix_add1(M->chan+k, M->log, n);
  /* M->log[n] = M->log[n+1] = M->log[n-1]; */

  return (int16_t) 0xBEEFFACE;
}


static zz_err_t init_stf(play_t * const P, u32_t spr)
{
  int ecode = E_OK;
  mix_stf_t * const M = &g_stf;
  uint32_t refspr;

  init_replay_table();
  P->mixer_data = M;
  P->spr = mulu(P->song.khz,1000u);
  zz_memclr(M,sizeof(*M));

  M->log = M->mix[0];
  M->phy = M->mix[1];
  M->cur = 0;

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

  spr = SPR_MAX;
  spr = set_sampling(P,spr);
  M->scl = ( divu(refspr<<13,spr) + 1 ) >> 1;

  prepare_timer();
  prepare_sound();

  return ecode;
}


static void free_stf(play_t * const P)
{
  stop_timer(P);
  stop_sound();
  P->mixer_data = 0;
}
