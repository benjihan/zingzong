/**
 * @file   zz_log.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-30
 * @brief  Convenience defines that should be private.
 */

#ifndef ZZ_DEF_H
#define ZZ_DEF_H

/**
 * Messages logging.
 * @{
 */

#ifndef SPR_MIN
# define SPR_MIN 4000                   /* sampling rate minimum */
#endif

#ifndef SPR_MAX
# define SPR_MAX 96000                  /* sampling rate maximum */
#endif

#ifndef SPR_DEF
# define SPR_DEF 48000                  /* sampling rate default */
#endif

#define RATE_MIN 50
#define RATE_MAX 800
#define RATE_DEF 200

#ifndef zz_void
# define zz_void ((void)0)
#endif

#if defined DEBUG && DEBUG != 2
# define DEBUG_LOG 1
#endif

#ifdef SC68
# include "sc68_debug.h"
#elif ! defined zz_assert
# ifdef NDEBUG
#  define zz_assert(E) zz_void
# else
#  include <assert.h>
#  define zz_assert(E) assert(E)
# endif
#endif

#ifndef FMT12
# if defined(__GNUC__) || defined(__clang__)
#  define FMT12 __attribute__ ((format (printf, 1, 2)))
# else
#  define FMT12
# endif
#endif

#define LU(X) ((unsigned long)(X))     /* printf format shenanigans. */
#define HU(X) ((unsigned short)(X))    /* printf format shenanigans. */

#ifdef NO_LOG

# ifndef ZZ_PRIVATE_H
#  error NO_LOG outside zz_private ? Really ?
# endif

# define zz_log_err(FMT,...) zz_void
# define zz_log_wrn(FMT,...) zz_void
# define zz_log_inf(FMT,...) zz_void
# define zz_log_dbg(FMT,...) zz_void

#else

# ifndef ZZ_EXTERN_C
#  ifdef __cplusplus
#   define ZZ_EXTERN_C extern "C"
#  else
#   define ZZ_EXTERN_C extern
#  endif
# endif

/**
 * Message logging (zz_log.c)
 * @{
 */
ZZ_EXTERN_C FMT12 void
zz_log_err(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_wrn(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_inf(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_dbg(const char * fmt,...);
#endif /* NO_LOG */

#ifndef ZZ_ERR_PREFIX
# define ZZ_ERR_PREFIX ""
#endif
#define emsg(FMT,...) zz_log_err(ZZ_ERR_PREFIX FMT,##__VA_ARGS__)

#ifndef ZZ_WRN_PREFIX
# define ZZ_WRN_PREFIX "/!\\ "
#endif
#define wmsg(FMT,...) zz_log_wrn(ZZ_WRN_PREFIX FMT,##__VA_ARGS__)

#ifndef ZZ_INF_PREFIX
# define ZZ_INF_PREFIX ""
#endif
#define imsg(FMT,...) zz_log_inf(ZZ_INF_PREFIX FMT,##__VA_ARGS__)

#ifdef DEBUG_LOG
#ifndef ZZ_DBG_PREFIX
# define ZZ_DBG_PREFIX ""
#endif
# define dmsg(FMT,...) zz_log_dbg(ZZ_DBG_PREFIX FMT,##__VA_ARGS__)
#else
# define dmsg(FMT,...) zz_void
#endif
/**
 * @}
 */

#define FCC_EQ(A,B) (0[A]==0[B]&&1[A]==1[B]&&2[A]==2[B]&&3[A]==3[B])


/**
 * memory functions (zz_mem.c).
 */
#define zz_malloc(P,N) zz_memnew( (void * restrict) (P), (N), 0 )
#define zz_calloc(P,N) zz_memnew( (void * restrict) (P), (N), 1 )
#define zz_free(P)     zz_memdel( (void * restrict) (P)  )

ZZ_EXTERN_C
zz_err_t zz_memnew(void * restrict pmem, zz_u32_t size, zz_u8_t clear);
ZZ_EXTERN_C
void zz_memdel(void * restrict pmem);
ZZ_EXTERN_C
zz_err_t zz_memchk_calls(void);
ZZ_EXTERN_C
zz_err_t zz_memchk_block(const void *);

#ifndef zz_memcpy
ZZ_EXTERN_C
void * zz_memcpy(void * restrict _d, const void * _s, zz_u32_t n);
#endif

#ifndef zz_memset
ZZ_EXTERN_C
void * zz_memset(void * restrict _d, int v, zz_u32_t n);
#endif

#ifndef zz_memclr
ZZ_EXTERN_C
void * zz_memclr(void * restrict _d, zz_u32_t n);
#endif

#ifndef zz_memcmp
ZZ_EXTERN_C
int zz_memcmp(const void *_a, const void *_b, zz_u32_t n);
#endif

/**
 * @}
 */

/**
 * Managed strings (zz_str.c)
 * @{
 */
typedef struct str_s * str_t;
ZZ_EXTERN_C str_t zz_strnew(zz_u16_t len);
ZZ_EXTERN_C str_t zz_strset(str_t str, const char * sta);
ZZ_EXTERN_C str_t zz_strdup(str_t str);
ZZ_EXTERN_C void  zz_strdel(str_t * pstr);
ZZ_EXTERN_C zz_u16_t zz_strlen(str_t const str);
/**
 * @}
 */

/**
 * Audio output interface (out_ao.c and out_raw.c).
 */

typedef struct zz_out_s zz_out_t;       /**< output. */

/** Audio output interface. */
struct zz_out_s {
  const char * name;                    /**< friendly name.  */
  const char * uri;                     /**< uri/path. */
  zz_u32_t     hz;                      /**< sampling rate. */

  /** Close and free function. */
  zz_err_t (*close)(zz_out_t *);

  /** Write PCM data function. */
  zz_u16_t (*write)(zz_out_t *, void *, zz_u16_t);
};

ZZ_EXTERN_C
zz_out_t * out_ao_open(zz_u32_t hz, const char * uri);
ZZ_EXTERN_C
zz_out_t * out_raw_open(zz_u32_t hz, const char * uri);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * @}
 */

#endif /* ZZ_DEF_H */
