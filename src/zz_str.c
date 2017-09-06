/**
 * @file   zz_str.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  memory and strings.
 */

#define ZZ_DBG_PREFIX "(str) "
#include "zz_private.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char strfcc[4] = { 'Z','S','T','R' };

static inline zz_err_t valid_str(const str_t str)
{
  zz_assert(str);
  zz_assert(str->max);
  zz_assert(str->ref);
  zz_assert(FCC_EQ(str->fcc,strfcc));

  return str && str->ref && str->max && FCC_EQ(str->fcc,strfcc)
    ? E_OK
    : E_ARG
    ;
}

static str_t
zz_strsetup(str_t str, u16_t max, u16_t len, void * buf)
{
  zz_assert(str);
  zz_assert(max);

  str->fcc[0] = strfcc[0];  str->fcc[1] = strfcc[1];
  str->fcc[2] = strfcc[2];  str->fcc[3] = strfcc[3];
  str->ref = 1;
  str->max = max;
  str->len = len;
  if (!buf)
    0[str->ptr = str->buf] = 0;
  else
    str->ptr = buf;
  zz_assert( E_OK == valid_str(str) );
  return str;
}

str_t zz_strnew(zz_u16_t len)
{
  const u16_t xtra = (intptr_t)(  ((str_t)0)->buf );
  str_t str = 0;
  zz_assert( len );

  if (likely(!zz_mem_malloc(&str,xtra+len)))
    zz_strsetup(str, len, 0, 0);
  return str;
}

str_t zz_strset(str_t str, const char * sta)
{
  zz_assert(sta);
  if (!sta)
    str = 0;
  else {
    u16_t len;
    for (len=0; sta[len]; ++len);
    ++len;                         /* length including zero ending  */

    for ( ;; ) {
      if (!str) {
        str = zz_strnew(len + 16);    /* alloc a bi more than needed  */
        break;
      }
      if (len <= str->max)
        break;
      zz_strdel(&str);
    }

    if (str) {
      zz_assert( len <= str->max );
      zz_memcpy(str->ptr, sta, str->len = len);
    }
  }
  return str;
}

str_t zz_strdup(str_t str)
{
  zz_assert ( E_OK == valid_str(str) );
  if (str) {
    ++str->ref;
    dmsg("dup<%p> +1 (%hu) \"%s\"\n", str, HU(str->ref), str->ptr);
  }
  return str;
}

void zz_strdel(str_t * pstr)
{
  zz_assert(pstr);
  if (pstr) {
    str_t str = *pstr;
    if (!str) {
      dmsg("del<%p> abort (nil pointer)\n",str);
      return;
    }
    *pstr = 0;
    zz_assert( E_OK == valid_str(str) );
    zz_assert( str->ref );
    --str->ref;
    dmsg("del<%p> -1 (%hu) \"%s\"\n", str, HU(str->ref), str->ptr);
    if (!str->ref) {
      /* last reference */
      if (str->ptr == str->buf) {
        dmsg("del<%p>: free dynamic\n", str);
        zz_mem_free(&str);
      } else {
        dmsg("del<%p>: free static\n", str);
        zz_memclr(str,sizeof(*str));
      }
    }
  }
}

zz_u16_t zz_strlen(str_t const str)
{
  zz_assert( E_OK == valid_str(str) );
  if (!str->len) {
    while ( str->len < str->max-1 && str->ptr[str->len] ) str->len++;
    ++str->len;
  }
  return str->len-1;
}


#if 0

str_t
zz_strsta(zz_play_t const P, const char * sta)
{
  struct str_s * str;
  const u8_t max = sizeof(P->st_strings) / sizeof(*P->st_strings);

  zz_assert(P);
  zz_assert(sta);
  if (!P || !sta)
    return 0;

  zz_assert(P->st_idx <= max);

  if (P->st_idx < max) {
    str = &P->st_strings[P->st_idx++];
    str->buf[0] = 0;
    str->len    = 0;
    str->ptr    = sta;
  } else {
    u16_t len;
    wmsg("no more static strings for -- (%hu)\"%s\"\n", HU(len), sta);
    str = zz_strset(0,sta);
  }
  return str;
}

#endif
