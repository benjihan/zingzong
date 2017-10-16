/**
 * @file   mix_fal.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari Falcon mixer (16-bit DMA).
 */

#include "../zz_private.h"

static zz_err_t init_fal(play_t * const, u32_t);
static void     free_fal(play_t * const);
static void    *push_fal(play_t * const, void *, i16_t);

mixer_t * mixer_fal(mixer_t * const M)
{
  M->name = "falcon";
  M->desc = "Atari Falcon via 16 bit DMA";
  M->init = init_fal;
  M->free = free_fal;
  M->push = push_fal;
  return M;
}

const char mix_fal_version[] = __DATE__;
uint8_t mix_fal_bss[256];

static void *push_fal(play_t * const P, void *pcm, i16_t n)
{
  zz_assert( 0 );
  return 0;
}

static zz_err_t init_fal(play_t * const P, u32_t spr)
{
  zz_assert( 0 );
  return E_666;
}

static void free_fal(play_t * const P)
{
  zz_assert( 0 );
}
