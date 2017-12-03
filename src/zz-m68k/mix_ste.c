/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA).
 */

#include "../zz_private.h"

static zz_err_t init_ste(play_t * const, u32_t);
static void     free_ste(play_t * const);
static i16_t    push_ste(play_t * const, void *, i16_t);

mixer_t * mixer_ste(mixer_t * const M)
{
  M->name = "stdma8";
  M->desc = "Atari ST via 8-bit DMA";
  M->init = init_ste;
  M->free = free_ste;
  M->push = push_ste;
  return M;
}

/* $$$ GB: TEMP for linker test. */
const char mix_ste_version[] = __DATE__;
uint8_t mix_ste_bss[128];
/* $$$ GB: TEMP for linker test. */

static i16_t push_ste(play_t * const P, void *pcm, i16_t n)
{
  zz_assert( 0 );
  return -1;
}

static zz_err_t init_ste(play_t * const P, u32_t spr)
{
  zz_assert( 0 );
  return E_666;
}

static void free_ste(play_t * const P)
{
  zz_assert( 0 );
}
