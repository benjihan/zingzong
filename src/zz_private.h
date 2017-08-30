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
# define _GNU_SOURCE
#endif

#ifdef ZZ_MINIMAL
# define NO_FLOAT_SUPPORT
# define NO_LOG
# define NO_LIBC
# define NO_VFS
#endif

#define ZZ_VFS_DRI
#include "zingzong.h"

#ifndef zz_void
# define zz_void ((void) 0)
#endif

#if !defined DEBUG && !defined NDEBUG
# define NDEBUG 1
#endif

#ifndef zz_assert
# ifdef NDEBUG
#  define zz_assert(E) zz_void
# else
#  include <assert.h>
#  define zz_assert(E) assert(E)
# endif
#endif

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#ifndef NO_LIBC
# include <string.h>                     /* memset ... */
# include <errno.h>
# define ZZ_EIO EIO
# define ZZ_EINVAL EINVAL
# define ZZ_ENODEV ENODEV
#else
# define ZZ_EIO    5
# define ZZ_EINVAL 22
# define ZZ_ENODEV 19
#endif

#ifndef EXTERN_C
# ifdef __cplusplus
#  define EXTERN_C extern "C"
# else
#  define EXTERN_C extern
# endif
#endif

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
#  define unreachable() if (0) {} else
# endif
#endif

#ifndef always_inline
# if defined(__GNUC__) || defined(__clang__)
#  define always_inline  __attribute__((always_inline))
# else
#  define always_inline inline
# endif
#endif

#ifndef MAX_DETECT
# define MAX_DETECT 1800    /* maximum seconds for length detection */
#endif

#ifndef SPR_MIN
# define SPR_MIN 4000                   /* sampling rate minimum */
#endif

#ifndef SPR_MAX
# define SPR_MAX 96000                  /* sampling rate maximum */
#endif

#ifndef SPR_DEF
# define SPR_DEF 48000                  /* sampling rate default */
#endif

#ifndef FP
# define FP 15u                         /* mixer step precision */
#endif

#define MIXBLK        16u               /*  */
#define VSET_UNROLL   1024u             /* Over estimated */
#define VSET_XSIZE    (20u*VSET_UNROLL) /* Additional space for loops  */
#define VSET_MAX_SIZE (1<<21) /* arbitrary .set max size */
#define SONG_MAX_SIZE (1<<18) /* arbitrary .4v max size  */
#define INFO_MAX_SIZE 4096    /* arbitrary .4q info max size */
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

typedef unsigned int   uint_t;    /**< unsigned int. */
typedef unsigned short ushort_t;  /**< unsigned short. */
typedef struct str_s   str_t;     /**< strings (static or dynamic) */
typedef struct str_s   bin_t;     /**< binary data container. */
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
typedef struct out_s   out_t;     /**< output. */

typedef struct vfs_s * vfs_t;
typedef struct zz_vfs_dri_s vfs_dri_t;

struct str_s {
  void   *_s;                 /**< string pointer. */
  uint_t  _l;                 /**< length including ending zero */
  uint_t  _n;                 /**< maximum allocated string */
  int8_t  _b[1];              /**< string buffer for dynamic string */
};

struct out_s {
  const char * name;                    /**< friendly name.  */
  const char * uri;                     /**< uri/path. */
  uint_t       hz;                      /**< sampling rate. */
  int (*close)(out_t *);                /**< close and free function. */
  int (*write)(out_t *, void *, int);   /**< write PCM data function.  */
};

struct mixer_s {
  const char * name;                   /**< friendly name and method. */
  const char * desc;                   /**< mixer brief description.  */
  int   (*init)(play_t * const);       /**< init mixer function.      */
  void  (*free)(play_t * const);       /**< release mixer function.   */
  int   (*push)(play_t * const);       /**< push PCM function.        */
};

struct inst_s {
  int len;                              /**< size in byte */
  int lpl;                              /**< loop length in byte */
  int end;                              /**< unroll end */
  uint8_t * pcm;                        /**< sample address */
};

