
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
  /* register must match the asm code in guess_hardware() */
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

    /* ignoring DSP for now, hopefully we'll add a nice DSP nixer later */
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
  return n;
}

/* void zz_strfree(str_t ** pstr) {} */
/* void zz_free_real(void ** pptr) {} */
/* void *zz_alloc_real(const uint_t size, const int clear) */
/* { */
/*   return (void *)(intptr_t) 0x1337;     /\* any value but 0 *\/; */
/* } */

/* static void * */
/* m68k_alloc(const uint_t size, int clear) */
/* { */
/*   return (void *)(intptr_t) 0x1337;     /\* any value but 0 *\/; */
/* } */
/* static void m68k_free(void ** pptr) { } */
/* void (*zz_free_func)(void **) = m68k_free; */
/* void * (*zz_alloc_func)(const uint_t, int) = m68k_alloc; */

/* void * zz_memcpy(void * restrict _d, const void * _s, int n) */
/* { */
/*   uint8_t * d = _d; const uint8_t * s = _s; */
/*   while (n--) *d++ = *s++; */
/*   return _d; */
/* } */

/* void zz_memclr(void * restrict _d, int n) */
/* { */
/*   uint8_t * restrict d = _d; */
/*   while (n--) *d++ = 0; */
/* } */

/* int zz_memcmp(const void *_a, const void *_b, int n) */
/* { */
/*   int8_t c = 0; */
/*   const uint8_t *a = _a, *b = _b; */
/*   if (n) do { */
/*       c = *a++ - *b++; */
/*     } while (!c && --n); */
/*   return c; */
/* } */
