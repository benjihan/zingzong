/**
 * @file   zingzong.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  zingzong public API.
 */

#ifndef ZINGZONG_H
#define ZINGZONG_H

#include <stdarg.h>
#include <stdint.h>

/**
 * Integer types the platform prefers matching our requirements.
 */
typedef  int_fast8_t  zz_i8_t;
typedef uint_fast8_t  zz_u8_t;
typedef  int_fast16_t zz_i16_t;
typedef uint_fast16_t zz_u16_t;
typedef  int_fast32_t zz_i32_t;
typedef uint_fast32_t zz_u32_t;

#ifndef ZINGZONG_API

# ifndef ZZ_EXTERN_C
#  ifdef __cplusplus
#   define ZZ_EXTERN_C extern "C"
#  else
#   define ZZ_EXTERN_C extern
#  endif
# endif

# if !defined ZZ_ATTRIBUT && defined DLL_EXPORT
#  if defined _WIN32 || defined WIN32 || defined __SYMBIAN32__
#   define ZZ_ATTRIBUT __declspec(dllexport)
#  elif __GNUC__ >= 4
#   define ZZ_ATTRIBUT __attribute__ ((visibility ("default"))
#  endif
# endif

# ifndef ZZ_ATTRIBUT
#  define ZZ_ATTRIBUT
# endif

# define ZINGZONG_API(TYPE,NAME) ZZ_EXTERN_C TYPE ZZ_ATTRIBUT NAME

#endif /* ZINGZONG_API */


/**
 * Zingzong error codes.
 */
enum {
  ZZ_OK,                       /**< No error.                       */
  ZZ_ERR,                      /**< Unspecified error.              */
  ZZ_EARG,                     /**< Argument error.                 */
  ZZ_ESYS,                     /**< System error (I/O, memory ...). */
  ZZ_EINP,                     /**< Problem with input.             */
  ZZ_EOUT,                     /**< Problem with output.            */
  ZZ_ESNG,                     /**< Voice set error.                */
  ZZ_ESET,                     /**< Song error.                     */
  ZZ_EPLA,                     /**< Player error.                   */
  ZZ_EMIX,                     /**< Mixer error.                    */
  ZZ_666 = 66                  /**< Internal error.                 */
};

/**
 * Known (but not always supported) Quartet file format.
 */
enum zz_format_e {
  ZZ_FORMAT_UNKNOWN,           /**< Not yet determined (must be 0)  */
  ZZ_FORMAT_4V,                /**< Original Atari ST song.         */
  ZZ_FORMAT_BUNDLE = 64,       /**< Next formats are bundles.       */
  ZZ_FORMAT_4Q,                /**< Single song bundle (MUG UK ?).  */
  ZZ_FORMAT_QUAR,              /**< Multi song bundle (SC68).       */
};

enum {
  ZZ_DEFAULT_MIXER = 255       /**< Default mixer id.               */
};

typedef zz_i8_t zz_err_t;
typedef struct vfs_s  * restrict zz_vfs_t;
typedef struct vset_s * restrict zz_vset_t;
typedef struct song_s * restrict zz_song_t;
typedef struct play_s * restrict zz_play_t;
typedef const struct zz_vfs_dri_s * zz_vfs_dri_t;
typedef zz_err_t (*zz_guess_t)(zz_play_t const, const char *);

typedef struct zz_info_s zz_info_t;

struct zz_info_s {

  /** format info. */
  struct {
    zz_u8_t      num;         /**< format (@see zz_format_e).       */
    const char * str;         /**< format string.                   */
  } fmt;


  struct {
    zz_u16_t     rate;       /**< player tick rate (200hz).         */
    zz_u32_t     ms;         /**< song duration in ms.              */
    zz_u32_t     ticks;      /**< song duration in ticks.           */
  } len;

  struct {
    zz_u32_t     spr;         /**< sampling rate.                   */
    zz_u16_t     ppt;         /**< pcm per tick.                    */
    zz_u8_t      num;         /**< mixer identifier.                */
    const char * name;        /**< mixer name or "".                */
    const char * desc;        /**< mixer description or "".         */
  } mix;                      /**< mixer related info.              */

  struct {
    const char * uri;         /**< URI or path.                     */
    zz_u32_t     khz;         /**< sampling rate reported.          */
  } set, sng;

  struct {
    const char * album;       /**< album or "".                     */
    const char * title;       /**< title or "".                     */
    const char * artist;      /**< artist or "".                    */
    const char * ripper;      /**< ripper or "".                    */
  } tag;

};

/**
 * Log level (first parameter of zz_log_t function).
 */
enum zz_log_e {
  ZZ_LOG_ERR,                           /**< Log error.   */
  ZZ_LOG_WRN,                           /**< Log warning. */
  ZZ_LOG_INF,                           /**< Log info.    */
  ZZ_LOG_DBG                            /*/< Log debug.   */
};

/**
 * Zingzong log function type (printf-like).
 */

typedef void (*zz_log_t)(zz_u8_t,void *,const char *,va_list);

