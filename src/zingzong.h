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
  E_OK, E_ERR, E_ARG, E_SYS, E_INP, E_OUT, E_SET, E_SNG, E_PLA, E_MIX,
  E_666 = 66
};

/**
 * Known (but not always supported) Quartet file format.
 */
enum zz_format_e {
  ZZ_FORMAT_UNKNOWN,            /**< Not yet determined.            */
  ZZ_FORMAT_4V,                 /**< Original Atari ST song.        */
  ZZ_FORMAT_4Q,                 /**< Single song bundle (MUG UK ?). */
  ZZ_FORMAT_QUAR,               /**< Multi song bundle (sc68).      */
};

enum {
  ZZ_DEFAULT_MIXER = -1         /**< Default mixer id.              */
};

typedef struct vfs_s  * restrict zz_vfs_t;
typedef struct vset_s * restrict zz_vset_t;
typedef struct song_s * restrict zz_song_t;
typedef struct play_s * restrict zz_play_t;
typedef const struct zz_vfs_dri_s * zz_vfs_dri_t;
typedef int (*zz_guess_t)(zz_play_t const, const char *);
typedef int8_t zz_err_t;

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

typedef void (*zz_log_t)(int,void *,const char *,va_list);

/**
 * Set Zingzong log function.
 *
 * @param func  pointer to the new log function (0: to disable all).
 * @param user  pointer user private data (parameter #2 of func).
 */

ZINGZONG_API( void , zz_log )
        (zz_log_t func, void * user)
        ;


/**
 * Get zingzong version string.
 * @retval "zingzong MAJOR.MINOR.PATCH.TWEAK"
 */
ZINGZONG_API( const char * , zz_version )
        (void)
        ;

ZINGZONG_API( zz_err_t , zz_new )
        (zz_play_t * play)
        ;
ZINGZONG_API( void , zz_del )
        (zz_play_t * play)
        ;
ZINGZONG_API( zz_err_t , zz_load )
        (zz_play_t const play,const char * song, const char * vset)
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
        (zz_play_t P)
        ;
ZINGZONG_API( zz_err_t , zz_tick )
        (zz_play_t play)
        ;
ZINGZONG_API( zz_err_t , zz_push )
        (zz_play_t play, int8_t * done)
        ;
ZINGZONG_API( int16_t * , zz_pull )
        (zz_play_t play, int * ptr_n)
        ;
ZINGZONG_API( unsigned int , zz_position )
        (zz_play_t P, unsigned int * ptick)
        ;

ZINGZONG_API( int , zz_mixer_enum )
        (int id, const char **pname, const char **pdesc)
        ;
ZINGZONG_API( int , zz_mixer_set)
        (zz_play_t P, int id)
        ;

#ifdef ZZ_VFS_DRI

enum {
  ZZ_SEEK_SET, ZZ_SEEK_CUR, ZZ_SEEK_END
};

struct zz_vfs_dri_s {
  const char * name;                      /**< friendly name.     */
  int (*reg)(zz_vfs_dri_t);               /**< register driver.   */
  int (*unreg)(zz_vfs_dri_t);             /**< unregister driver. */
  int (*ismine)(const char *);            /**< is mine.           */
  zz_vfs_t (*new)(const char *, va_list); /**< create VFS.        */
  void (*del)(zz_vfs_t);                  /**< destroy VFS.       */
  const char * (*uri)(zz_vfs_t);          /**< get URI.           */
  int (*open)(zz_vfs_t);                  /**< open.              */
  int (*close)(zz_vfs_t);                 /**< close.             */
  int (*read)(zz_vfs_t, void *, int);     /**< read.              */
  int (*tell)(zz_vfs_t);                  /**< get position.      */
  int (*size)(zz_vfs_t);                  /**< get size.          */
  int (*seek)(zz_vfs_t,int,int);          /**< offset,whence.     */
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


#endif /* ZZ_VFS_DRI */

#endif /* #ifndef ZINGZONG_H */
