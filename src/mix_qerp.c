/**
 * @file   mix_qerp.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Lagrange polynomial quadratic interpolation.
 */

#define NAME "int"
#define METH "qerp"
#define SYMB mixer_zz_qerp
#define DESC "lagrange quadratic interpolation (fast,MQ)"

#define ZZ_DBG_PREFIX "(mix-" METH  ") "
#include "zz_private.h"

#define OPEPCM(OP) do {                         \
    zz_assert( pcm+(idx>>FP)+0 < K->end );      \
    zz_assert( pcm+(idx>>FP)+1 < K->end );      \
    zz_assert( pcm+(idx>>FP)+2 < K->end );      \
    *b++ OP lagrange(pcm,idx);                  \
    idx += stp;                                 \
  } while(0)

/* Lagrange Polynomial Quadratic interpolation.
 *
 * GB: Interpolation is a reconstruction filter. It's relatively good
 *     for up-sampling. That usually our case when playing for modern
 *     sampling rate standard.
 *
 *     However this is not a proper interpolation filter for DSP
 *     purpose. It causes -- zero-order hold -- distortion in the
 *     original pass-band. We should pass the interpolated data
 *     through a low pass filter (anti-imaging or interpolation
 *     filter). In DSP, interpolation is also called zero-packing.
 *
 *     For down-sampling we need anti-aliasing filter usually some
 *     kind of low-pass filter prior to a simple decimation process.
 */
static inline int16_t lagrange(const int8_t * const pcm, u32_t idx)
{
  const i32_t i = idx >> FP;
  const i32_t j = (idx >> (FP-7u)) & 0x7F; /* the mid poi32_t is f(.5) */

  const i32_t p1 = pcm[i+0];              /* f(0) */
  const i32_t p2 = pcm[i+1];              /* f(.5) */
  const i32_t p3 = pcm[i+2];              /* f(1) */

  /* f(x) = ax^2+bx+c */
  const i32_t c =    p1            ;
  const i32_t b = -3*p1 +4*p2 -  p3;
  const i32_t a =  2*p1 -4*p2 +2*p3;

  /* x is fp8; x^2 is fp16; r is fp16 => 24bit */
  i32_t r =
    ( ( a * j * j) +
      ( b * j << 8 ) +
      ( c << 16 )
      );

  /* scale r to 14-bit so that 4 voices fit into a 16 bit integer
   * Apply an empirical additional 3:4 scale to avoid clipping as the
   * interpolation can generate values outside the sample range.
   */
  r = ( r * 3 ) >> ( 2+24-14 );

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
