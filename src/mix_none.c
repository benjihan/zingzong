/**
 * @file   mix_none.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Low quality no interpolation mixer.
 */

#include "zz_private.h"

#define SETPCM() *b++  = (int8_t)(pcm[idx>>FP]) << 6; idx += stp
#define ADDPCM() *b++ += (int8_t)(pcm[idx>>FP]) << 6; idx += stp

#define NAME none
#define DESC "no interpolation (very fast/LQ)"

#include "mix_common.c"
