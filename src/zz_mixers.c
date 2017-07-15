/**
 * @file   zz_mixers.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Supported mixers.
 */

/* ---------------------------------------------------------------------- */

#include "zz_private.h"

/* ---------------------------------------------------------------------- */

EXTERN_C mixer_t mixer_zz_none, mixer_zz_qerp;

#ifndef NO_SOXR
EXTERN_C mixer_t mixer_soxr_qq, mixer_soxr_lq, mixer_soxr_mq;
EXTERN_C mixer_t mixer_soxr_hq, mixer_soxr_vhq;
#endif

#ifndef NO_SRATE
EXTERN_C mixer_t mixer_srate_best, mixer_srate_medium, mixer_srate_fast;
EXTERN_C mixer_t mixer_srate_zero, mixer_srate_linear;
#endif

 mixer_t * const zz_mixers[] = {
  &mixer_zz_none, &mixer_zz_qerp,

#ifndef NO_SOXR
  &mixer_soxr_qq, &mixer_soxr_lq, &mixer_soxr_mq,
  &mixer_soxr_hq, &mixer_soxr_vhq,
#endif

#ifndef NO_SRATE
  &mixer_srate_best, &mixer_srate_medium, &mixer_srate_fast,
  &mixer_srate_zero, &mixer_srate_linear,
#endif
  /* */
  0
};
