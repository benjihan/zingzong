/**
 * @file   vfs_file.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  standard libc FILE VFS.
 */

#include "zz_private.h"

#if defined NO_LIBC || defined NO_VFS
# error vfs_file.c should not be compiled with NO_LIBC or NO_VFS defined
#endif

/* ---------------------------------------------------------------------- */

static int x_reg(zz_vfs_dri_t);
static int x_unreg(zz_vfs_dri_t);
static int x_ismine(const char *);
static zz_vfs_t x_new(const char *, va_list);
static void x_del(vfs_t);
static const char *x_uri(vfs_t);
static int x_open(vfs_t);
static int x_close(vfs_t);
static int x_read(vfs_t, void *, int);
static int x_tell(vfs_t);
static int x_size(vfs_t);
static int x_seek(vfs_t,int,int);

/* ---------------------------------------------------------------------- */

static struct zz_vfs_dri_s file_dri = {
  "file",
  x_reg, x_unreg,
  x_ismine,
  x_new, x_del, x_uri,
  x_open, x_close, x_read,
  x_tell, x_size, x_seek
};

zz_vfs_dri_t zz_file_vfs(void) { return &file_dri; }

/* ---------------------------------------------------------------------- */

typedef struct vfs_file_s * vfs_file_t;

struct vfs_file_s {
  struct vfs_s X;                       /**< Common to all VFS.   */
  FILE      * fp;                       /**< FILE pointer         */
  char        uri[1];                   /**< Path /!\ LAST /!\    */
};

/* ---------------------------------------------------------------------- */

static int regcnt;
static int x_reg(zz_vfs_dri_t dri) { return ++regcnt; }
static int x_unreg(zz_vfs_dri_t dri) { return regcnt>0?--regcnt:-1; }

static int
x_ismine(const char * uri)
{
  zz_assert(uri);
  return (!!*uri) << 10;
}

static const char *
x_uri(vfs_t _vfs)
{
  return ((vfs_file_t)_vfs)->uri;
}

static zz_vfs_t
x_new(const char * uri, va_list list)
{
  int len = strlen(uri);
  vfs_file_t const fs = zz_malloc("vfs-file",sizeof(*fs)+len);
  if (fs) {
    fs->X.dri = &file_dri;
    fs->fp = 0;
    zz_memcpy(fs->uri, uri, len+1);
  }
  return (zz_vfs_t) fs;
}

static void
x_del(vfs_t vfs)
{
  zz_free("vfs-file", (void *)&vfs);
  if (vfs)
    vfs->err = errno;
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
  fs->X.err = errno;
  return ret;
}

static int
x_open(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  if (fs->fp)
    return -1;
  fs->fp = fopen(fs->uri,"rb");
  fs->X.err = errno;
  return -!fs->fp;
}

static int
x_read(vfs_t const _vfs, void * ptr, int n)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  int ret = fread(ptr,1,n,fs->fp);
  fs->X.err = errno;
  return ret;
}

static int
x_tell(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  int ret = ftell(fs->fp);
  fs->X.err = errno;
  return ret;
}

static int
x_size(vfs_t const _vfs)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  size_t tell, size;
  int ret =
    (-1 == (tell = ftell(fs->fp))    || /* save position    */
     -1 == fseek(fs->fp,0,SEEK_END)  || /* go to end        */
     -1 == (size = ftell(fs->fp))    || /* size = position  */
     -1 == fseek(fs->fp,tell,SEEK_SET)) /* restore position */
    ? -1 : size;
  fs->X.err = errno;
  return ret;
}

static int
x_seek(vfs_t const _vfs, int offset, int whence)
{
  vfs_file_t const fs = (vfs_file_t) _vfs;
  int ret = fseek(fs->fp,offset,whence);
  fs->X.err = errno;
  return ret;
}
