/**
 * @file   zz_str.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  memory and strings.
 */

#include "zz_private.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>


#ifdef NO_LIBC

void (*zz_free_func)(void ** pptr);
void * (*zz_alloc_func)(const uint_t size, int clear);

#else

void
zz_free_real(void ** pptr)
{
  if (*pptr) {
    free(*pptr);
    *pptr = 0;
  }
}

void *
zz_alloc_real(const uint_t size, const int clear)
{
  void * ptr = clear ? calloc(1,size) : malloc(size);
  return ptr;
}

#endif

str_t *
zz_strset(str_t * str, const char * set)
{
  const uint_t len = strlen(set)+1;
  if (!str)
    str = zz_strdup(set);
  else {
    str->_s = (char *)set;
    str->_l = len;
    str->_n = len;
  }
  return str;
}

str_t *
zz_stralloc(unsigned int size)
{
  str_t * str;
  const uint_t nalloc = size + (intptr_t)(((str_t*)0)->_b);

  str = zz_malloc("string", nalloc);
  if (str) {
    str->_s = str->_b;
    str->_n = size;
    str->_l = 0;                        /* 0: undef */
    *str->_b = 0;
  }
  return str;
}

str_t *
zz_strdup(const char * org)
{
  str_t * str;
  const uint_t len = strlen(org) + 1;

  str = zz_stralloc(len);
  if (str) {
    zz_memcpy(str->_b,org,len);
    str->_l = len;
  }
  return str;
}

void
zz_strfree(str_t ** pstr)
{
  str_t * const str = *pstr;
  if (str) {
    void * const s = str->_s;
    zz_memclr(str,sizeof(*str));
    if (s == str->_b)
      free(str);
    *pstr = 0;
  }
}

int
zz_strlen(str_t * str)
{
  if (!str->_l)
    str->_l = strnlen(str->_s, str->_n)+1;
  return str->_l-1;
}
