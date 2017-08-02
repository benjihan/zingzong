/**
 * @file   zz_bin.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  binary containers.
 */

#include "zz_private.h"

void
bin_free(bin_t ** pbin)
{
  zz_strfree(pbin);
}

int
bin_alloc(bin_t ** pbin, const char * path,
          uint_t len, uint_t xlen)
{
  bin_t * bin;
  assert(pbin); assert(path);
  *pbin = bin = zz_stralloc(len + xlen);
  if (!bin)
    return E_SYS;
  bin->_l = len;
  return E_OK;
}

int
bin_read(bin_t * bin, vfs_t vfs, uint_t off, uint_t len)
{
  return vfs_read_exact(vfs, ZZOFF(bin,off), len)
    ? E_INP
    : E_OK
    ;
}

int
bin_load(bin_t ** pbin, vfs_t vfs, uint_t len, uint_t xlen, uint_t max)
{
  int ecode;
  const char * path = vfs_uri(vfs);

  if (!len) {
    int pos;
    if (-1 == (pos = vfs_tell(vfs)) ||
        -1 == (len = vfs_size(vfs))) {
      ecode = E_SYS;
      goto error;
    }
    len -= pos;
  }
  if (max && len > max) {
    emsg("too large (load > %u) -- %s\n", max, path);
    ecode = E_ERR;
    goto error;
  }
  ecode = bin_alloc(pbin, path, len, xlen);
  if (ecode)
    goto error;
  ecode = bin_read(*pbin, vfs, 0, len);

error:
  if (ecode)
    bin_free(pbin);
  return ecode;
}
