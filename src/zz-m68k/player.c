/**
 * @file    player.c
 * @author  Benjamin Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   zingzong m68k player entry points.
 *
 * void player_init(bin_t * song, bin_t * vset);
 * void player_kill(void);
 * void player_play(void);
 */

#include "../zz_private.h"
#include "zz_m68k.h"

int song_init_header(zz_song_t const song, const void * hd);
int song_init(zz_song_t const song);
int vset_init_header(zz_vset_t const vset, const void * hd);
int vset_init(zz_vset_t const vset);

static play_t play;
static bin_t songbin, vsetbin;
static volatile zz_err_t ready, ecode;
static void * pcm = (void *)1;

static bin_t * set_bin(bin_t * bin, bin_t *src, int16_t off)
{
  bin->ptr = src->ptr + off;
  bin->max = src->max - off;
  bin->len = src->len - off;
  return bin;
}

/* /!\ Hack /!\ */
static void * newf(zz_u32_t n) { return (void *) 0xdeadbea7; }
static void delf(void * p) { }

#if defined DEBUG && defined SC68
static void
logfunc(zz_u8_t chan, void * user, const char *fmt, va_list list)
{
#ifndef NO_LIBC
  char tmp[256];
  vsnprintf(tmp,sizeof(tmp),fmt,list);
  tmp[sizeof(tmp)-1] = 0;
  fmt = tmp;
#endif

  switch (chan)
  {
  case ZZ_LOG_ERR: BRKMSG(fmt); break;
  case ZZ_LOG_WRN:
  case ZZ_LOG_INF:
  case ZZ_LOG_DBG: DBGMSG(fmt); break;
  }
}
# define LOGFUNC logfunc
#else
# define LOGFUNC 0
#endif

void player_init(bin_t * song, bin_t * vset, uint32_t d0)
{
  BRKMSG("player_init()");

  ready = 0;
  zz_memclr(&play,sizeof(play));

  /* Skip the headers */
  if (!song->ptr) song->ptr = song->_buf;
  play.song.bin = set_bin(&songbin, song, 16);
  if (!vset->ptr) vset->ptr = vset->_buf;
  play.vset.bin = set_bin(&vsetbin, vset, 222);

  zz_log_fun(LOGFUNC,0);
  zz_mem(newf,delf);

  d0 = (uint8_t)(d0-1);       /* 0->DEF 1->AGA 2->STF 3->STE 4->FAL */
  ready =
    1
    && ! song_init_header(&play.song, song->ptr)
    && ! song_init(&play.song)
    && ( play.vset.iref = play.song.iref)
    && ! vset_init_header(&play.vset, vset->ptr)
    && ! vset_init(&play.vset)
    /* && ( zz_mute(&play,0,0xF-8) || 1 ) */
    && ! zz_init(&play,0,0)             /* rate,duration */
    && ! zz_setup(&play,d0,0)           /* mixer,spr */
    ;
}

void player_kill(void)
{
  ready = 0;
  zz_close(&play);
}

void player_play(void)
{
  if (ready) {
    int16_t n = zz_play(&play,0,0);
    if (n > 0)
      n = zz_play(&play,pcm,n);
    if ( n < 0 ) {
      ready = 0;
      ecode = -n;
      BREAKP;
    }
  }
}
