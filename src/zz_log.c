/**
 * @file   zz_log.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  messages logging.
 */

#include "zz_private.h"

#ifdef NO_LOG

void zz_log(zz_log_t func, void * user) {}
/* # error zz_log.c should not be compiled with NO_LOG defined */
#else

static zz_log_t log_func;
static void * log_user;

void zz_log(zz_log_t func, void * user)
{
  log_func = func;
  log_user = user;
}

void zz_log_err(const char * fmt,...)
{
  va_list list;
  if (log_func) {
    va_start(list,fmt);
    log_func(ZZ_LOG_ERR, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_wrn(const char * fmt,...)
{
  va_list list;
  if (log_func) {
    va_start(list,fmt);
    log_func(ZZ_LOG_WRN, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_inf(const char * fmt,...)
{
  va_list list;
  if (log_func) {
    va_start(list,fmt);
    log_func(ZZ_LOG_INF, log_user, fmt, list);
    va_end(list);
  }
}

void zz_log_dbg(const char * fmt,...)
{
  va_list list;
  if (log_func) {
    va_start(list,fmt);
    log_func(ZZ_LOG_DBG, log_user, fmt, list);
    va_end(list);
  }
}

#endif /* NO_LOG */
