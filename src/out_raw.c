/**
 * @file   out_raw.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Raw file output (files/null/stdout/stderr) .
 */

#define ZZ_DBG_PREFIX "(raw) "
#include "zingzong.h"
#include "zz_def.h"

#if defined NO_LIBC
# error out_raw.c should not be compiled with NO_LIBC
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#if defined WIN32 || defined _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <unistd.h> /* STDOUT_FILENO ... */

#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

typedef struct out_raw_s out_raw_t;

struct out_raw_s {
  zz_out_t out;
  FILE    *fp;
};

static zz_err_t xclose(zz_out_t *);
static zz_u16_t xwrite(zz_out_t *, void *, zz_u16_t);

static out_raw_t raw = {
  { "raw", 0, 0, xclose, xwrite }
};

static FILE * const null_ptr = (FILE *)&raw;

int8_t can_use_std = 3;

int set_file_binary(FILE * f)
{
  int fd = -1;

  if (f == stdout)
    can_use_std &= ~1;
  else if (f == stderr)
    can_use_std &= ~2;

  fd = fileno(f);
  if (fd == -1) {
    emsg("fileno: (%d) %s\n", errno, strerror(errno));
    return -1;
  }

  if (fd == STDOUT_FILENO)
    can_use_std &= ~1;
  if (fd == STDERR_FILENO)
    can_use_std &= ~2;

#if defined WIN32 || defined _WIN32
  if (1) {
    int err;
    errno = 0;
    err = _setmode(fd, _O_BINARY);
    if ( err == -1 || errno )
      emsg("setmode: (%d) %s\n", errno, strerror(errno));
  }
#endif
  return 0;
}

static int uri_is_null(const char * uri)
{
  return 0
    || !strncasecmp(uri,"null:",5)
    || !strcasecmp(uri,"/dev/null")
    ;
}

static int uri_is_stdout(const char * uri)
{
  return 0
    || !strncasecmp(uri,"stdout:",7)
    || !strcasecmp(uri,"/dev/stdout")
    || !strcasecmp(uri,"/dev/fd/" CPPSTR(STDOUT_FILENO))
    ;
}

static int uri_is_stderr(const char * uri)
{
  return 0
    || !strncasecmp(uri,"stderr:",7)
    || !strcasecmp(uri,"/dev/stderr")
    || !strcasecmp(uri,"/dev/fd/" CPPSTR(STDERR_FILENO))
    ;
}

zz_out_t * out_raw_open(zz_u32_t hz, const char * uri)
{
  zz_assert( ! raw.fp );

  raw.fp = 0;
  if (uri_is_null(uri)) {
    raw.out.name = "<null>";
    raw.fp = null_ptr;
  }
  else if (uri_is_stdout(uri)) {
    raw.out.name = "<stdout>";
    if (set_file_binary(stdout))
      return 0;
    raw.fp = stdout;
  }
  else if (uri_is_stderr(uri)) {
    raw.out.name = "<stderr>";
    if (set_file_binary(stderr))
      return 0;
    raw.fp = stderr;
  }
  else {
    raw.out.name = "<file>";
    raw.fp = fopen(uri, "wb");
    if (!raw.fp) {
      emsg("open: (%d) %s -- %s\n", errno, strerror(errno), uri);
      return 0;
    }
  }
  raw.out.uri = uri;
  raw.out.hz  = hz;
  return &raw.out;
}

static zz_err_t xclose(zz_out_t * out)
{
  if (out == &raw.out) {
    if (raw.fp == null_ptr)
      raw.fp = 0;
    if (raw.fp) {
      FILE * fp = raw.fp;
      raw.fp = 0;
      if ( (fflush(fp) | fclose(fp)) < 0 ) {
        emsg("close: (%d) %s -- %s\n", errno, strerror(errno), raw.out.uri);
        return ZZ_ESYS;
      }
    }
  }
  return ZZ_OK;
}

static zz_u16_t xwrite(zz_out_t * out, void * ptr, zz_u16_t n)
{
  zz_u16_t w = 0;

  zz_assert(n >= 0);
  zz_assert(ptr);
  zz_assert(out == &raw.out);

  if (n && ptr && out == &raw.out) {
    if (raw.fp == null_ptr)
      w = n;
    else {
      w = fwrite(ptr,1,n,raw.fp);
      if (w == (zz_u16_t)-1)
        emsg("write: (%d) %s -- %s\n", errno, strerror(errno),raw.out.uri);
      else if (w != n)
        emsg("write truncated (%hu) -- %s\n", HU(n-w), raw.out.uri);
    }
  }
  return w;
}
