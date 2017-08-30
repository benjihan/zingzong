/**
 * @file   zz_vfs.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  High level VFS functions.
 */

#include "zz_private.h"

#ifdef NO_VFS
# error  zz_vfs.c should not be compiled with NO_VFS defined
#endif

#define VFS_OR_DIE(E) if (!(E)) { return -1; } else (E)->err=0
#define VFS_OR_NIL(E) if (!(E)) { return  0; } else (E)->err=0

#ifndef DRIVER_MAX
# define DRIVER_MAX 8
#endif

static zz_vfs_dri_t drivers[DRIVER_MAX];

#ifdef NO_LOG
# define vfs_emsg(DRI,ERR,FCT,ALT,OBJ) (-1)
# else
static int
vfs_emsg(const char * dri, int err, const char * fct, const char * alt, const char * obj)
{
  const char* msg = alt ? alt : "failed";

#ifndef NO_LIBC
  if (err)
    msg = strerror(err);
#endif
  if (!obj)
    emsg("VFS(%s): %s: (%d) %s\n",
         dri, fct, err, msg);
  else
    emsg("VFS(%s): %s: (%d) %s -- %s\n",
         dri, fct, err, msg, obj);
  return -1;
}
#endif

static int
vfs_find(zz_vfs_dri_t dri)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i;
  for (i=0; i<max && drivers[i] != dri; ++i)
    ;
  return i<max ? i : -1;
}


int
vfs_register(zz_vfs_dri_t dri)
{
  int i = -1;
  if (dri) {
    i = vfs_find(dri);              /* looking for this driver slot */
    if (i < 0)                      /* not found ? */
      i = vfs_find(0);              /* looking for a free slot */
    if (i >= 0) {
      if (dri->reg(dri) >= 0)
        drivers[i] = dri;
      else
        i = -1;
    }
    if (i < 0)
      (void) vfs_emsg(dri->name,0,"register",0,0);
  }
  return -(i<0);
}

int
vfs_unregister(zz_vfs_dri_t dri)
{
  int i = -1;
  if (dri) {
    i = vfs_find(dri);              /* looking for this driver slot */
    if (i >= 0) {                   /* found ? */
      int res = drivers[i]->unreg(dri);
      if (!res)
        drivers[i] = 0;
      else if (res < 0)
        i = vfs_emsg(dri->name,0,"unregister",0,0);
    }
  }
  return -(i<0);
}


vfs_t
vfs_new(const char * uri, ...)
{
  const int max = sizeof(drivers)/sizeof(*drivers);
  int i, best, k = -1;
  vfs_t vfs = 0;

  zz_assert(uri);
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
    if (vfs)
      vfs->err = 0;
    else
      (void)vfs_emsg(drivers[k]->name,0,"new",0,uri);
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
    ret = vfs_emsg(vfs->dri->name,vfs->err,"open",0,vfs_uri(vfs));
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
    ? vfs_emsg(vfs->dri->name,vfs->err,"close", 0, vfs_uri(vfs))
    : 0
    ;
}

int
vfs_read(zz_vfs_t vfs, void * ptr, int size)
{
  if (!size || size == -1)
    return size;
  if (!ptr) vfs = 0;
  VFS_OR_DIE(vfs);
  return vfs->dri->read(vfs,ptr,size);
}

int
vfs_read_exact(vfs_t vfs, void * ptr, int size)
{
  uint_t n;
  if (!size || size == -1)
    return size;
  if (!ptr) vfs = 0;
  VFS_OR_DIE(vfs);
  n = vfs->dri->read(vfs,ptr,size);

  if (n == -1)
    n = vfs_emsg(vfs->dri->name, vfs->err,"read_exact", 0, vfs_uri(vfs));
  else if (n != size) {
    emsg("%s: read too short by %u\n",
             vfs->dri->uri(vfs), size-n);
    n = -1;
  }
  else
    n = 0;
  return n;
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
      ? 0
      : vfs_emsg(vfs->dri->name,vfs->err,"seek","seek error", vfs_uri(vfs));
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
      return vfs_emsg(vfs->dri->name, vfs->err=ZZ_EINVAL,
                      "seek", "invalid whence", vfs_uri(vfs));
    }

    dmsg("seek[%s]: goto=%d offset=%+d\n",
         vfs->dri->name, pos, pos-tell);

    if (pos < tell || pos > size) {
      dmsg("%s: seek simulation impossible (%d/%d/%d)\n",
           vfs->dri->uri(vfs), pos, tell, size);
      return -1;
    }

    while (tell < pos) {
      uint8_t tmp[64];
      int r, n = pos-tell;
      if (n > sizeof(tmp)) n = sizeof(tmp);
      r = vfs->dri->read(vfs,tmp,n);
      if (r != n)
        return vfs_emsg(vfs->dri->name, vfs->err=ZZ_EIO,
                        "seek", "simulation impossible", vfs_uri(vfs));
      tell += n;
    }

    dmsg("seek[%s]: tell=%d\n",
         vfs->dri->name, vfs->dri->tell(vfs));

    zz_assert( tell == pos );
    zz_assert( pos == vfs->dri->tell(vfs) );
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
  const char * uri = vfs ? vfs->dri->uri(vfs) : 0;
  if (!uri) uri = "";
  return uri;
}

int
vfs_open_uri(vfs_t * pvfs, const char * uri)
{
  vfs_t vfs = 0;
  if (pvfs && uri) {
    vfs = vfs_new(uri,0);
    if (vfs && vfs_open(vfs)) {
      vfs_del(&vfs);
      zz_assert(vfs == 0);
      vfs = 0;
    }
    *pvfs = vfs;
  }
  return -!vfs;
}
