/**
 * @file   zz_m68k.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-11-27
 * @brief  m68k special.
 */

#ifndef ZZ_M68K_H
#define ZZ_M68K_H

/* GB: Keep order in sync with sc68 track in the test case.
 *     We do (d0-1) so that d0 is 0 for auto-detect, 1 for Amiga ...
 */

enum {
  MIXER_AGA,                /**< Amiga mixer identifier.             */
  MIXER_STF,                /**< PSG mixer identifier.               */
  MIXER_S7S,                /**< stereo 8-bit DMA mixer identifier.  */
  MIXER_FAL,                /**< stereo 16-bit DMA mixer identifier. */
  MIXER_STE,                /**< mono 8-bit DMA mixer identifier.    */

  /**/
  MIXER_LAST                        /**< Number of valid mixer.     */
};

#endif
