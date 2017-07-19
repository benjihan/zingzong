/**
 * @file   zz_mixers.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Supported mixers.
 */

/* ---------------------------------------------------------------------- */

#include "zz_private.h"

/* ---------------------------------------------------------------------- */

EXTERN_C mixer_t mixer_zz_none, mixer_zz_lerp, mixer_zz_qerp;

#if WITH_SOXR == 1
EXTERN_C mixer_t mixer_soxr_hq;
# ifndef ZZ_DEF_MIXER
#  define ZZ_DEF_MIXER mixer_soxr_hq
# endif
#endif

#if WITH_SRATE == 1
EXTERN_C mixer_t mixer_srate_best, mixer_srate_medium, mixer_srate_fast;
# ifndef ZZ_DEF_MIXER
#  define ZZ_DEF_MIXER mixer_srate_medium
# endif
#endif

#if WITH_SMARC == 1
EXTERN_C mixer_t mixer_smarc;
#endif

mixer_t * const zz_mixers[] = {
  &mixer_zz_qerp, &mixer_zz_lerp, &mixer_zz_none,

#if WITH_SOXR == 1
  &mixer_soxr_hq,
#endif

#if WITH_SRATE == 1
  &mixer_srate_best, &mixer_srate_medium, &mixer_srate_fast,
#endif

#if WITH_SMARC == 1
  &mixer_smarc,
#endif

  0
};

#ifndef ZZ_DEF_MIXER
# define ZZ_DEF_MIXER mixer_zz_qerp
#endif

mixer_t * zz_default_mixer = &ZZ_DEF_MIXER;
