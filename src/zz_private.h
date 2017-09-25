/**
 * @file   zz_private.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  zingzong private definitions.
 */

#ifdef ZZ_PRIVATE_H
# error Should be included only once
#endif
#define ZZ_PRIVATE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# define _DEFAULT_SOURCE
# define _GNU_SOURCE                    /* for GNU basename() */
# define _BSD_SOURCE
#endif

#if defined __m68k__ && __SIZEOF_INT__ != 2
# error __SIZEOF_INT__ != 2
#endif

#ifdef ZZ_MINIMAL
# define NO_FLOAT_SUPPORT
# define NO_LOG
# define NO_LIBC
# define NO_VFS
#endif

#define ZZ_VFS_DRI
#include "zingzong.h"

/* Old error code names have been changed to avoid name conflict
 * likeliness in the public space. They are still in use in zingzong
 * internal code. */
#define E_OK  ZZ_OK
#define E_ERR ZZ_ERR
#define E_666 ZZ_666
#define E_ARG ZZ_EARG
#define E_SYS ZZ_ESYS
#define E_MEM ZZ_ESYS
#define E_INP ZZ_EINP
#define E_OUT ZZ_EOUT
#define E_SET ZZ_ESET
#define E_SNG ZZ_ESNG
#define E_PLA ZZ_EPLA
#define E_MIX ZZ_EMIX

#if !defined DEBUG && !defined NDEBUG
# define NDEBUG 1
#endif

#ifndef NO_LIBC

# include <stdio.h>
# include <ctype.h>
# include <string.h>                     /* memset, basename */

#ifdef __MINGW32__
# include <libgen.h>     /* no GNU version of basename() with mingw */
#endif

# include <errno.h>
# define ZZ_EIO    EIO
# define ZZ_EINVAL EINVAL
# define ZZ_ENODEV ENODEV

#else

# define ZZ_EIO    5
# define ZZ_EINVAL 22
# define ZZ_ENODEV 19

#endif

/* because this is not really part of the API but we need similar
 * functions in the program that use the public API and I am too lazy
 * to duplicate the code. */

#ifndef likely
# if defined(__GNUC__) || defined(__clang__)
#  define likely(x)   __builtin_expect(!!(x),1)
#  define unlikely(x) __builtin_expect((x),0)
# else
#  define likely(x)   (x)
#  define unlikely(x) (x)
# endif
#endif

#ifndef unreachable
# if defined(__GNUC__) || defined(__clang__)
#  define unreachable() __builtin_unreachable()
# else
#  define unreachable() zz_void
# endif
#endif

#ifndef always_inline
# if defined __GNUC__ || defined __clang__
#  define always_inline __attribute__((always_inline))
# elif defined _MSC_VER
#  define always_inline __forceinline
# else
#  define always_inline inline
# endif
#endif

#ifndef never_inline
# if defined(__GNUC__) || defined(__clang__)
#  define never_inline  __attribute__((noinline))
# elif defined _MSC_VER
#  define never_inline __declspec(noinline)
# else
#  define never_inline
# endif
#endif

#ifndef FP
# define FP 15u                         /* mixer step precision */
#endif

#define MIXBLK        16u               /*  */
#define VSET_UNROLL   1024u             /* Over estimated */
#define VSET_XSIZE    (20u*VSET_UNROLL) /* Additional space for loops  */
#define VSET_MAX_SIZE (1<<19) /* arbitrary .set max size */
#define SONG_MAX_SIZE 0xFFF0  /* arbitrary .4v max size  */
#define INFO_MAX_SIZE 2048    /* arbitrary .4q info max size */
#define MAX_LOOP      67      /* max loop depth (singsong.prg) */

#define SEQ_STP_MIN   0x04C1B
#define SEQ_STP_MAX   0x50A28

enum {
  TRIG_NOP, TRIG_NOTE, TRIG_SLIDE, TRIG_STOP
};

/* ----------------------------------------------------------------------
 *  Types definitions
 * ----------------------------------------------------------------------
 */

/* Don't need for the exact size but at least this size. */
typedef uint_fast64_t  u64_t;
typedef int_fast64_t   i64_t;
typedef zz_u32_t       u32_t;
typedef zz_i32_t       i32_t;
typedef zz_u16_t       u16_t;
typedef zz_i16_t       i16_t;
typedef zz_u8_t        u8_t;
typedef zz_i8_t        i8_t;

