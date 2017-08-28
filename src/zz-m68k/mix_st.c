/**
 * @file   mix_st.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari ST mixer
 */

#include "../zz_private.h"

enum {
 ATARI_UNKNOWN, ATARI_ST, ATARI_STE, ATARI_FALCON
};

static int push_st_cb(play_t * const P)
{
  return E_MIX;
}

static int push_ste_cb(play_t * const P)
{
  return E_MIX;
}

static int push_falcon_cb(play_t * const P)
{
  return E_MIX;
}

static int init_st_cb(play_t * const P)
{
  return E_666;
}

static int init_ste_cb(play_t * const P)
{
  return E_666;
}

static int init_falcon_cb(play_t * const P)
{
  return E_666;
}

static void free_st_cb(play_t * const P)
{
}

static void free_ste_cb(play_t * const P)
{
}

static void free_falcon_cb(play_t * const P)
{
}


static mixer_t _mixer_st;

/* Need to keep PCR */
EXTERN_C mixer_t * mixer_get(void);

static int atari_machine_detection()
{
  return ATARI_ST;
}

mixer_t * mixer_get(void)
{
  switch ( atari_machine_detection() ) {
  case ATARI_ST:
      _mixer_st.name = "Atari-ST";
      _mixer_st.desc = "Atari-ST via YM-2149";
      _mixer_st.init = init_st_cb;
      _mixer_st.free = free_st_cb;
      _mixer_st.push = push_st_cb;
      break;
  case ATARI_STE:
      _mixer_st.name = "Atari-STe";
      _mixer_st.desc = "Atari-STe via audio DMA";
      _mixer_st.init = init_ste_cb;
      _mixer_st.free = free_ste_cb;
      _mixer_st.push = push_ste_cb;
      break;

  case ATARI_FALCON:
      _mixer_st.name = "Atari-Falcon";
      _mixer_st.desc = "Atari-Falcon via audio DMA";
      _mixer_st.init = init_falcon_cb;
      _mixer_st.free = free_falcon_cb;
      _mixer_st.push = push_falcon_cb;
      break;
  }

  return &_mixer_st;
}
