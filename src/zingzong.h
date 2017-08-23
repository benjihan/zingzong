/**
 * @file   zingzong.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  zingzong public API.
 */

#ifndef ZINGZONG_H
#define ZINGZONG_H

#ifdef __cplusplus
extern "C" {
#endif

  enum {
    ZZ_FORMAT_UNKNOWN,
    ZZ_FORMAT_4V,
    ZZ_FORMAT_4Q,
  };

  typedef struct vfs_s  * restrict zz_vfs_t;
  typedef struct vset_s * restrict zz_vset_t;
  typedef struct song_s * restrict zz_song_t;
  typedef struct play_s * restrict zz_play_t;

  /* int zz_get_vernum(); */
  /* int zz_get_verstr(); */
  /* int zz_player_new(zz_play_t * play, int extra); */
  /* int zz_player_free(zz_play_t * play); */

  int zz_load(zz_play_t const play,
              const char * songuri, const char * pvseturi);
  int zz_close(zz_play_t const play);
  int (*zz_guess_vset)(zz_play_t const play, const char * songuri);

  /**
   */
  /* int zz_player_cntl(zz_play_t play, int ZZ_OP); */

  zz_song_t * zz_player_song(zz_play_t const play);
  zz_vset_t * zz_player_vset(zz_play_t const play);

  int song_init_header(zz_song_t const song, const void * hd);
  int song_init(zz_song_t const song);

  int vset_init_header(zz_vset_t const vset, const void * hd);
  int vset_init(zz_vset_t const vset);

/* ----------------------------------------------------------------------
 *  VFS driver (for extension)
 * ----------------------------------------------------------------------
 */
#ifdef ZZ_VFS_DRI

#include <stdarg.h>

  enum {
    ZZ_SEEK_SET, ZZ_SEEK_CUR, ZZ_SEEK_END
  };

  typedef const struct {
    const char * name;                      /**< friendly name. */
    int (*ismine)(const char *);            /**< is mine.       */
    zz_vfs_t (*new)(const char *, va_list); /**< create VFS.    */
    void (*del)(zz_vfs_t);                  /**< destroy VFS.   */
    const char * (*uri)(zz_vfs_t);          /**< get uri.       */
    int (*open)(zz_vfs_t);                  /**< open.          */
    int (*close)(zz_vfs_t);                 /**< close.         */
    int (*read)(zz_vfs_t, void *, int);     /**< read.          */
    int (*tell)(zz_vfs_t);                  /**< get position.  */
    int (*size)(zz_vfs_t);                  /**< get size.      */
    int (*seek)(zz_vfs_t,int,int);          /**< offset,whence. */
  } zz_vfs_dri_t;

  struct vfs_s {
    zz_vfs_dri_t * dri;
  };

#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef ZINGZONG_H */
