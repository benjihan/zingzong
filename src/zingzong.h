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

# define ZINGZONG_API ZZ_EXTERN_C ZZ_ATTRIBUT

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
  ZZ_ESNG,                     /**< Song error.                     */
  ZZ_ESET,                     /**< Voice set error                 */
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

/**
 * Mixer identifiers.
 */
enum {
  ZZ_MIXER_XTN = 254,          /**< External mixer.                 */
  ZZ_MIXER_DEF = 255           /**< Default mixer id.               */
};

/**
 * Sampler quality.
 */
enum zz_quality_e {
  ZZ_FQ = 1,                            /**< Fastest quality. */
  ZZ_LQ,                                /**< Low quality.     */
  ZZ_MQ,                                /**< Medium quality.  */
  ZZ_HQ                                 /**< High quality.v   */
};

typedef zz_i8_t zz_err_t;
typedef struct vfs_s  * zz_vfs_t;
typedef struct vset_s * zz_vset_t;
typedef struct song_s * zz_song_t;
typedef struct play_s * zz_play_t;
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
    zz_u16_t     rate;        /**< player tick rate (200hz).        */
    zz_u32_t     ms;          /**< song duration in ms.             */
  } len;

  struct {
    zz_u32_t     spr;         /**< sampling rate.                   */
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

ZINGZONG_API
/**
 * Get/Set zingzong active logging channels.
 *
 * @param  clr  bit mask of channels to disable).
 * @param  set  bit mask of channels to en able).
 * @return previous active logging channel mask.
 */
zz_u8_t zz_log_bit(const zz_u8_t clr, const zz_u8_t set);

ZINGZONG_API
/**
 * Set Zingzong log function.
 *
 * @param func  pointer to the new log function (0: to disable all).
 * @param user  pointer user private data (parameter #2 of func).
 */

void zz_log_fun(zz_log_t func, void * user);

/**
 * Memory allocation function types.
 */
typedef void * (*zz_new_t)(zz_u32_t);
typedef void   (*zz_del_t)(void *);

ZINGZONG_API
/**
 * Set Zingzong memory management function.
 *
 * @param  newf pointer to the memory allocation function.
 * @param  delf pointer to the memory free function.
 */
void zz_mem(zz_new_t newf, zz_del_t delf);

ZINGZONG_API
/**
 * Get zingzong version string.
 *
 * @retval "zingzong MAJOR.MINOR.PATCH.TWEAK"
 */
const char * zz_version(void);

ZINGZONG_API
/**
 * Create a new player instance.
 *
 * @param pplay pointer to player instance
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_new(zz_play_t * pplay);

ZINGZONG_API
/**
 * Delete player instance.
 * @param pplay pointer to player instance
 */
void zz_del(zz_play_t * pplay);


ZINGZONG_API
/**
 * Load quartet song and voice-set.
 *
 * @param  play  player instance
 * @param  song  song URI or path ("": skip).
 * @param  vset  voice-set URI or path (0: guess voice-set "":skip)
 * @param  pfmt  points to a variable to store file format (can be null).
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_load(zz_play_t const play,
                 const char * song, const char * vset,
                 zz_u8_t * pfmt);

ZINGZONG_API
/**
 * Close player (release allocated resources).
 *
 * @param  play  player instance
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_close(zz_play_t const play);

ZINGZONG_API
/**
 * Get player info.
 *
 * @param  play  player instance
 * @param  info  info to be fill.
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_info(zz_play_t play, zz_info_t * pinfo);

ZINGZONG_API
/**
 * Init player.
 *
 * @param  play  player instance
 * @param  mixer mixer-id
 * @param  spr   sampling rate or quality
 * @param  rate  player tick rate (0:default)
 * @param  maxms 0:infinite, (+):this number of ms , (-) max detect
 * @return error code
 * @retval ZZ_OK(0) on success
 */
zz_err_t zz_init(zz_play_t play,
                 zz_u8_t mixer, zz_u32_t spr,
                 zz_u16_t rate, zz_i32_t maxms);

ZINGZONG_API
/**
 * Play.
 *
 * @param  play  player instance
 * @param  pcm   pcm buffer (format might depend on mixer).
 * @param  n     >0: number of pcm to fill
 *                0: get number of pcm to complete the tick.
 *               <0: complete the tick but not more than -n pcm.
 *
 * @return number of pcm.
 * @retval 0 play is over
 * @retval >0 number of pcm
 * @retval <0 -error code
 */
zz_i16_t zz_play(zz_play_t play, void * pcm, zz_i16_t n);

ZINGZONG_API
/**
 * Mute and ignore voices.
 * - LSQ bits (0~3) are ignored channels.
 * - MSQ bits (4-7) are muted channels.
 *
 * @param  play  player instance
 * @param  clr   clear these bits
 * @param  set   set these bits
 * @return old bits
 */
uint8_t zz_mute(zz_play_t P, uint8_t clr, uint8_t set);

ZINGZONG_API
/**
 * Init player.
 *
 * @param  play  player instance
 * @param  ms    maximum ms for detection (0: infinite)
 * @return duration in ms
 * @retval ZZ_EOF on error
 */
zz_u32_t zz_measure(zz_play_t play, zz_u32_t ms);

ZINGZONG_API
/**
 * Get current frame/tick position (in ms).
 * @return a number of millisecond
 * @retval ZZ_EOF on error
 */
zz_u32_t zz_position(zz_play_t play);

ZINGZONG_API
/**
 * Get info mixer info.
 *
 * @param  id  mixer identifier (first is 0)
 * @param  pname  receive a pointer to the mixer name
 * @param  pdesc  receive a pointer to the mixer description
 * @return mixer-id (usually id unless id is ZZ_MIXER_DEF)
 * @retval ZZ_MIXER_DEF on error
 *
 * @notice The zz_mixer_info() function can be use to enumerate all
 *         available mixers.
 */
zz_u8_t zz_mixer_info(zz_u8_t id, const char **pname, const char **pdesc);

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

ZINGZONG_API
/**
 * Register a VFS driver.
 * @param  dri  VFS driver
 * @return Number of time this driver is registered
 * @retval -1 on error
 */
zz_err_t zz_vfs_add(zz_vfs_dri_t dri);

ZINGZONG_API
/**
 * Unregister a VFS driver.
 * @param  dri  VFS driver
 * @return Number of time this driver is registered
 * @retval -1 on error
 */
zz_err_t zz_vfs_del(zz_vfs_dri_t dri);

#endif /* #ifndef ZINGZONG_H */
