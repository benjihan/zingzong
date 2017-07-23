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
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined (_DEFAULT_SOURCE)
# define _DEFAULT_SOURCE 1
#endif

#define ZZ_VFS_DRI
#include "zingzong.h"

#if !defined(DEBUG) && !defined(NDEBUG)
# define NDEBUG 1
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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

enum {
  E_OK, E_ERR, E_ARG, E_SYS, E_INP, E_OUT, E_SET, E_SNG, E_PLA, E_MIX,
  E_666 = 66
};

/* ----------------------------------------------------------------------
 *  Types definitions
 * ----------------------------------------------------------------------
 */

typedef unsigned int   uint_t;    /**< unsigned int. */
typedef unsigned short ushort_t;  /**< unsigned short. */
typedef struct str_s   str_t;     /**< strings (static or dynamic) */
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
typedef struct out_s   out_t;     /**< output. */

struct str_s {
  char   *s;                  /**< string pointer. */
  uint_t  l;                  /**< length including ending zero */
  uint_t  n;                  /**< maximum allocated string */
  char    b[1];               /**< string buffer for dynamic string */
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
  int   (*pull)(play_t * const, int);  /**> pull PCM function.        */
};

struct bin_s {
  uint_t   size;                        /**< data size */
  uint_t   xtra;                        /**< xtra allocated bytes */
  uint8_t  data[sizeof(int)];           /**< data array */
};

struct inst_s {
  int len;                              /**< size in byte */
  int lpl;                              /**< loop length in byte */
  int end;                              /**< unroll end */
  uint8_t * pcm;                        /**< sample address */
};

struct vset_s {
  bin_t * bin;                    /**< data container */
  int khz;                        /**< sampling rate from .set */
  int nbi;                        /**< number of instrument [1..20] */
  inst_t inst[20];                /**< instrument definition */
};

struct sequ_s {
  uint8_t cmd[2],len[2],stp[4],par[4];
};

struct song_s {
  bin_t * bin;
  uint8_t khz;
  uint8_t barm;
  uint8_t tempo;
  uint8_t sigm;
  uint8_t sigd;
  uint32_t stepmin;
  uint32_t stepmax;
  sequ_t *seq[4];
};

struct info_s {
  bin_t *bin;
  char * comment;
};

struct note_s {
  int cur, aim, stp;
  inst_t * ins;
};

struct chan_s {
  int      id;                          /* letter ['A'..'D'] */
  int      num;                         /* channel number [0..3] */

  sequ_t  *seq;                         /* sequence address */
  sequ_t  *cur;                         /* next sequence */
  sequ_t  *end;                         /* last sequence */
  sequ_t  *sq0;                         /* First wait-able command */
  sequ_t  *sqN;                         /* Last  wait-able command */

  int loop_level;
  struct {
    sequ_t *seq;                        /* loop point */
    int     cnt;                        /* loop count */
  } loop[MAX_LOOP];

  note_t note;                          /* note (and slide) info */

  int trig;                          /* see TRIG_* enum */
  int curi;                          /* current instrument number */
  int wait;                          /* number of tick left to wait */
  int has_loop;                      /* has loop (counter) */

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
  str_t strings[4];                     /* static strings */

  str_t * vseturi;
  str_t * songuri;
  str_t * infouri;
  str_t * waveuri;

  vset_t vset;
  song_t song;
  info_t info;

  uint_t tick;
  uint_t max_ticks;

  int16_t * mix_buf;

  uint_t pcm_per_tick;
  uint_t spr;

  uint16_t rate;               /* player rate (usually 200hz) */
  uint8_t  muted_voices;       /* channels mask */
  uint8_t  has_loop;           /* channels mask */
  uint8_t  end_detect;         /* true if trying to auto detect end */
  uint8_t  done;               /* true when done */

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

EXTERN_C
mixer_t * const zz_mixers[], * zz_default_mixer;


/* ---------------------------------------------------------------------- */

/**
 * Motorola integers
 * @{
 */
static inline uint16_t u16(const uint8_t * const v) {
  return ((uint16_t)v[0]<<8) | v[1];
}

static inline uint32_t u32(const uint8_t * const v) {
  return ((uint32_t)u16(v)<<16) | u16(v+2);
}
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
 * Memory and string functions.
 * @{
 */
EXTERN_C
void zz_free(const char * obj, void * pptr);
EXTERN_C
void *zz_alloc(const char * obj, const uint_t size, const int clear);
EXTERN_C
void *zz_calloc(const char * obj, const uint_t size);
EXTERN_C
void *zz_malloc(const char * obj, const uint_t size);
EXTERN_C
str_t * zz_strset(str_t * str, const char * set);
EXTERN_C
str_t * zz_stralloc(unsigned int size);
EXTERN_C
str_t * zz_strdup(const char * org);
EXTERN_C
void zz_strfree(str_t ** pstr);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Messages and errors.
 * @{
 */
#ifndef FMT12
# if defined(__GNUC__) || defined(__clang__)
#  define FMT12 __attribute__ ((format (printf, 1, 2)))
# else
#  define FMT12
# endif
#endif

typedef int (*msg_f)(FILE *, const char *, va_list);

EXTERN_C
msg_f msgfunc;
EXTERN_C
void ensure_newline(void);
EXTERN_C
int set_binary(FILE * f);
EXTERN_C FMT12
void emsg(const char * fmt, ...);
EXTERN_C FMT12
void wmsg(const char * fmt, ...);
EXTERN_C FMT12
void debug_msg(const char *fmt, ...);
EXTERN_C FMT12
void imsg(const char *fmt, ...);
EXTERN_C
int sysmsg(const char * obj, const char * alt);
EXTERN_C FMT12
void dummy_msg(const char *fmt, ...);

#ifndef dmsg
# if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
#  if DEBUG == 1
#   define dmsg(FMT,...) debug_msg((FMT),##__VA_ARGS__)
#  else
#   define dmsg(FMT,...) if (0) {} else
#  endif
# else
#  define dmsg dummy_msg
# endif
#endif

/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Binary container functions.
 * @{
 */
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
 * VFS functions.
 * @{
 */
EXTERN_C
int vfs_mode(const char * mode);
EXTERN_C
int vfs_add(vfs_dri_t * dri);
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
int song_load(song_t *song, const char *path);
EXTERN_C
int vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, uint_t size);
EXTERN_C
int q4_load(vfs_t vfs, q4_t *q4);
/**
 * @}
 */

/* ---------------------------------------------------------------------- */

/**
 * Player
 * @{
 */
EXTERN_C
int zz_init(play_t * P);
EXTERN_C
int zz_play(play_t * P);
EXTERN_C
int zz_kill(play_t * P);
/**
 * @}
 */
