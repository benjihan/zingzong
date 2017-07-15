/**
 * @file   mix_soxr.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-14
 * @brief  Convertion between float and integer PCM.
 */

#include "zz_private.h"

#ifndef NO_FLOAT_SUPPORT

#include <math.h>

static float i8tofl_lut[256];

/** Initialize int8_t to float PCM look up table. */
static void
i8tofl_init(void)
{
    const float sc = 3.0 / (4.0*4.0*128.0);
    int i;
    for (i=-128; i<128; ++i)
      i8tofl_lut[i&0xFF] = sc * (float)i;
}

/* Convert int8_t PCM buffer to float PCM */
void
i8tofl(float * const d, const uint8_t * const s, const int n)
{
  static int init = 0;
  if (unlikely(!init)) {
    init = 1;
    i8tofl_init();
  }

  if (n > 0) {
    int i = 0;
    do {
      d[i] = i8tofl_lut[s[i]];
    } while (++i<n);
  }
}

/* Convert normalized float PCM buffer to in16_t PCM */
void
fltoi16(int16_t * const d, const float * const s, const int n)
{
  const float sc = 32768.0; int i;

  /* $$$ Slow conversion. Need some improvement once everything work */
  for (i=0; i<n; ++i) {
    int v;
    const float f = s[i] * sc;

    if ( unlikely(s[i] >= 1.0) )
      v = 32767;
    else if  ( unlikely(s[i] <= -1.0) )
      v = -32768;
    else
      v =  (int)( s[i] * sc );
    d[i] = v;
  }
}

#endif /* #ifndef NO_FLOAT_SUPPORT */
