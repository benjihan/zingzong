/**
 * @file   zz_msg.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  messages and errors.
 */

#include "zz_private.h"
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

EXTERN_C const char me[];      /* Program name for error messages. */
static int newline = 1;        /* Lazy newline tracking. */
static int can_use_stdout = 1; /* Whether stdout is used for other mean. */

static int deffunc(FILE *f, const char *fmt, va_list list)
{
  return (!f)
    ? vprintf(fmt,list)
    : vfprintf(f,fmt,list)
    ;
}

msg_f msgfunc = deffunc;

int set_binary(FILE * f)
{
  int fd = -1;

  if (f == stdout)
    can_use_stdout = 0;

  fd = fileno(f);
  if (fd == -1)
    return sysmsg("fileno","fileno");

#ifdef STDOUT_FILENO
  if (fd == STDOUT_FILENO)
    can_use_stdout = 0;
#endif

#ifdef WIN32
  if (1) {
    int err;
    errno = 0;
    err = _setmode(fd, _O_BINARY);
    if ( err == -1 || errno )
      return sysmsg("setmode","setmode");
  }
#endif
  return 0;
}

static FILE * stdout_or_stderr(void)
{
  return can_use_stdout ? stdout : stderr;
}

static void set_newline(const char * fmt)
{
  if (*fmt)
    newline = fmt[strlen(fmt)-1] == '\n';
}

void ensure_newline(void)
{
  if (!newline) {
    fputc('\n',stdout_or_stderr());
    newline = 1;
  }
}


static int
vmsg(msg_f fct, FILE *f, const char * fmt, va_list list)
{
  return fct(f,fmt,list);
}

static int
msg(msg_f fct, FILE *f, const char * fmt, ...)
{
  int ret;
  va_list list;
  va_start(list,fmt);
  ret = vmsg(fct,f,fmt,list);
  va_end(list);
  return ret;
}

void
emsg(const char * fmt, ...)
{
  va_list list;

  msg(msgfunc, stderr, "\n%s: "+newline, me);
  newline = 0;

  va_start(list,fmt);
  vmsg(msgfunc, stderr, fmt, list);
  set_newline(fmt);
  va_end(list);
}

int
sysmsg(const char * obj, const char * alt)
{
  const char * msg = errno ? strerror(errno) : alt;

  if (obj && msg)
    emsg("%s -- %s\n", msg, obj);
  else if (obj)
    emsg("%s\n", obj);
  else if (msg)
    emsg("%s\n", msg);
  return E_SYS;
}

void
wmsg(const char * fmt, ...)
{
  va_list list;

  msg(msgfunc, stderr,"%s","\nWARNING: "+newline);
  newline = 0;

  va_start(list,fmt);
  vmsg(msgfunc, stderr, fmt, list);
  set_newline(fmt);
  fflush(stderr);
  va_end(list);
}

void
debug_msg(const char *fmt, ...)
{
  FILE * const out = stdout_or_stderr();
  va_list list;

  ensure_newline();
  va_start(list,fmt);
  vfprintf(out,fmt,list);
  va_end(list);
  set_newline(fmt);
  fflush(out);
}

void
dummy_msg(const char *fmt, ...) {}

void
imsg(const char *fmt, ...)
{
  FILE * const out = stdout_or_stderr();
  va_list list;

  ensure_newline();
  va_start(list,fmt);
  vmsg(msgfunc, out, fmt, list);
  va_end(list);
  set_newline(fmt);
  fflush(out);
}

void
dummymsg(const char *fmt, ...) { }
