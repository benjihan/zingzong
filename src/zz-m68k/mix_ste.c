/**
 * @file   mix_ste.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STe mixer (8-bit DMA).
 */

#include "../zz_private.h"

static zz_err_t init_ste_cb(play_t * const P);
static void     free_ste_cb(play_t * const P);
static zz_err_t push_ste_cb(play_t * const P);

mixer_t * mixer_ste(mixer_t * const M)
{
  M->name = "stdma8";
  M->desc = "Atari ST via 8-bit DMA";
  M->init = init_ste_cb;
  M->free = free_ste_cb;
  M->push = push_ste_cb;
  return M;
}

/* $$$ GB: TEMP for linker test. */
const char mix_ste_version[] = __DATE__;
uint8_t mix_ste_bss[128];
/* $$$ GB: TEMP for linker test. */

static zz_err_t push_ste_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_MIX;
}

static zz_err_t init_ste_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_666;
}

static void free_ste_cb(play_t * const P)
{
  zz_assert( 0 );
}
