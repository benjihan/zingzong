/**
 * @file   zz_m68k.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-11-27
 * @brief  m68k special.
 */

#ifndef ZZ_M68K_H
#define ZZ_M68K_H

enum {
  MIXER_AGA,                        /**< Amiga mixer identifier.    */
  MIXER_STF,                        /**< PSG mixer identifier.      */
  MIXER_STE,                        /**< 8bit DMA mixer identifier. */
  MIXER_FAL,                        /**< 16bit DMA ixer identifier. */

  /**/
  MIXER_LAST                        /**< Number of valid mixer.     */
};

#endif
