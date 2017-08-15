/**
 * @file   mix_aga.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-11
 * @brief  Amiga/Paula mixer
 */

#include "../zz_private.h"

#define NAME "paula"
#define DESC "Amiga hardware channels mixer"


#define DMACON (*(volatile uint16_t *)0xDFF096)

#define PAULA_PAL  7093789u /* 7093789.2 */
#define PAULA_NTSC 7159090u /* 7159090.5 */

typedef struct mix_aga_s mix_aga_t;
typedef struct mix_chan_s mix_chan_t;
typedef struct audio_s audio_t;

struct audio_s {
  uint32_t adr;                        /* 00.l: sample address */
  uint16_t len;                        /* 04.w sample length in word (0=65536) */
  uint16_t per;                        /* 06.w period */
  uint16_t vol;                        /* 08.w volume [0..$40] */
  uint16_t dat;                        /* 0A.w current PCM */
};

struct mix_chan_s {
  volatile audio_t * hw;
  uint16_t dmacon;
  uint32_t lp_adr;
  uint16_t lp_len;
  uint8_t  status;

};

struct mix_aga_s {
  mix_chan_t chan[4];
};


static mix_aga_t g_aga;

static uint16_t xstep(uint32_t stp, uint8_t khz)
{
  static const uint32_t ftbl[][2] = {
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
    /* 20kHz */ { 0x00b15840, 0x00b2fa2e },
  };

  return divu(ftbl[khz-4][0]>>6,stp>>6);
}


static int
push_cb(play_t * const P)
{
  mix_aga_t * const M = (mix_aga_t *)P->mixer_data;
  int k;

  assert(P);
  assert(M);

  /* Setup channels */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    chan_t     * const C = P->chan+k;
    const uint8_t old_status = K->status;

    switch (K->status = C->trig) {
    case TRIG_NOTE: {
      uint32_t pcm, end;
      inst_t * const ins = C->note.ins;
      assert(ins);
      C->note.ins = 0;                  /* safety check */

      DMACON = K->dmacon;              /* Disable audio channel DMA */

      pcm = (intptr_t) ins->pcm;
      end = ins->end;
      if (end > (1<<17))
        end = 1 << 17;

      /* Program new sound start */
      K->hw->adr = pcm;
      K->hw->len = end >> 1;             /* 1<<17 => 0 => max size */
      K->hw->per = xstep(C->note.cur, P->song.khz);
      K->hw->vol = 64;
      DMACON = 0x8000 | K->dmacon;     /* Enable audio channel DMA */

      if (!ins->lpl) {
        K->lp_adr = pcm + end - 2;
        K->lp_len = 1;
      } else {
        K->lp_adr = pcm + ins->len - ins->lpl;
        K->lp_len = ins->lpl >> 1;
      }
    } break;

    case TRIG_SLIDE:
      K->hw->per = xstep(C->note.cur, P->song.khz);
    case TRIG_NOP:
      if (old_status == TRIG_NOTE) {
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
      assert(!"wtf");
      return E_MIX;
    }
  }

  return E_OK;
}

static int init_cb(play_t * const P)
{
  int ecode = E_OK;
  int16_t k;
  mix_aga_t * const M = &g_aga;

  P->mixer_data = M;
  zz_memclr(M,sizeof(*M));

  DMACON = 0x000F;                    /* disable audio DMAs */
  DMACON = 0x8200;                    /* enable DMAs */
  for (k=0; k<4; ++k) {
    mix_chan_t * const K = M->chan+k;
    K->dmacon = 1 << k;
    K->hw = (volatile audio_t *) ( 0xDFF0A0 + (k<<4) );
    K->hw->adr = 0;
    K->hw->len = 1;
    K->hw->vol = 0;
    K->hw->per = 0x100;
  }

  /* TO DO : prepare instrument */
  return ecode;
}

static void free_cb(play_t * const P)
{
  P->mixer_data = 0;
}

static mixer_t _mixer_aga;

/* Need to keep PCR */
EXTERN_C mixer_t * mixer_aga_get(void);

mixer_t * mixer_aga_get(void)
{
  _mixer_aga.name = NAME;
  _mixer_aga.desc = DESC;
  _mixer_aga.init = init_cb;
  _mixer_aga.free = free_cb;
  _mixer_aga.push = push_cb;
  return &_mixer_aga;
}