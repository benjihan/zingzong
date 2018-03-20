/**
 * @file   mix_ste_hub.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-03-20
 * @brief  Atari 8-bit DMA autoselect.
 */

#include "../zz_private.h"
#include "mix_ata.h"

#define MIXALIGN      1
#define TICKMAX       256
#define FIFOMAX       (TICKMAX*3)
#define TEMPMAX       TICKMAX
#define ALIGN(X)      ((X)&-(MIXALIGN))
#define ULIGN(X)      ALIGN((X)+MIXALIGN-1)
#define IS_ALIGN(X)   ((X) == ALIGN(X))
#define init_spl(P)   init_spl(P)

#define fast_mix(R,N) fast_ste(mixtbl, R, _temp, M->ata.fast, N)
typedef int8_t spl_t;

ZZ_EXTERN_C mixer_t * mixer_ste_dnr(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste_lrb(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste_s7s(mixer_t * const M);

ZZ_STATIC zz_err_t init_ste_hub(core_t * const, u32_t);
ZZ_STATIC void     free_ste_hub(core_t * const);
ZZ_STATIC i16_t    push_ste_hub(core_t * const, void *, i16_t);

mixer_t * mixer_ste_hub(mixer_t * const M)
{
  M->name = "dma8:hub";
  M->desc = "8-bit DMA autoselect";
  M->init = init_ste_hub;
  M->free = free_ste_hub;
  M->push = push_ste_hub;
  return M;
}

/* ---------------------------------------------------------------------- */

ZZ_STATIC zz_err_t
init_ste_hub(core_t * const P, u32_t spr)
{
  mixer_t * mixer;

  zz_assert( P );
  zz_assert( P->mixer );

  /* Setup the right mixer depending on the initial blending value. */
  if (P->lr8 == 128) {
    mixer = mixer_ste_dnr( P->mixer );
  }
  else if ( !(uint8_t)P->lr8 ) {
    mixer = mixer_ste_s7s( P->mixer );
  }
  else {
    mixer = mixer_ste_lrb( P->mixer );
  }

  zz_assert( mixer );
  zz_assert( mixer->init != init_ste_hub );

  P->mixer = mixer;

  return mixer->init(P,spr);
}

ZZ_STATIC void
free_ste_hub(core_t * const P)
{
  zz_assert(!"free_ste_hub() should not be called");
  ILLEGAL;
}

ZZ_STATIC i16_t
push_ste_hub(core_t * const P, void * pcm, i16_t n)
{
  zz_assert(!"push_ste_hub() should not be called");
  ILLEGAL;
  return -1;
}
