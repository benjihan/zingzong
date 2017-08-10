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

void
zz_free(const char * obj, void * pptr)
{
  void ** const p = (void **) pptr;
  if (*p) {
    errno = 0;
    free(*p);
    *p = 0;
    if (errno)
      sysmsg(obj,"free");
  }
}

void *
zz_alloc(const char * obj, const uint_t size, const int clear)
{
  void * ptr = clear ? calloc(1,size) : malloc(size);
  if (!ptr)
    sysmsg(obj,"alloc");
  return ptr;
}

void *
zz_calloc(const char * obj, const uint_t size)
{
  return zz_alloc(obj, size, 1);
}

void *
zz_malloc(const char * obj, const uint_t size)
{
  return zz_alloc(obj, size, 0);
}

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

  str = zz_alloc("string", nalloc, 0);
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