struct vset_s {
  bin_t * bin;                  /**< data container */
  int khz;                      /**< sampling rate from .set */
  int nbi;                      /**< number of instrument [1..20] */
  int iused;                    /**< mask of instrument really used */
  inst_t inst[20];              /**< instrument definition */
};

struct sequ_s {
  uint8_t cmd[2],len[2],stp[4],par[4];
};

struct song_s {
  bin_t * bin;              /**< raw data. */
  uint8_t khz;              /**< header sampling rate. */
  uint8_t barm;             /**< header bar measure. */
  uint8_t tempo;            /**< header tempo. */
  uint8_t sigm;             /**< header signature numerator. */
  uint8_t sigd;             /**< header signature denominator. */

  uint32_t iused;           /**< mask of instrument really used */
  uint32_t stepmin;         /**< estimated minimal note been used. */
  uint32_t stepmax;         /**< estimated maximal note been used. */
  sequ_t *seq[4];           /**< pointers to channel sequences. */
};

struct info_s {
  bin_t * bin;
  char  * comment;
  char  * title;
  char  * artist;
};

struct note_s {
  int cur, aim, stp;
  inst_t * ins;
};

struct chan_s {
  sequ_t  *seq;                         /* sequence address */
  sequ_t  *cur;                         /* next sequence */
  sequ_t  *end;                         /* last sequence */
  sequ_t  *sq0;                         /* First wait-able command */
  sequ_t  *sqN;                         /* Last  wait-able command */

  int8_t  id;                           /* letter ['A'..'D'] */
  int8_t  num;                          /* channel number [0..3] */
  int8_t  trig;                         /* see TRIG_* enum */
  int8_t  curi;                         /* current instrument number */

  uint16_t has_loop;                 /* has loop (counter) */
  uint16_t wait;                     /* number of tick left to wait */

  struct loop_s {
    uint16_t off;                       /* loop point */
    uint16_t cnt;                       /* loop count */
  } loops[MAX_LOOP], *loop_sp;
  note_t note;                          /* note (and slide) info */
};

struct q4_s {
  song_t * song;
  vset_t * vset;
  info_t * info;
  uint_t songsz;
  uint_t vsetsz;
  uint_t infosz;
};

struct play_s {

  str_t * vseturi;
  str_t * songuri;
  str_t * infouri;

  vset_t vset;
  song_t song;
  info_t info;

  uint_t tick;                          /* current tick */
  uint_t max_ticks;                     /* maximum tick to play ( */
  uint_t spr;                           /* sampling rate (hz) */

  int16_t * mix_buf;                    /* mix buffer [pcm_per_tick] */
  int16_t * mix_ptr;                    /* current pull pointer */
  uint16_t pcm_per_tick;                /* pcm per tick */
  uint16_t rate;               /* player rate (usually 200hz) */
  uint8_t  muted_voices;       /* channels mask */
  uint8_t  has_loop;           /* channels mask */
  uint8_t  end_detect;         /* true if trying to auto detect end */
  uint8_t  done;               /* true when done */
  uint8_t  format;             /* see ZZ_FORMAT_ enum */

