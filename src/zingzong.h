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

typedef struct vfs_s *vfs_t;            /**< Virtual filesystem. */

/* ----------------------------------------------------------------------
 *  VFS driver (for extension)
 * ----------------------------------------------------------------------
 */
#ifdef ZZ_VFS_DRI

#include <stdarg.h>

typedef const struct {
  const char * name;                      /**< friendly name.    */
  int (*ismine)(const char *);            /**< is mine.          */
  vfs_t (*new)(const char *, va_list);    /**< create VFS.       */
  void (*del)(vfs_t);                     /**< destroy VFS.      */
  const char * (*uri)(vfs_t);             /**< get uri.          */
  int (*open)(vfs_t);                     /**< open.             */
  int (*close)(vfs_t);                    /**< close.            */
  int (*read)(vfs_t, void *, int);        /**< read.             */
  int (*tell)(vfs_t);                     /**< get position.     */
  int (*size)(vfs_t);                     /**< get size.         */
} vfs_dri_t;

struct vfs_s {
  vfs_dri_t * dri;
};

#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef ZINGZONG_H */
