/**
 * @file   mix_lerp.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Linear interpolation.
 */

#define NAME "int"
#define METH "lerp"
#define SYMB mixer_zz_lerp
#define DESC "linear interpolation (very fast)"

#define ZZ_DBG_PREFIX "(mix-" METH  ") "
#include "zz_private.h"

#define OPEPCM(OP) do {                         \
    zz_assert( pcm+(idx>>FP)+0 < K->end );      \
    zz_assert( pcm+(idx>>FP)+1 < K->end );      \
    *b++ OP lerp(pcm,idx);                      \
    idx += stp;                                 \
  } while (0)

/* Linear Interpolation.
 */
static inline int lerp(const uint8_t * const pcm, u32_t idx)
{
  const i32_t i = idx >> FP;
  const i16_t a = pcm[i+0]-128;           /* f(0) */
  const i16_t b = pcm[i+1]-128;           /* f(1) */
  const i16_t j = idx & ((1<<FP)-1);

  const i16_t r = ( (b * j) + (a * ((1<<FP)-j)) ) >> ( FP - 6 );

#if 0
  if ( unlikely (r < -0x2000) )
    return -0x2000;
  else if ( unlikely (r > 0x1fff) )
    return 0x1fff;
#else
  zz_assert( r >= -0x2000 );
  zz_assert( r <   0x2000 );
#endif

  return r;
}

static zz_err_t init_meth(play_t * P)
{
  const uint8_t * end = (uint8_t *)P->vset.bin->ptr+P->vset.bin->max;
  int i;

  for (i=0; i<P->vset.nbi; ++i) {
    const i32_t len = P->vset.inst[i].len;
    const i32_t lpl = P->vset.inst[i].lpl;

    if (len) {
      uint8_t * const pcm = P->vset.inst[i].pcm;
      P->vset.inst[i].end = len+1;   /* lerp needs 1 additional PCM */
      if (pcm+P->vset.inst[i].end > end) {
        emsg(METH ": I#%02hu out of range\n",HU(i));
        return E_MIX;
      }

      pcm[len] = !lpl ? 128 : pcm[len-lpl];
    }
  }
  return ZZ_OK;
}

#include "mix_common.c"
