/**
 * @file   mix_st.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari ST mixer
 */

#include "../zz_private.h"

enum {
 ATARI_UNKNOWN, ATARI_STE, ATARI_FALCON
};

static int push_ste_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_MIX;
}

static int push_falcon_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_MIX;
}

static int init_ste_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_666;
}

static int init_falcon_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_666;
}

static void free_ste_cb(play_t * const P)
{
  zz_assert( 0 );
}

static void free_falcon_cb(play_t * const P)
{
  zz_assert( 0 );
}

static
mixer_t * mixer_setup(mixer_t * const M, zz_u8_t machine)
{
  switch (machine) {

  case ATARI_STE:
    M->name = "Atari-STe";
    M->desc = "Atari-STe via audio DMA";
    M->init = init_ste_cb;
    M->free = free_ste_cb;
    M->push = push_ste_cb;
    break;

  case ATARI_FALCON:
    M->name = "Atari-Falcon";
    M->desc = "Atari-Falcon via audio DMA";
    M->init = init_falcon_cb;
    M->free = free_falcon_cb;
    M->push = push_falcon_cb;
    break;

  default:
    zz_assert(!"invalid machine");
    return 0;
  }

  return M;
}


mixer_t * mixer_ste(mixer_t * const M)
{
  return mixer_setup(M,ATARI_STE);
}

mixer_t * mixer_fal(mixer_t * const M)
{
  return mixer_setup(M,ATARI_FALCON);
}
