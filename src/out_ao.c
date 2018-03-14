/**
 * @file   out_ao.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  libao output.
 */

#ifndef NO_AO

#define ZZ_DBG_PREFIX "(aud) "
#include "zingzong.h"
#include "zz_def.h"
#include <ao/ao.h>

typedef struct aoout_s aoout_t;
struct aoout_s {
  zz_out_t          out;
  int               id;
  ao_device        *dev;
  ao_info          *info;
  ao_sample_format  fmt;
};

static zz_err_t close(zz_out_t *);
static zz_u16_t write(zz_out_t *, void *, zz_u16_t);

static aoout_t aoo = {
  { "<ao>", 0, 0, close, write }
};

static zz_err_t
close(zz_out_t * out)
{
  int ret = ZZ_OK;
  zz_assert(out == &aoo.out);

  if (aoo.dev) {
    ret = -!ao_close(aoo.dev);
    aoo.dev = 0;
    ao_shutdown();
  }
  return ret;
}

zz_out_t *
out_ao_open(zz_u32_t hz, const char * uri)
{
  zz_assert(!aoo.dev);
  if (aoo.dev) {
    errno = EAGAIN;
    return 0;
  }

  ao_initialize();
  aoo.fmt.bits        = 16;
  aoo.fmt.rate        = hz;
  aoo.fmt.channels    = 2;
  aoo.fmt.byte_format = AO_FMT_NATIVE;
  aoo.id = uri
    ? ao_driver_id("wav")
    : ao_default_driver_id()
    ;
  aoo.info = ao_driver_info(aoo.id);
  if (!aoo.info) {
    dmsg("libao: failed to get driver #%d info\n", aoo.id);
  } else {
    aoo.dev = uri
      ? ao_open_file(aoo.id, uri, 1, &aoo.fmt, 0)
      : ao_open_live(aoo.id, &aoo.fmt, 0)
      ;
    if (!aoo.dev) {
      dmsg("libao: failed to initialize audio driver #%d \"%s\"\n",
           aoo.id, aoo.info->short_name);
    } else {
      aoo.out.hz = aoo.fmt.rate;
      if (aoo.info->type == AO_TYPE_LIVE) {
        aoo.out.name = "<ao>";
        aoo.out.uri  = aoo.info->short_name;
        dmsg("ao live device#%i \"%s\" is open at %luhz\n",
             aoo.id, aoo.out.uri, LU(aoo.out.hz));
      } else  {
        aoo.out.name = aoo.info->short_name;
        aoo.out.uri  = uri;
        dmsg("ao file device#%i %s:\"%s\" is open at %luhz\n",
             aoo.id, aoo.out.name, aoo.out.uri, LU(aoo.out.hz));
      }
    }
  }

  if (!aoo.dev) {
    ao_shutdown();
    return 0;
  }

  return &aoo.out;
}

static zz_u16_t
write(zz_out_t * out, void * const pcm, zz_u16_t bytes)
{
  zz_assert(out == &aoo.out);

  if (out == &aoo.out &&
      !ao_play(aoo.dev, pcm, bytes)) {
    dmsg("libao: failed to play buffer #%d \"%s\"\n",
         aoo.id, aoo.info->short_name);
    return -1;
  }
  return bytes;
}

#endif /* #ifndef NO_AO */
