/**
 * @file   mix_void.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-31
 * @brief  dummy mixer that does nothing (use to measure duration).
 */

#include "zz_private.h"

static void f0_void(play_t * const P) { }
static int  f1_void(play_t * const P) { return E_OK; }
mixer_t mixer_void = {
  "void" , "I didn't do nothing", f1_void, f0_void, f1_void
};
