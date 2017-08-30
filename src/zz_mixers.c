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
# ifndef DEFAULT_MIXER
#  define DEFAULT_MIXER mixer_soxr_hq
# endif
#endif

#if WITH_SRATE == 1
EXTERN_C mixer_t mixer_srate_best, mixer_srate_medium, mixer_srate_fast;
# ifndef DEFAULT_MIXER
#  define DEFAULT_MIXER mixer_srate_medium
# endif
#endif

static mixer_t * const zz_mixers[] = {
  &mixer_zz_qerp, &mixer_zz_lerp, &mixer_zz_none,

#if WITH_SOXR == 1
  &mixer_soxr_hq,
#endif

#if WITH_SRATE == 1
  &mixer_srate_best, &mixer_srate_medium, &mixer_srate_fast,
#endif

  0
};

#ifndef DEFAULT_MIXER
# define DEFAULT_MIXER mixer_zz_qerp
#endif

static mixer_t * default_mixer = &DEFAULT_MIXER;


static mixer_t * get_mixer(int * const n)
{
  const int max = sizeof(zz_mixers)/sizeof(*zz_mixers) - 1;
  mixer_t * mixer = 0;
  if (*n == ZZ_DEFAULT_MIXER) {
    int i;
    mixer = default_mixer;
    for (i=0; i<max; ++i)
      if (mixer == zz_mixers[i]) {
        *n = i; break;
      }
  } else if (*n < max) {
    mixer = zz_mixers[*n];
  } else {
    *n = -1;
  }
  return mixer;
}

int zz_mixer_enum(int n, const char ** pname, const char ** pdesc)
{
  const mixer_t * mixer;

  if (mixer = get_mixer(&n), mixer) {
    *pname = mixer->name;
    *pdesc = mixer->desc;
  }
  return n;
}

int zz_mixer_set(play_t * P, int n)
{
  mixer_t * mixer;

  if (mixer = get_mixer(&n), mixer) {
    if (!P)
      default_mixer = mixer;
    else
      P->mixer = mixer;
  }
  return n;
}
