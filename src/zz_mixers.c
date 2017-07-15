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

#if WITH_SOXR == 1
EXTERN_C mixer_t mixer_soxr_qq, mixer_soxr_lq, mixer_soxr_mq;
EXTERN_C mixer_t mixer_soxr_hq, mixer_soxr_vhq;
#endif

#if WITH_SRATE == 1
EXTERN_C mixer_t mixer_srate_best, mixer_srate_medium, mixer_srate_fast;
EXTERN_C mixer_t mixer_srate_zero, mixer_srate_linear;
#endif

#if WITH_SMARC == 1
EXTERN_C mixer_t mixer_smarc;
#endif

mixer_t * const zz_mixers[] = {
  &mixer_zz_none, &mixer_zz_qerp,

#if WITH_SOXR == 1
  &mixer_soxr_qq, &mixer_soxr_lq, &mixer_soxr_mq,
  &mixer_soxr_hq, &mixer_soxr_vhq,
#endif

#if WITH_SRATE == 1
  &mixer_srate_best, &mixer_srate_medium, &mixer_srate_fast,
  &mixer_srate_zero, &mixer_srate_linear,
#endif

#if WITH_SMARC == 1
  &mixer_smarc,
#endif

  0
};
