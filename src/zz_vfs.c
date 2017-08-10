/**
 * @file   zz_vfs.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High level VFS functions.
 */

#include "zz_private.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define VFS_OR_DIE(E) if (!(E)) {errno=EINVAL; return -1;} else errno=0
#define VFS_OR_NIL(E) if (!(E)) {errno=EINVAL; return  0;} else errno=0

EXTERN_C const vfs_dri_t vfs_file_dri;

#ifndef DRIVER_MAX
# define DRIVER_MAX 8
#endif

static vfs_dri_t * drivers[DRIVER_MAX] = {
  &vfs_file_dri,
  0
};


static int
vfs_emsg(const char * dri, const char * fct, const char * alt)
{
  emsg("VFS(%s): %s -- %s\n",
       dri, fct, errno ? strerror(errno) : (alt?alt:"failed"));
  return -1;
}

int
vfs_add(vfs_dri_t * dri)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i = max;

  if (dri) {
    for (i=0; i<max; ++i) {
      if (!drivers[i])
        drivers[i] = dri;
      if (drivers[i] == dri)
        break;
    }
    if (i == max) {
      errno = 0;
      vfs_emsg(dri->name,"add",0);
    }
  }
  return -(i==max);
}

vfs_t
vfs_new(const char * uri, ...)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i, best, k = -1;
  vfs_t vfs = 0;

  assert(uri);
  errno = 0;
  for (i=0, best=0, k=-1; i<max; ++i) {
    int score;
    if (!drivers[i]) continue;
    score = drivers[i]->ismine(uri);
    if (score > best) {
      best = score;
      k = i;
    }
  }

  if (k < 0)
    emsg("VFS: not available -- %s\n",uri);
  else {
    va_list list;
    va_start(list,uri);
    vfs = drivers[k]->new(uri, list);
    va_end(list);
    if (!vfs)
      vfs_emsg(drivers[k]->name,"new",0);
  }
  return vfs;
}

int
vfs_open(vfs_t vfs)
{
  int ret;
  VFS_OR_DIE(vfs);
  ret = vfs->dri->open(vfs);
  if (ret < 0)
    vfs_emsg(vfs->dri->name,"open",0);
  return ret;
}

void
vfs_del(vfs_t * pvfs)
{
  vfs_t const vfs = pvfs ? *pvfs : 0;
  if (vfs) {
    vfs->dri->close(vfs);
    vfs->dri->del(vfs);
    *pvfs = 0;
  }
}

int
vfs_close(vfs_t vfs)
{
  VFS_OR_DIE(vfs);
  return vfs->dri->close(vfs)
    ? vfs_emsg( vfs->dri->name, "close", 0)
    : 0
    ;
}

int
vfs_read(zz_vfs_t vfs, void * ptr, int size)
{
  if (!size || size == -1)
    return size;
  VFS_OR_DIE(vfs && ptr);
  return vfs->dri->read(vfs,ptr,size);
}

int
vfs_read_exact(vfs_t vfs, void * ptr, int size)
{
  uint_t n;
  if (!size || size == -1)
    return size;
  VFS_OR_DIE(vfs && ptr);
  n = vfs->dri->read(vfs,ptr,size);

  if (n == -1)
    sysmsg(vfs->dri->uri(vfs),"read");
  else if (n != size)
    emsg("%s: read too short by %u\n",
         vfs->dri->uri(vfs), size-n);
  return -(n != size);
}

int
vfs_tell(vfs_t vfs)
{
  VFS_OR_DIE(vfs);
  return vfs->dri->tell(vfs);
}

int
vfs_seek(vfs_t vfs, int pos, int set)
{
  VFS_OR_DIE(vfs);

  if (vfs->dri->seek) {
    return !vfs->dri->seek(vfs,pos,set)
      ? 0 : vfs_emsg(vfs->dri->name, "seek", "seek error");
  } else {
    /* For seek forward simulate by reading */
    int size, tell;

    dmsg("seek[%s]: pos=%d set=%d\n",
         vfs->dri->name, pos, set);

    if ( -1 == (size = vfs->dri->size(vfs)) ||
         -1 == (tell = vfs->dri->tell(vfs)) )
      return -1;

    dmsg("seek[%s]: size=%u\n", vfs->dri->name, size);
    dmsg("seek[%s]: tell=%u\n", vfs->dri->name, tell);

    switch (set) {
    case ZZ_SEEK_SET: break;
    case ZZ_SEEK_END: pos = size + pos; break;
    case ZZ_SEEK_CUR: pos = tell + pos; break;
    default:
      errno = EINVAL;
      return vfs_emsg(vfs->dri->name, "seek", "invalid whence");
    }

    dmsg("seek[%s]: goto=%d offset=%+d\n",
         vfs->dri->name, pos, pos-tell);

    if (pos < tell || pos > size) {
      emsg("%s: seek simulation impossible (%d/%d/%d)\n",
           vfs->dri->uri(vfs), pos, tell, size);
      return -1;
    }

    while (tell < pos) {
      uint8_t tmp[64];
      int r, n = pos-tell;
      if (n > sizeof(tmp)) n = sizeof(tmp);
      r = vfs->dri->read(vfs,tmp,n);
      if (r != n) {
        emsg("%s: seek simulation impossible (read error)\n",
             vfs->dri->uri(vfs));
        return -1;
      }
      tell += n;
    }

    dmsg("seek[%s]: tell=%d\n",
         vfs->dri->name, vfs->dri->tell(vfs));

    assert (tell == pos);
    assert (pos == vfs->dri->tell(vfs));
    return 0;
  }
}

int
vfs_size(vfs_t vfs)
{
  VFS_OR_DIE(vfs);
  return vfs->dri->size(vfs);
}

const char *
vfs_uri(vfs_t vfs)
{
  VFS_OR_NIL(vfs);
  return vfs->dri->uri(vfs);
}

int
vfs_open_uri(vfs_t * pvfs, const char * uri)
{
  vfs_t vfs = 0;
  errno = EINVAL;
  if (pvfs && uri) {
    vfs = vfs_new(uri,0);
    if (vfs && vfs_open(vfs)) {
      vfs_del(&vfs);
      assert(vfs == 0);
      vfs = 0;
    }
    *pvfs = vfs;
  }
  return -!vfs;
}