typedef struct bin_s   bin_t;     /**< binary data container. */
typedef struct q4_s    q4_t;      /**< 4q header */
typedef struct info_s  info_t;    /**< song info. */
typedef struct vset_s  vset_t;    /**< voice set (.set file). */
typedef struct inst_s  inst_t;    /**< instrument. */
typedef struct song_s  song_t;    /**< song (.4v file). */
typedef struct sequ_s  sequ_t;    /**< sequence definition. */
typedef struct play_s  play_t;    /**< player. */
typedef struct chan_s  chan_t;    /**< one channel. */
typedef struct note_s  note_t;    /**< channel step (pitch) info. */
typedef struct mixer_s mixer_t;   /**< channel mixer. */

typedef struct vfs_s * vfs_t;
typedef struct zz_vfs_dri_s vfs_dri_t;

struct str_s {
  /**
   * Pointer to the actual string.
   * @notice  ALWAYS FIRST
   */
  char * ptr;                /**< buf: dynamic else: static. */
#ifdef WITH_ZSTR_FCC
  char fcc[4];               /**< "ZSTR". */
#endif
  zz_u16_t ref;              /**< number of reference. */
  zz_u16_t max;              /**< 0: const static else buffer size. */
  zz_u16_t len;              /**< 0: ndef else len+1. */
  /**
   * buffer when dynamic.
   * @notice  ALWAYS LAST
   */
  char buf[4];
};

struct bin_s {
  uint8_t *ptr;                      /**< pointer to data (_buf).   */
  u32_t    max;                      /**< maximum allocated string. */
  u32_t    len;                      /**< length including.         */
  uint8_t _buf[1];                   /**< buffer (always last).     */
};

/** Channels re-sampler and mixer interface. */
struct mixer_s {
  const char * name;                 /**< friendly name and method. */
  const char * desc;                 /**< mixer brief description.  */
  zz_err_t (*init)(play_t * const);  /**< init mixer function.      */
  void     (*free)(play_t * const);  /**< release mixer function.   */
  zz_err_t (*push)(play_t * const);  /**< push PCM function.        */
};

/** Prepared instrument (sample). */
struct inst_s {
  u32_t     len;             /**< size in bytes.                    */
  u32_t     lpl;             /**< loop length in bytes.             */
  u32_t     end;             /**< unrolled end.                     */
  uint8_t * pcm;             /**< sample address.                   */
};

/** Prepared instrument set. */
struct vset_s {
  bin_t *bin;                /**< voiceset data container.          */
  u8_t   khz;                /**< sampling rate from .set.          */
  u8_t   nbi;                /**< number of instrument [1..20].     */
  u32_t  iused;              /**< mask of instrument really used.   */
  inst_t inst[20];           /**< instrument definitions.           */
};

/** File/Memory sequence. */
struct sequ_s {
  uint8_t cmd[2],len[2],stp[4],par[4];
};

struct memb_s {
  bin_t  *bin;               /**< data container.                   */
};

/** Prepared song. */
struct song_s {
  bin_t  *bin;               /**< song data container.              */
  u16_t   rate;              /**< tick rate (0: unspecified).       */
  u8_t    khz;               /**< header sampling rate (kHz).       */
  u8_t    barm;              /**< header bar measure.               */
  u8_t    tempo;             /**< header tempo.                     */
  u8_t    sigm;              /**< header signature numerator.       */
  u8_t    sigd;              /**< header signature denominator.     */

  u32_t   iused;             /**< mask of instrument really used    */
  u32_t   stepmin;           /**< estimated minimal note been used. */
  u32_t   stepmax;           /**< estimated maximal note been used. */
  sequ_t *seq[4];            /**< pointers to channel sequences.    */
};

/** Song meta info. */
struct info_s {
  bin_t *bin;                /**< info data container.              */
  char  *album;              /**< decoded album.                    */
  char  *title;              /**< decoded title.                    */
  char  *artist;             /**< decoded artist.                   */
  char  *ripper;             /**< decoded ripper.                   */
};

