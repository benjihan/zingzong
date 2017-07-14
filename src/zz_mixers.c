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

 mixer_t * const zz_mixers[] = {
  &mixer_zz_none, &mixer_zz_qerp,
#ifndef NO_SOXR
  &mixer_soxr_qq, &mixer_soxr_lq, &mixer_soxr_mq,
  &mixer_soxr_hq, &mixer_soxr_vhq,
#endif
  /* */
  0
};