/**
 * Get/Set zingzong active logging channels.
 *
 * @param  clr  bit mask of channels to disable).
 * @param  set  bit mask of channels to en able).
 * @return previous active logging channel mask.
 */
ZINGZONG_API( zz_u8_t, zz_log_bit)
        (const zz_u8_t clr, const zz_u8_t set)
        ;

/**
 * Set Zingzong log function.
 *
 * @param func  pointer to the new log function (0: to disable all).
 * @param user  pointer user private data (parameter #2 of func).
 */

ZINGZONG_API( void , zz_log_fun )
        (zz_log_t func, void * user)
        ;

/**
 * Memory allocation function types.
 */
typedef void * (*zz_new_t)(zz_u32_t);
typedef void   (*zz_del_t)(void *);

/**
 * Set Zingzong memory management function.
 *
 * @param  newf pointer to the memory allocation function.
 * @param  delf pointer to the memory free function.
 */

ZINGZONG_API( void , zz_mem )
        (zz_new_t newf, zz_del_t delf)
        ;

/**
 * Get zingzong version string.
 * @retval "zingzong MAJOR.MINOR.PATCH.TWEAK"
 */
ZINGZONG_API( const char * , zz_version )
        (void)
        ;

ZINGZONG_API( zz_err_t , zz_new )
        (zz_play_t * pplay)
        ;
ZINGZONG_API( void , zz_del )
        (zz_play_t * pplay)
        ;

ZINGZONG_API( zz_err_t , zz_setup )
        (zz_play_t play, zz_u8_t mixerid,
         zz_u32_t spr, zz_u16_t rate,
         zz_u32_t max_ticks, zz_u8_t end_detect)
        ;

ZINGZONG_API( zz_err_t , zz_info )
        (zz_play_t play, zz_info_t * pinfo)
        ;

ZINGZONG_API( zz_err_t , zz_load )
        (zz_play_t const play,
         const char * song, const char * vset,
         zz_u8_t * pfmt)
        ;
ZINGZONG_API( zz_err_t , zz_close)
        (zz_play_t const play)
        ;
ZINGZONG_API( zz_guess_t , zz_set_guess )
        (zz_guess_t)
        ;

ZINGZONG_API( zz_err_t , zz_init)
        (zz_play_t P)
        ;
ZINGZONG_API( zz_err_t , zz_measure )
        (zz_play_t P, zz_u32_t * pticks, zz_u32_t * pms)
        ;
ZINGZONG_API( zz_err_t , zz_tick )
        (zz_play_t play)
        ;
ZINGZONG_API( int16_t * , zz_play )
        (zz_play_t play, zz_u16_t * ptr_n)
        ;
ZINGZONG_API( zz_u32_t , zz_position )
        (zz_play_t P, zz_u32_t * const pms)
        ;

ZINGZONG_API( zz_u8_t , zz_mixer_enum )
        (zz_u8_t id, const char **pname, const char **pdesc)
        ;
ZINGZONG_API( zz_u8_t , zz_mixer_set)
        (zz_play_t P, zz_u8_t id)
        ;

enum {
  ZZ_SEEK_SET, ZZ_SEEK_CUR, ZZ_SEEK_END
};

#define ZZ_EOF ((zz_u32_t)-1)

struct zz_vfs_dri_s {
  const char * name;                      /**< friendly name.      */
  zz_err_t (*reg)(zz_vfs_dri_t);          /**< register driver.    */
  zz_err_t (*unreg)(zz_vfs_dri_t);        /**< un-register driver. */
  zz_u16_t (*ismine)(const char *);       /**< is mine.            */
  zz_vfs_t (*new)(const char *, va_list); /**< create VFS.         */
  void     (*del)(zz_vfs_t);              /**< destroy VFS.        */
  const
  char *   (*uri)(zz_vfs_t);                    /**< get URI.       */
  zz_err_t (*open)(zz_vfs_t);                   /**< open.          */
  zz_err_t (*close)(zz_vfs_t);                  /**< close.         */
  zz_u32_t (*read)(zz_vfs_t, void *, zz_u32_t); /**< read.          */
  zz_u32_t (*tell)(zz_vfs_t);                   /**< get position.  */
  zz_u32_t (*size)(zz_vfs_t);                   /**< get size.      */
  zz_err_t (*seek)(zz_vfs_t,zz_u32_t,zz_u8_t);  /**< offset,whence. */
};

/**
 * Common (inherited) part to all VFS instance.
 */
struct vfs_s {
  zz_vfs_dri_t dri;                     /**< pointer to the VFS driver */
  int err;                              /**< last error number. */
};

/**
 * Register a VFS driver.
 * @param  dri  VFS driver
 * @return Number of time this driver is registered
 * @retval -1 on error
 */
ZINGZONG_API( zz_err_t , zz_vfs_add )
        (zz_vfs_dri_t dri)
        ;

/**
 * Unregister a VFS driver.
 * @param  dri  VFS driver
 * @return Number of time this driver is registered
 * @retval -1 on error
 */
ZINGZONG_API( zz_err_t , zz_vfs_del )
        (zz_vfs_dri_t dri)
        ;

#endif /* #ifndef ZINGZONG_H */
