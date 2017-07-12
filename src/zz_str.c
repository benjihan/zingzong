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
  str->s = (char *)set;
  str->l = len;
  str->n = len;
  return str;
}

str_t *
zz_stralloc(unsigned int size)
{
  str_t * str = 0;
  const uint_t nalloc = size + (intptr_t)str->b;

  str = zz_alloc("string", nalloc, 0);
  if (str) {
    str->s = str->b;
    str->n = size;
    str->l = 0;                       /* 0: undef */
    str->b[0] = 0;
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
    memcpy(str->b,org,len);
    str->l = len;
  }
  return str;
}

void
zz_strfree(str_t ** pstr)
{
  assert(pstr);
  if (pstr) {
    str_t * const str = *pstr;
    if (str && str->s == str->b)
      free(str);
    *pstr = 0;
  }
}
