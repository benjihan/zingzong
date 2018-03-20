/**
 * @file   mix_aga.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-11
 * @brief  Amiga/Paula mixer
 */

#include "../zz_private.h"

static zz_err_t init_aga(core_t * const, u32_t);
static void     free_aga(core_t * const);
static i16_t    push_aga(core_t * const, void *, i16_t);

#define PER_MIN_PAL  123
#define PER_MIN_NTSC 124
#define PER_MIN      PER_MIN_NTSC

mixer_t * mixer_aga(mixer_t * const M)
{
  M->name = "paula";
  M->desc = "Amiga hardware channels mixer";
  M->init = init_aga;
  M->free = free_aga;
  M->push = push_aga;
  return M;
}

#define DMACON (*(volatile uint16_t *)0xDFF096)
#define PAULA_PAL  7093789u /* 7093789.2 */
#define PAULA_NTSC 7159090u /* 7159090.5 */

typedef struct mix_aga_s mix_aga_t;
typedef struct mix_chan_s mix_chan_t;
typedef struct audio_s audio_t;

struct audio_s {
  uint32_t adr;             /* 00.l: sample address */
  uint16_t len;             /* 04.w sample length in word (0=65536) */
  uint16_t per;             /* 06.w period */
  uint16_t vol;             /* 08.w volume [0..$40] */
  uint16_t dat;             /* 0A.w current PCM */
};

struct mix_chan_s {
  volatile audio_t * hw;
  uint16_t dmacon;
  uint32_t lp_adr;
  uint16_t lp_len;
  uint8_t  status;
  uint8_t  inum;
  uint8_t  oct;
};

struct mix_aga_s {
  mix_chan_t chan[4];
  struct aga_inst {
    uint32_t adr, lp_adr;
    uint16_t len, lp_len;
    uint8_t num,oct,align[2];
  } inst[20];
};

static uint8_t tohw[256];

static mix_aga_t g_aga;

/* @see xstep.py */
static uint16_t _xstep(uint32_t stp, uint8_t khz)
{
  static const uint32_t ftbl[][2] = {
    /* SAMPLING   PAL         NTSC       */
    /* 04kHz */ { 0x0376b941, 0x037ee2e5 },
    /* 05kHz */ { 0x02c56101, 0x02cbe8b8 },
    /* 06kHz */ { 0x024f262b, 0x02549744 },
    /* 07kHz */ { 0x01fab301, 0x01ff5d15 },
    /* 08kHz */ { 0x01bb5ca1, 0x01bf7173 },
    /* 09kHz */ { 0x018a1972, 0x018dba2d },
    /* 10kHz */ { 0x0162b080, 0x0165f45c },
    /* 11kHz */ { 0x014271e9, 0x014569c8 },
    /* 12kHz */ { 0x01279316, 0x012a4ba2 },
    /* 13kHz */ { 0x0110d68a, 0x01135982 },
    /* 14kHz */ { 0x00fd5980, 0x00ffae8b },
    /* 15kHz */ { 0x00ec75ab, 0x00eea2e8 },
    /* 16kHz */ { 0x00ddae50, 0x00dfb8b9 },
    /* 17kHz */ { 0x00d0a40f, 0x00d28fbe },
    /* 18kHz */ { 0x00c50cb9, 0x00c6dd17 },
    /* 19kHz */ { 0x00baadbd, 0x00bc65aa },
    /* 20kHz */ { 0x00b15840, 0x00b2fa2e }
  };
  zz_assert( (stp>>3) < 0x10000 );
  return divu(ftbl[khz-4][0]>>3, stp>>3);
}

static uint16_t xstep(uint32_t stp, uint8_t khz, uint8_t inum)
{
  uint16_t per = _xstep(stp,khz);
  if (per < PER_MIN) {                      /*  */
    /* Sometime we reach periods that excess the DMA speed. This is
     * unfortunate and the only way to prevent that would be to
     * down-sample the instruments.
     */
    dmsg("period overflow I#%02hu stp:0x%lx per:%hu\n",
         HU(inum), LU(stp), HU(per));
    per = PER_MIN;
  }
  return per;
}

static i16_t push_aga(core_t * const P, void * pcm, i16_t npcm)
{
  mix_aga_t * const M = (mix_aga_t *) P->data;
  int k;
  const uint8_t lr_swap = (P->lr8>128) << 1;

  zz_assert(P);
  zz_assert( M == &g_aga );

  /* Setup channels */
  for (k=0; k<4; ++k) {
    chan_t     * const C = P->chan+k;
    mix_chan_t * const K = M->chan+(C->pam^lr_swap);
    const uint8_t old_status = K->status;

    switch (K->status = C->trig) {
    case TRIG_NOTE:
      zz_assert(C->note.ins == &P->vset.inst[C->curi]);
      DMACON = K->dmacon;              /* Disable audio channel DMA */
      K->inum    = C->curi;
      K->oct     = M->inst[C->curi].oct;
      K->lp_adr  = M->inst[C->curi].lp_adr;
      K->lp_len  = M->inst[C->curi].lp_len;
      K->hw->adr = M->inst[C->curi].adr;
      K->hw->len = M->inst[C->curi].len;
      K->hw->vol = 48;
      K->hw->per = xstep(C->note.cur >> K->oct, P->song.khz, K->inum);
      DMACON = 0x8000 | K->dmacon;      /* Enable audio channel DMA */
      break;

    case TRIG_SLIDE:
      K->hw->per = xstep(C->note.cur >> K->oct, P->song.khz, K->inum);
    case TRIG_NOP:
      if (old_status == TRIG_NOTE) {
        /* Setting the sample loop at the frame following the note
           trigger. Hopefully it's good enough but in some extreme
           cases it might trigger problems with unwanted loop. */
        K->hw->adr = K->lp_adr;
        K->hw->len = K->lp_len;
      }
      break;

    case TRIG_STOP:
      DMACON = K->dmacon;              /* Disable audio channel DMA */
      K->hw->vol = 0;                  /* Mute */
      K->hw->dat = 0;                  /* set current PCM to 0 */
      break;

    default:
      zz_assert(!"wtf");
      return -1;
    }
  }

  return npcm;
}