/** Played note. */
struct note_s {
  i32_t   cur;               /**< current note.                     */
  i32_t   aim;               /**< current note slide goal.          */
  i32_t   stp;               /**< note slide speed (step).          */
  inst_t *ins;               /**< Current instrument.               */
};

/** Played channel. */
struct chan_s {
  sequ_t  *seq;                       /**< sequence address.        */
  sequ_t  *cur;                       /**< next sequence.           */
  sequ_t  *end;                       /**< last sequence.           */
  sequ_t  *sq0;                       /**< First wait-able command. */
  sequ_t  *sqN;                       /**< Last  wait-able command. */

  char id;                          /**< letter ['A'..'D'].         */
  u8_t num;                         /**< channel number [0..3].     */
  u8_t trig;                        /**< see TRIG_* enum.           */
  u8_t curi;                        /**< current instrument number. */

  u16_t has_loop;                 /**< has loop (counter).          */
  u16_t wait;                     /**< number of tick left to wait. */

  note_t note;                          /**< note (and slide) info. */

  struct loop_s {
    u16_t off;                       /**< loop point. */
    u16_t cnt;                       /**< loop count. */
  }
  *loop_sp,                             /**< loop stack pointer.    */
  loops[MAX_LOOP];                      /**< loop stack.   */

};

/** .4q file. */
struct q4_s {
  song_t * song;
  vset_t * vset;
  info_t * info;

  u32_t songsz;
  u32_t vsetsz;
  u32_t infosz;
};

#include "zz_def.h"

struct play_s {
  volatile u8_t st_idx;
  struct str_s st_strings[4];

  str_t songuri;
  str_t vseturi;
  str_t infouri;

  song_t song;
  vset_t vset;
  info_t info;

  u32_t tick;                          /**< current tick */
  u32_t max_ticks;                     /**< maximum tick to play ( */
  u32_t spr;                           /**< sampling rate (hz) */

  int16_t * mix_buf;                    /**< mix buffer [pcm_per_tick] */
  int16_t * mix_ptr;                    /**< current pull pointer */

  u16_t pcm_per_tick;                /**< pcm per tick */
  u16_t rate;               /**< player rate (usually 200hz) */
  u8_t  muted_voices;       /**< channels mask */
  u8_t  has_loop;           /**< channels mask */
  u8_t  end_detect;         /**< true if trying to auto detect end */
  u8_t  done;               /**< true when done (GB: bits to be defined) */
  u8_t  format;             /**< see ZZ_FORMAT_ enum */

  u8_t      mixer_id;        /**< Mixer identifier.                 */
  mixer_t * mixer;           /**< Mixer to use (almost never null). */
  void    * mixer_data;      /**< Mixer private data (never null).  */

  chan_t chan[4];                       /**< 4 channels info. */
};

typedef struct songhd songhd_t;
struct songhd {
  uint8_t rate[2];
  uint8_t measure[2];
  uint8_t tempo[2];
  uint8_t timesig[2];
  uint8_t reserved[2*4];
};


/* ---------------------------------------------------------------------- */

/**
 * Machine specific (byte order and fast mul/div.
 * @{
 */

#ifndef __ORDER_BIG_ENDIAN__
# define __ORDER_BIG_ENDIAN__ 4321
#endif

#ifdef __m68k__

# include "m68k_muldiv.h"
# ifndef __BYTE_ORDER__
#  define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
# endif

#else /* __m68k__ */

static inline u32_t always_inline c_mulu(u16_t a, u16_t b)
{ return a * b; }
static inline uint32_t always_inline c_mulu32(uint32_t a, uint16_t b)
{ return a * b; }
static inline u32_t always_inline c_divu(u32_t n, u16_t d)
{ return n / d; }
static inline u32_t always_inline c_modu(u32_t n, u16_t d)
{ return n % d; }
static inline u32_t always_inline c_divu32(u32_t n, u16_t d)
{ return n / d; }
# define mulu(a,b)   c_mulu((a),(b))
# define divu(a,b)   c_divu((a),(b))
# define modu(a,b)   c_modu((a),(b))
# define mulu32(a,b) c_mulu32((a),(b))
# define divu32(a,b) c_divu32((a),(b))

#endif /* __m68k__ */

