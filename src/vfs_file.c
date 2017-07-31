/**
 * @file   vfs_file.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  standard FILE VFS.
 */

#include "zz_private.h"
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------- */

static int x_ismine(const char *);
static vfs_t x_new(const char *, va_list);
static void x_del(vfs_t);
static const char *x_uri(vfs_t);
static int x_open(vfs_t);
static int x_close(vfs_t);
static int x_read(vfs_t, void *, int);
static int x_tell(vfs_t);
static int x_size(vfs_t);

/* ---------------------------------------------------------------------- */

vfs_dri_t vfs_file_dri = {
  "file", x_ismine,
  x_new, x_del, x_uri,
  x_open, x_close,
  x_read, x_tell, x_size
};

/* ---------------------------------------------------------------------- */

typedef struct {
  vfs_dri_t * dri;                      /**< Driver /!\ FIRST /!\ */
  FILE      * fp;                       /**< FILE pointer         */
  char        uri[1];                   /**< Path /!\ LAST /!\    */
} *vfs_file_t;

/* ---------------------------------------------------------------------- */

static int
x_ismine(const char * uri)
{
  assert(uri);
  return (!!*uri) << 10;
}

static const char *
x_uri(vfs_t _vfs)
{
  return ((vfs_file_t)_vfs)->uri;
}

static vfs_t
x_new(const char * uri, va_list list)
{
  int len = strlen(uri);
  vfs_file_t const fs = zz_malloc("vfs-file",sizeof(*fs)+len);
  if (fs) {
    fs->dri  = &vfs_file_dri;
    fs->fp   = 0;
    zz_memcpy(fs->uri, uri, len+1);
  }
  return (vfs_t) fs;
}

static void
x_del(vfs_t _vfs)
{
  zz_free("vfs-file", &_vfs);
}

static int
x_close(vfs_t const _vfs)
{
  int ret;
  vfs_file_t const fs = (vfs_file_t) _vfs;

  ret = fclose(fs->fp);
  if (!ret) {
    fs->fp = 0;
  }
  return ret;
}

static int
x_open(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  if (fs->fp)
    return -1;

  fs->fp = fopen(fs->uri,"rb");
  return -!fs->fp;
}

static int
x_read(vfs_t const _vfs, void * ptr, int n)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  return fread(ptr,1,n,fs->fp);
}

static int
x_tell(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  return ftell(fs->fp);
}

static int
x_size(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  size_t tell, size;
  return (-1 == (tell = ftell(fs->fp))    || /* save position    */
          -1 == fseek(fs->fp,0,SEEK_END)  || /* go to end        */
          -1 == (size = ftell(fs->fp))    || /* size = position  */
          -1 == fseek(fs->fp,tell,SEEK_SET)) /* restore position */
    ? -1 : size ;
}