static zz_err_t init_aga(core_t * const P, u32_t spr)
{
  int ecode = E_OK;
  int16_t k;
  mix_aga_t * const M = &g_aga;

  P->data = M;
  P->spr = mulu(P->song.khz,1000u);
  zz_memclr(M,sizeof(*M));

  DMACON = 0x000F;                    /* disable audio DMAs */
  DMACON = 0x8200;                    /* enable DMAs */
  for (k=0; k<4; ++k) {
    static const uint8_t order[4] = { 0,3,1,2 }; /* A/D/C/B */
    mix_chan_t * const K = M->chan+k;
    const uint8_t o = order[k];
    K->dmacon = 1 << o;
    K->hw = (volatile audio_t *) ( 0xDFF0A0 + (o<<4) );
    K->hw->adr = 0;
    K->hw->len = 1;
    K->hw->vol = 0;
    K->hw->per = 0x100;
  }

  /* Because of Paula limitation some sample need down-sampling. The
   * song parser stores the maximum step used by instrument.
   */
  for (k=0; k<20; ++k) {
    inst_t * const inst = P->vset.inst + k;
    const uint16_t permin = PER_MIN;
    uint16_t per;
    uint8_t oct;

    if (!inst->len) continue;
    if (!P->song.istep[k]) continue;

    oct = 0;
    per = _xstep( (u32_t) P->song.istep[k] << 12, P->song.khz);
    if (!per) continue;
    while ( per < permin ) {
      per <<= 1;
      ++oct;
    }

    if (oct) {
      uint16_t i, j, is = 1 << oct;

      dmsg("I#%02hu has been downsampled by %hu (per was %hu)\n",
           HU(k+1), HU(is), HU(per>>oct));
      inst->len >>= oct;
      inst->lpl >>= oct;
      for (i=j=0; j<inst->len; i+=is, ++j)
        inst->pcm[j] = inst->pcm[i];
      M->inst[k].oct = oct;
    }
  }

  /* Change PCM sign and unroll loops. */
  if (!*tohw)
    for (k=0; k<256; ++k)
      tohw[k] = k-128;
  vset_unroll(&P->vset,tohw);

  for (k=0; k<20; ++k) {
    inst_t * const inst = P->vset.inst + k;
    u32_t nw;

    M->inst[k].num = k;
    M->inst[k].len = 1;
    if (!inst->len) continue;
    M->inst[k].adr = -2 & ( (intptr_t) inst->pcm + 1 );

    /* Get unrolled end with hardware limitation (even,<128KiB) */
    nw = inst->end >> 1;
    if (nw > 0x10000) nw = 0x10000;   /* hardware limit */

    if (!inst->lpl) {
      uint8_t * lp_addr = &inst->pcm[(inst->end-2)&-2];
      M->inst[k].len = nw;
      M->inst[k].lp_adr = (intptr_t)lp_addr;
      M->inst[k].lp_len = 1;
      lp_addr[0] = lp_addr[1] = 0;
    } else {
      /* GB: Include as much as unrolled complete loop in both
       *     runs. The 32-bit div/mul might be a bit overkill but
       *     technically in extreme cases the classic m68k
       *     instructions could overflow.
       *
       * GB: Using unrolled data in the loop partially fix de-tuning
       *     caused by cutting very small odd length due to even
       *     hardware limitation. We could do one better by selecting
       *     the loop point and the loop length at even address
       *     both. However as long as the unroll space is large enough
       *     it should not matter that much.
       */
      uint32_t xtra_len = (nw<<1) - inst->len;
      uint32_t loop_cnt = divu32(xtra_len, inst->lpl);
      uint32_t loop_len = mulu32(loop_cnt, inst->lpl);

      dmsg("I#%02u len:%lu lpl:%lu end:%lu xtra:%lu loop:%lux%lu=%lu \n",
           HU(k+1), LU(inst->len), LU(inst->lpl), LU(inst->end),
           LU(xtra_len), LU(loop_cnt), LU(inst->lpl), LU(loop_len));

      zz_assert( (inst->len+loop_len) >> 1 <= 0x10000 );
      M->inst[k].len = (inst->len + loop_len) >> 1;
      M->inst[k].lp_adr = -2 & ((intptr_t)inst->pcm+inst->len-inst->lpl);
      M->inst[k].lp_len = (inst->lpl+loop_len) >> 1;
    }
  }

  return ecode;
}

static void free_aga(core_t * const P)
{
  P->data = 0;
}