/* GB: could probably use httons(). */
#ifndef U16
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline u16_t host_u16(const uint8_t * const v)
{ return *(const uint16_t *)v; }
#  define U16(v) host_u16((v))
# else
static inline u16_t byte_u16(const uint8_t * const v) {
  return ((u16_t)v[0]<<8) | v[1];
}
#  define U16(v) byte_u16((v))
# endif
#endif

/* GB: could probably use httonl(). */
#ifndef U32
# if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static inline u32_t host_u32(const uint8_t * const v)
{ return *(const uint32_t *)v; }
#  define U32(v) host_u32((v))
# else
static inline u32_t byte_u32(const u8_t * const v)
{ return ((u32_t)U16(v)<<16) | U16(v+2); }
#  define U32(v) byte_u32((v))
# endif
#endif

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Floating point conversion.
 * @{
 */
#ifndef NO_FLOAT_SUPPORT

ZZ_EXTERN_C
void i8tofl(float * const d, const uint8_t * const s, const int n);

ZZ_EXTERN_C
void fltoi16(int16_t * const d, const float * const s, const int n);

#endif /* NO_FLOAT_SUPPORT */
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Memory, string and binary container functions.
 * @{
 */
#ifndef NO_LIBC

# ifndef zz_memcmp
#  define zz_memcmp(D,S,N) memcmp((D),(S),(N))
# endif

# ifndef zz_memcpy
#  define zz_memcpy(D,S,N) memcpy((D),(S),(N))
# endif

# ifndef zz_memclr
#  define zz_memclr(D,N) memset((D),0,(N))
# endif

# ifndef zz_memset
#  define zz_memset(D,V,N) memset((D),(V),(N))
# endif

#endif /* NO_LIBC */
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Binary container.
 * @{mak
 */
ZZ_EXTERN_C
void bin_free(bin_t ** pbin);
ZZ_EXTERN_C
zz_err_t bin_alloc(bin_t ** pbin, u32_t len, u32_t xlen);
ZZ_EXTERN_C
zz_err_t bin_read(bin_t * bin, vfs_t vfs, u32_t off, u32_t len);
ZZ_EXTERN_C
zz_err_t bin_load(bin_t ** pbin, vfs_t vfs, u32_t len, u32_t xlen, u32_t max);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * VFS functions.
 * @{
 */
ZZ_EXTERN_C
zz_err_t vfs_register(const vfs_dri_t * dri);
ZZ_EXTERN_C
zz_err_t vfs_unregister(const vfs_dri_t * dri);
ZZ_EXTERN_C
const char * vfs_uri(vfs_t vfs);
ZZ_EXTERN_C
void vfs_del(vfs_t * pvfs);
ZZ_EXTERN_C
vfs_t vfs_new(const char * uri, ...);
ZZ_EXTERN_C
zz_err_t vfs_open_uri(vfs_t * pvfs, const char * uri);
ZZ_EXTERN_C
zz_err_t vfs_open(vfs_t vfs);
ZZ_EXTERN_C
zz_u32_t vfs_read(vfs_t vfs, void *b, zz_u32_t n);
ZZ_EXTERN_C
zz_err_t vfs_read_exact(vfs_t vfs, void *b, zz_u32_t n);
ZZ_EXTERN_C
zz_u32_t vfs_tell(vfs_t vfs);
ZZ_EXTERN_C
zz_u32_t vfs_size(vfs_t vfs);
ZZ_EXTERN_C
zz_err_t vfs_seek(vfs_t vfs, zz_u32_t pos, zz_u8_t set);

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * voiceset and song file loader.
 * @{
 */
ZZ_EXTERN_C
zz_err_t song_parse(song_t *song, vfs_t vfs, uint8_t *hd, u32_t size);
ZZ_EXTERN_C
zz_err_t song_load(song_t *song, const char *uri);
ZZ_EXTERN_C
zz_err_t vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, u32_t size);
ZZ_EXTERN_C
zz_err_t vset_load(vset_t *vset, const char *uri);
ZZ_EXTERN_C
zz_err_t q4_load(vfs_t vfs, q4_t *q4);

ZZ_EXTERN_C
void zz_song_wipe(song_t * song);
ZZ_EXTERN_C
void zz_vset_wipe(vset_t * vset);
ZZ_EXTERN_C
void zz_info_wipe(info_t * info);
/**
 * @}
 */
