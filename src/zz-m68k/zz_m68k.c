/**
 * @file   zz_m68k.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  m68k special.
 */

#include "../zz_private.h"
#include "zz_m68k.h"

void bin_free(bin_t ** pbin) {}

static mixer_t mixer;

/* TOS cookie jar */
#define COOKIEJAR (* (uint32_t **) 0x5A0)

/* _SND cookie bits */
#define SND_YM2149    1
#define SND_8BIT_DMA  2
#define SND_16BIT_DMA 4
#define SND_DSP_56K   8

/* Amiga DMACON register */
#define DMACON (*(volatile uint16_t *)0xDFF096)


static void __attribute__((interrupt)) bus_error(void)
{
  /* address register must match the one in guess_hardware() */
  asm volatile (
    "addq.w #8,%a7     \n\t"
    "move.l %a4,2(%a7) \n\t"
    );
}

/* #pragma GCC diagnostic ignored "-Wmultichar" */

#if defined NO_LIBC && ! defined NDEBUG
/* Because we use isalpha() in debug message only. */
int isalpha(int c)
{
  return c >= 'A' && c <= 'z' && ( c <= 'Z' || c >= 'a' );
}
#endif


/* The guess_hardware() function determines the kind of machine the
 * program is running on. It currently supports Amiga and Atari ST
 * machine. The method is to firstly try to write at Amiga/Paula IO
 * address space after trapping the bus error exceptions. If it's not
 * an Amiga machine we assume it's an Atari ST. The default is to
 * assume a good old Atari ST without any sound DMA. Then we use the
 * _SND cookie to determine which hardware is supported.
 */

static
uint8_t guess_hardware(void)
{
  uint8_t id = MIXER_LAST, aga;
  volatile intptr_t bus_vector/* , adr_vector */;

  /* asm volatile("illegal\n\t"); */

  /* Save vectors */
  bus_vector = 2[(volatile uint32_t *)0];
  /* adr_vector = 3[(volatile uint32_t *)0]; */

  /* Install handlers */
  2[(volatile uint32_t *)0] = (intptr_t) bus_error;
  /* 3[(volatile uint32_t *)0] = (intptr_t) address_error; */

  /* GB: Totally hacked !!! */
  asm volatile (
    "sf.b  %[flag]   \n\t"
    "lea   1f,%%a4   \n\t"
    "sf.b  0xDFF096  \n\t"
    "st.b  %[flag]   \n"
    "1:              \n\t"
    : [flag] "=g" (aga)
    :
    : "a4","cc" );

  /* Restore vectors */
  2[(volatile uint32_t *)0] = bus_vector;
  /* 3[(volatile uint32_t *)0] = adr_vector; */


  if (aga)
    id = MIXER_AGA;
  else {
    uint32_t * jar = COOKIEJAR, cookie;
    uint8_t snd = SND_YM2149;

    while (cookie = *jar++, cookie) {
      uint32_t value = *jar++;
      if (cookie == FCC('_','S','N','D')/* '_SND' */) {
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

/* GB: /!\ WARNING UGLY HACK /!\
 *
 * It's using a unique static mixer meaning that enumerating the
 * mixers actually change the mixer.
 */

ZZ_EXTERN_C mixer_t * mixer_aga(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_stf(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_fal(mixer_t * const M);

static mixer_t * mixer_of(zz_u8_t n)
{
  mixer_t * M;
  switch (n) {
  case MIXER_AGA: M = mixer_aga(&mixer); break;
  case MIXER_STF: M = mixer_stf(&mixer); break;
  case MIXER_STE: M = mixer_ste(&mixer); break;
  case MIXER_FAL: M = mixer_fal(&mixer); break;
  default: M = 0;
  }
  return M;
}

zz_u8_t zz_mixer_set(play_t * P, zz_u8_t n)
{
  mixer_t * mixer;

  if (n >= MIXER_LAST)
    n = guess_hardware();

  mixer = mixer_of(n);
  if (mixer)
    P->mixer = mixer;
  else
    n = ZZ_MIXER_DEF;
  return P->mixer_id = n;
}

static mixer_t * get_mixer(zz_u8_t * const pn)
{
  u8_t n = *pn;
  if (n == ZZ_MIXER_DEF)
    n = guess_hardware();
  if (n >= MIXER_LAST)
    n = ZZ_MIXER_DEF;
  *pn = n;
  return mixer_of(n);
}

zz_u8_t zz_mixer_enum(zz_u8_t n, const char ** pname, const char ** pdesc)
{
  const mixer_t * M;

  if (M = get_mixer(&n), M) {
    *pname = M->name;
    *pdesc = M->desc;
  }
  return n;
}

#ifndef NO_LIBC

/* GB: This is referenced by malloc() family functions. Normally we
 *     should not call malloc() but as we currently are we need this
 *     to be defined.
 */

int32_t sbrk() {
  asm volatile("illegal \n\t");
  return 0;
}

#endif
