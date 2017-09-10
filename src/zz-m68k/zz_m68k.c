
#include "../zz_private.h"

void bin_free(bin_t ** pbin) {}

enum {
  MIXER_AGA,
  MIXER_STF,
  MIXER_STE,
  MIXER_FAL,

  /**/
  MIXER_LAST
};

static mixer_t mixer;
static volatile intptr_t bus_vector, adr_vector;

/* tos cookie jar */
#define COOKIEJAR (* (uint32_t **) 0x5A0)

/* _SND cookie bits */
#define SND_YM2149    1
#define SND_8BIT_DMA  2
#define SND_16BIT_DMA 4
#define SND_DSP_56K   8

/* Amiga DMACON register */
#define DMACON (*(volatile uint16_t *)0xDFF096)

static void __attribute__((interrupt)) exception_handler(void)
{
  /* address register must match the asm code in guess_hardware() */
  asm volatile ("move.l %a4,2(%a7) \n\t");
}

#pragma GCC diagnostic ignored "-Wmultichar"

static
uint8_t guess_hardware(void)
{
  uint8_t id = MIXER_LAST, aga;

  BREAKP;

  /* Save vectors */
  bus_vector = 2[(volatile uint32_t *)0];
  adr_vector = 3[(volatile uint32_t *)0];

  /* Install handlers */
  2[(volatile uint32_t *)0] = (intptr_t) exception_handler;
  3[(volatile uint32_t *)0] = (intptr_t) exception_handler;

  /* GB: Totally hacked !!! */
  asm volatile (
    "sf.b  %[flag]     \n\t"
    "lea   1f,%%a4     \n\t"
    "move  #0,0xDFF096 \n\t"
    "st.b  %[flag]     \n\t"
    "1:                \n\t"
    : [flag] "=d" (aga)
    :
    : "a4","cc" );

  /* Restore vectors */
  2[(volatile uint32_t *)0] = bus_vector;
  3[(volatile uint32_t *)0] = adr_vector;

  if (aga)
    id = MIXER_AGA;
  else {
    uint32_t * jar = COOKIEJAR, cookie;
    uint8_t snd = SND_YM2149;

    while (cookie = *jar++, cookie) {
      uint32_t value = *jar++;
      if (cookie == '_SND') {
        snd = value;
        break;
      }
    }

    /* Ignoring DSP for now, hopefully we'll add a nice DSP mixer
     * later. */
    if ( snd & SND_16BIT_DMA )
      id = MIXER_FAL;
    else if ( snd & SND_8BIT_DMA )
      id = MIXER_STE;
    else if ( snd & SND_YM2149 )
      id = MIXER_STF;
  }

  return id;

}

ZZ_EXTERN_C mixer_t * mixer_aga(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_stf(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_fal(mixer_t * const M);

zz_u8_t zz_mixer_set(play_t * P, zz_u8_t n)
{
  if (n >= MIXER_LAST)
    n = guess_hardware();

  switch ( n ) {
  case MIXER_AGA: P->mixer = mixer_aga(&mixer); break;
  case MIXER_STF: P->mixer = mixer_stf(&mixer); break;
  case MIXER_STE: P->mixer = mixer_ste(&mixer); break;
  case MIXER_FAL: P->mixer = mixer_fal(&mixer); break;
  default: n = ZZ_DEFAULT_MIXER;
  }
  return P->mixer_id = n;
}