  mixer_t * mixer;
  void    * mixer_data;
  out_t   * out;
  chan_t chan[4];
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

/* EXTERN_C */
/* mixer_t * const zz_mixers[], * zz_default_mixer, mixer_void; */

/* ---------------------------------------------------------------------- */

/**
 * Motorola integers
 * @{
 */

#ifdef __m68k__

static inline uint16_t u16(const uint8_t * const v) {
  return *(const uint16_t *)v;
}

static inline uint32_t u32(const uint8_t * const v) {
  return *(const uint32_t *)v;
}

static inline uint_t always_inline mulu(uint16_t a, uint16_t b)
{
  uint32_t c;
  asm ("mulu.w %1,%0\n\t" : "=d" (c) : "iSd" (a), "0" (b));
  return c;
}

static inline uint_t always_inline divu(uint32_t v, uint16_t d)
{
  asm("divu.w %1,%0\n\t" : "+d" (v) : "iSd" (d));
  return (uint16_t) v;
}

static inline uint_t always_inline modu(uint32_t v, uint16_t d)
{
  asm("divu.w %1,%0  \n\t"
      "clr.w %0      \n\t"
      "swap %0       \n\t"
      : "+d" (v) : "iSd" (d));
  return v;
}


static inline uint_t always_inline divu32(uint32_t v, uint16_t d)
{
  uint32_t tp1, tp2;
  asm("move.l %[val],%[tp1]  \n\t" /* tp1 = Xx:Yy */
      "clr.w %[val]          \n\t" /* val = Xx:00 */
      "sub.l %[val],%[tp1]   \n\t" /* tp1 = 00:Yy */
      "swap %[val]           \n\t" /* val = 00:Xx */
      "divu.w %[div],%[val]  \n\t" /* val = Rx:Qx */
      "move.l %[val],%[tp2]  \n\t" /* tp2 = Rx:Qx */
      "clr.w %[tp2]          \n\t" /* tp2 = Rx:00 */
      "move.w %[tp1],%[tp2]  \n\t" /* tp2 = Rx:Yy */
      "divu.w %[div],%[tp2]  \n\t" /* tp2 = Ri:Qi */
      "swap %[val]           \n\t" /* val = Qx:Rx */
      "move.w %[tp2],%[val]  \n\t" /* val = Qx:Qi */
      : [val] "+d" (v), [tp1] "=r" (tp1), [tp2] "=d" (tp2)
      : [div] "iSd" (d)
      :
    );
  return v;
}

#else

static inline uint16_t u16(const uint8_t * const v) {
  return ((uint16_t)v[0]<<8) | v[1];
}

static inline uint32_t u32(const uint8_t * const v) {
  return ((uint32_t)u16(v)<<16) | u16(v+2);
}

static inline uint_t always_inline mulu(uint16_t a, uint16_t b)
{
  return a * b;
}

static inline uint_t always_inline divu(uint32_t n, uint16_t d)
{
  return n / d;
}

static inline uint_t always_inline modu(uint32_t n, uint16_t d)
{
  return n % d;
}

static inline uint_t always_inline divu32(uint32_t n, uint16_t d)
{
  return n / d;
}

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

EXTERN_C
void i8tofl(float * const d, const uint8_t * const s, const int n);

EXTERN_C
void fltoi16(int16_t * const d, const float * const s, const int n);

#endif
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Memory, string and binary container functions.
 * @{
 */
#define ZZBUF(X)   ((uint8_t *)((X)->_s))
#define ZZOFF(X,Y) (ZZBUF(X)+(Y))
#define ZZSTR(X)   ((char *)((X)->_s))
#define ZZLEN(X)   (X)->_l
#define ZZMAX(X)   (X)->_n

# ifndef zz_memmove
#  define zz_memmove(D,S,N) *(const char *)(D) = (char)(N)
# endif

# ifndef zz_memset
#  define zz_memset(D,S,N) *(const char *)(D) = (char)(S)
# endif

#ifndef NO_LIBC

# ifndef zz_memcmp
#  define zz_memcmp(D,S,N) memcmp(D,S,N)
# endif

# ifndef zz_memcpy
#  define zz_memcpy(D,S,N) memcpy(D,S,N)
# endif

#ifndef zz_memclr
#define zz_memclr(D,N) memset(D,0,N)
#endif

#else

EXTERN_C
void zz_memcpy(void * restrict _d, const void * _s, int n);
EXTERN_C
void zz_memclr(void * restrict _d, int n);
EXTERN_C
int zz_memcmp(const void *_a, const void *_b, int n);

#endif


#if 0 && defined NO_LIBC

EXTERN_C
void (*zz_free_func)(void ** pptr);
EXTERN_C
void * (*zz_alloc_func)(const uint_t size, int clear);

#define zz_malloc(OBJ,SIZE) (zz_alloc_func ? zz_alloc_func((SIZE),0) : 0)
#define zz_calloc(OBJ,SIZE) (zz_alloc_func ? zz_alloc_func((SIZE),1) : 0)
#define zz_free(OBJ,PTR) \
  if (zz_free_func) { zz_free_func( (void**)(PTR) ); } else

#else

EXTERN_C
void zz_free_real(void ** pptr);
EXTERN_C
void *zz_alloc_real(const uint_t size, const int clear);

#define zz_malloc(OBJ,SIZE) zz_alloc_real((SIZE),0)
#define zz_calloc(OBJ,SIZE) zz_alloc_real((SIZE),1)
#define zz_free(OBJ,PTR) zz_free_real( (void **) (PTR) ) 

#endif


EXTERN_C
str_t * zz_strset(str_t * str, const char * set);
EXTERN_C
str_t * zz_stralloc(unsigned int size);
EXTERN_C
str_t * zz_strdup(const char * org);
EXTERN_C
void zz_strfree(str_t ** pstr);
EXTERN_C
int zz_strlen(str_t * str);
EXTERN_C
void bin_free(bin_t ** pbin);
EXTERN_C
int bin_alloc(bin_t ** pbin, const char * uri, uint_t len, uint_t xlen);
EXTERN_C
int bin_read(bin_t * bin, vfs_t vfs, uint_t off, uint_t len);
EXTERN_C
int bin_load(bin_t ** pbin, vfs_t vfs, uint_t len, uint_t xlen, uint_t max);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Messages logging.
 * @{
 */
#ifndef FMT12
# if defined(__GNUC__) || defined(__clang__)
#  define FMT12 __attribute__ ((format (printf, 1, 2)))
# else
#  define FMT12
# endif
#endif

#ifdef NO_LOG
# define zz_log_err(FMT,...) zz_void
# define zz_log_wrn(FMT,...) zz_void
# define zz_log_inf(FMT,...) zz_void
# define zz_log_dbg(FMT,...) zz_void
#else
EXTERN_C FMT12 void
zz_log_err(const char * fmt,...);
EXTERN_C FMT12 void
zz_log_wrn(const char * fmt,...);
EXTERN_C FMT12 void
zz_log_inf(const char * fmt,...);
EXTERN_C FMT12 void
zz_log_dbg(const char * fmt,...);
#endif

#define emsg(FMT,...) zz_log_err(FMT,##__VA_ARGS__)
#define wmsg(FMT,...) zz_log_wrn(FMT,##__VA_ARGS__)
#define imsg(FMT,...) zz_log_inf(FMT,##__VA_ARGS__)
#ifndef NDEBUG
# define dmsg(FMT,...) zz_log_dbg(FMT,##__VA_ARGS__)
#else
# define dmsg(FMT,...) zz_void
#endif

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * VFS functions.
 * @{
 */
EXTERN_C
int vfs_register(const vfs_dri_t * dri);
EXTERN_C
int vfs_unregister(const vfs_dri_t * dri);
EXTERN_C
const char * vfs_uri(vfs_t vfs);
EXTERN_C
void vfs_del(vfs_t * pvfs);
EXTERN_C
vfs_t vfs_new(const char * uri, ...);
EXTERN_C
int vfs_open_uri(vfs_t * pvfs, const char * uri);
EXTERN_C
int vfs_open(vfs_t vfs);
EXTERN_C
int vfs_read(vfs_t vfs, void *b, int n);
EXTERN_C
int vfs_read_exact(vfs_t vfs, void *b, int n);
EXTERN_C
int vfs_tell(vfs_t vfs);
EXTERN_C
int vfs_size(vfs_t vfs);
EXTERN_C
int vfs_seek(vfs_t vfs, int pos, int set);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Output functions
 * @{
 */
EXTERN_C
out_t * out_ao_open(int hz, const char * uri);
EXTERN_C
out_t * out_raw_open(int hz, const char * uri);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * voiceset and song file loader.
 * @{
 */

EXTERN_C
int song_parse(song_t *song, vfs_t vfs, uint8_t *hd, uint_t size);
EXTERN_C
int song_load(song_t *song, const char *uri);
EXTERN_C
int vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, uint_t size);
EXTERN_C
int vset_load(vset_t *vset, const char *uri);
EXTERN_C
int q4_load(vfs_t vfs, q4_t *q4);
/**
 * @}
 */
