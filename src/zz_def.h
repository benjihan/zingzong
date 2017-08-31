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

#ifndef zz_assert
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

ZZ_EXTERN_C FMT12 void
zz_log_err(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_wrn(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_inf(const char * fmt,...);
ZZ_EXTERN_C FMT12 void
zz_log_dbg(const char * fmt,...);
#endif /* NO_LOG */

#define emsg(FMT,...) zz_log_err(FMT,##__VA_ARGS__)
#define wmsg(FMT,...) zz_log_wrn(FMT,##__VA_ARGS__)
#define imsg(FMT,...) zz_log_inf(FMT,##__VA_ARGS__)
#ifdef DEBUG_LOG
# define dmsg(FMT,...) zz_log_dbg(FMT,##__VA_ARGS__)
#else
# define dmsg(FMT,...) zz_void
#endif

typedef struct zz_out_s zz_out_t;       /**< output. */

/** Audio output interface. */
struct zz_out_s {
  const char * name;                    /**< friendly name.  */
  const char * uri;                     /**< uri/path. */
  zz_u32_t     hz;                      /**< sampling rate. */

  /**
     Close and free function.
  */
  zz_err_t (*close)(zz_out_t *);

  /**
     write PCM data function.
  */
  zz_u16_t (*write)(zz_out_t *, void *, zz_u16_t);
};

/**
 * Output functions
 * @{
 */
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
