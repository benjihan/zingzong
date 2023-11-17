/**
 * @file   zz_m68k.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  m68k special.
 */

#include "../zz_private.h"
#include "zz_m68k.h"

#define AMIGA_DETECTION 0

#ifndef mixer_STE
#define mixer_STE mixer_ste_hub
#endif

void bin_free(bin_t ** pbin) {}

ZZ_STATIC mixer_t mixer;
ZZ_STATIC uint32_t _SND;		/* '_SND' cookie value */

extern uint32_t * volatile _p_cookies[];

/* _SND cookie bits */
enum {
  SND_YM2149	= 1,
  SND_8BIT_DMA	= 2,
  SND_16BIT_DMA = 4,
  SND_DSP_56K	= 8
};

#if AMIGA_DETECTION

/* Amiga DMACON register */
#define DMACON (*(volatile uint16_t *)0xDFF096);

static void __attribute__((interrupt)) bus_error(void)
{
  /* see guess_hardware():
   * - Restore SP
   * - Push PC
   * - Push SR
   * interrupt will exit with RTE restoring SR and PC
   */
  asm volatile (
    "  move.w %d0,%sr  \n"
    "  move.l %d1,%a7  \n"
    "  jmp    (%a0)    \n\t"
    );
}

#endif

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

/* Define symbol for TOS cookie jar */
asm (
  ".global _p_cookie" "\n"
  ".set _p_cookie,0x5A0" "\n"
  );

static
uint8_t guess_hardware(void)
{
  uint8_t id = MIXER_LAST;
  uint8_t aga = 0;

#if AMIGA_DETECTION
  volatile intptr_t bus_vector/* , adr_vector */;

  /* Save vectors & Install handlers*/
  bus_vector = 2[(volatile uint32_t *)0];
  2[(volatile uint32_t *)0] = (intptr_t) bus_error;

  /* GB: Totally hacked !!!
   * - CLear flag
   * - Save SR,SP,PC in d0,d1,a0
   * - Access Amiga Hardware -> BUS error or not
   * - Set flag (only if no BUS error)
   *
   * GB: Does not work with all Atari machine configuration. Sometime
   *     it's possible for $dff096 to be in valid memory range
   *     (e.g. Falcon 14MB)
   */
  asm volatile (
    "  move.w %%sr,%%d0  \n"
    "  move.l %%a7,%%d1  \n"
    "  lea    1f,%%a0    \n"
    "  clr.w  0xDFF096   \n"
    "  st.b   %[flag]    \n"
    "1:                  \n\t"
    : [flag] "=g" (aga)
    :
    : "d0","d1","a0","cc");

  /* Restore vectors */
  2[(volatile uint32_t *)0] = bus_vector;
#endif

  if (aga)
    id = MIXER_AGA;
  else {
    uint8_t snd = SND_YM2149;
    uint32_t * cookie;
    _SND = 0;
    /* gB: Prevents gcc-13.2.0 to wrongly warn about array bounds and
     *     force absw address mode.
     */
    asm volatile (
      "\n\t" "move.l 0x5A0.w,%[res]"
      : [res] "=a" (cookie)
      );
    if (cookie) {
      for ( ; cookie[0]; cookie += 2 ) {
	if ( cookie[0] == 0x5f534e44 /* '_SND' */) {
	  _SND = snd = cookie[1];
	  break;
	}
      }
    }
    /* gB: Ignoring DSP for now.  May be later. */
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
ZZ_EXTERN_C mixer_t * mixer_fal(mixer_t * const M);

ZZ_EXTERN_C mixer_t * mixer_ste_hub(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste_dnr(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste_lrb(mixer_t * const M);
ZZ_EXTERN_C mixer_t * mixer_ste_s7s(mixer_t * const M);

static mixer_t * mixer_of(zz_u8_t n, mixer_t * M)
{
  if (!M) M = &mixer;
  switch (n) {

  case MIXER_AGA: M = mixer_aga(M); break;
  case MIXER_STF: M = mixer_stf(M); break;
  case MIXER_STE: M = mixer_STE(M); break;
  case MIXER_FAL: M = mixer_fal(M); break;
  default: M = 0;
  }
  return M;
}

mixer_t * zz_mixer_get(zz_u8_t * const pn)
{
  zz_u8_t n = (uint8_t) *pn;
  if (n == ZZ_MIXER_DEF)
    n = guess_hardware();
  if (n >= MIXER_LAST)
    n = ZZ_MIXER_DEF;
  *pn = n;
  return mixer_of(n,0);
}

zz_u8_t zz_mixer_enum(zz_u8_t n, const char ** pname, const char ** pdesc)
{
  const mixer_t * M;
  if (M = zz_mixer_get(&n), M) {
    *pname = M->name;
    *pdesc = M->desc;
  }
  return n;
}

#ifndef zz_memclr
void * zz_memclr(void * restrict _d, zz_u32_t n)
{
  return zz_memset(_d,0,n);
}
#endif

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
