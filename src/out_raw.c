/**
 * @file   out_raw.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  raw output (file,null,stdout) .
 */

#include "zz_private.h"
#include <string.h>

typedef struct out_raw_s out_raw_t;

struct out_raw_s {
  out_t out;
  FILE * fp;
};

static int close(out_t *);
static int write(out_t *, void *, int);

static out_raw_t raw = {
  { "raw", 0, 0, close, write }
};

static FILE * const null_ptr = (FILE *)&raw;

out_t * out_raw_open(int hz, const char * uri)
{
  assert ( ! raw.fp );

  raw.fp = 0;
  if (!strcmp(uri,"null:")) {
    raw.out.name = "<null>";
    raw.fp = null_ptr;
  }
  else if (!strcmp(uri,"stdout:")) {
    raw.out.name = "<stdout>";
    if (set_binary(stdout))
      return 0;
    raw.fp = stdout;
  } else {
    raw.out.name = "<file>";
    raw.fp = fopen(uri, "wb");
    if (!raw.fp) {
      sysmsg("uri", "open");
      return 0;
    }
  }
  raw.out.uri = uri;
  raw.out.hz  = hz;
  return &raw.out;
}

static int close(out_t * out)
{
  if (out == &raw.out) {
    if (raw.fp == null_ptr)
      raw.fp = 0;
    if (raw.fp) {
      FILE * fp = raw.fp;
      raw.fp = 0;
      if ( (fflush(fp) | fclose(fp)) < 0 ) {
        sysmsg(raw.out.uri,"close");
        return -1;
      }
    }
  }
  return 0;
}

static int write(out_t * out, void * ptr, int n)
{
  int w = 0;

  assert(n >= 0);
  assert(ptr);
  assert(out == &raw.out);

  if (n && ptr && out == &raw.out) {
    if (raw.fp == null_ptr)
      w = n;
    else {
      w = fwrite(ptr,1,n,raw.fp);
      if (w == -1)
        sysmsg(raw.out.uri,"write");
      else if (w != n)
        emsg("write truncated (%d) -- %s\n", n-w, raw.out.uri);
    }
  }
  return w;
}
