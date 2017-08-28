
#include "../zz_private.h"

void bin_free(bin_t ** pbin) {}
void zz_strfree(str_t ** pstr) {}
void zz_free_real(void ** pptr) {}
void *zz_alloc_real(const uint_t size, const int clear)
{
  return (void *)(intptr_t) 0x1337;     /* any value but 0 */;
}

/* static void * */
/* m68k_alloc(const uint_t size, int clear) */
/* { */
/*   return (void *)(intptr_t) 0x1337;     /\* any value but 0 *\/; */
/* } */
/* static void m68k_free(void ** pptr) { } */
/* void (*zz_free_func)(void **) = m68k_free; */
/* void * (*zz_alloc_func)(const uint_t, int) = m68k_alloc; */

void zz_memcpy(void * restrict _d, const void * _s, int n)
{
  uint8_t * d = _d; const uint8_t * s = _s;
  while (n--) *d++ = *s++;
}

void zz_memclr(void * restrict _d, int n)
{
  uint8_t * restrict d = _d;
  while (n--) *d++ = 0;
}

int zz_memcmp(const void *_a, const void *_b, int n)
{
  int8_t c = 0;
  const uint8_t *a = _a, *b = _b;
  if (n) do {
      c = *a++ - *b++;
    } while (!c && --n);
  return c;
}
