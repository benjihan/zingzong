/**
 * @file   mix_fal.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari Falcon mixer (16-bit DMA).
 */

#include "../zz_private.h"

static zz_err_t init_fal_cb(play_t * const P);
static void     free_fal_cb(play_t * const P);
static zz_err_t push_fal_cb(play_t * const P);

mixer_t * mixer_fal(mixer_t * const M)
{
  M->name = "falcon";
  M->desc = "Atari Falcon via 16 bit DMA";
  M->init = init_fal_cb;
  M->free = free_fal_cb;
  M->push = push_fal_cb;
  return M;
}

const char mix_fal_version[] = __DATE__;
uint8_t mix_fal_bss[256];

static zz_err_t push_fal_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_MIX;
}

static zz_err_t init_fal_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_666;
}

static void free_fal_cb(play_t * const P)
{
  zz_assert( 0 );
}
