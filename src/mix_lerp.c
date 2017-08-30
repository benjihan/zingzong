/**
 * @file   mix_lerp.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Lagrange polynomial quadratic interpolation.
 */

#include "zz_private.h"

#define SETPCM() *b  = lerp(pcm,idx); ++b; idx += stp
#define ADDPCM() *b += lerp(pcm,idx); ++b; idx += stp

#define NAME "int"
#define METH "lerp"
#define SYMB mixer_zz_lerp
#define DESC "linear interpolation (very fast)"

/* Linear Interpolation.
 */
static inline int lerp(const int8_t * const pcm, uint_t idx)
{
  const int i = idx >> FP;
  const int a = pcm[i+0];              /* f(0) */
  const int b = pcm[i+1];              /* f(1) */
  const int j = idx & ((1<<FP)-1);

  const int r = ( (b * j) + (a * ((1<<FP)-j)) ) >> ( FP - 6 );

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

#include "mix_common.c"
