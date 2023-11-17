/**
 * @file    player.c
 * @author  Benjamin Gerard AKA Ben/OVR
 * @date    2017-09
 * @brief   zingzong m68k player entry points.
 *
 */

#include "../zz_private.h"
#include "zz_m68k.h"

zz_err_t zz_core_init(core_t*, mixer_t*, u32_t);
void	 zz_core_kill(core_t *);
i16_t	 zz_core_play(core_t*, void*, const i16_t);
mixer_t *zz_mixer_get(zz_u8_t * const);

typedef struct m68kplay m68k_t;

struct m68kplay {
  volatile int8_t ready;
  zz_u8_t  dri;
  void	  *set;
  uint16_t ppt;
  uint16_t rate;
  core_t   core;
};

ZZ_STATIC m68k_t play;
ZZ_STATIC bin_t songbin, vsetbin;

static bin_t * set_bin(bin_t * bin, bin_t *src, int16_t off)
{
  bin->ptr = src->ptr + off;
  bin->max = src->max - off;
  bin->len = src->len - off;
  return bin;
}

#ifdef DEBUG
ZZ_STATIC void
logfunc(zz_u8_t chan, void * user, const char *fmt, va_list list)
{
#ifndef NO_LIBC
  char tmp[256];
  vsnprintf(tmp,sizeof(tmp),fmt,list);
  tmp[sizeof(tmp)-1] = 0;
  fmt = tmp;
#endif /* NO_LIBC */
  switch (chan)
  {
  case ZZ_LOG_ERR: BRKMSG(fmt); break;
  case ZZ_LOG_WRN:
  case ZZ_LOG_INF:
  case ZZ_LOG_DBG: DBGMSG(fmt); break;
  }
}
#endif /* DEBUG */

uint8_t player_mute(uint8_t clr, uint8_t set)
{
  return zz_core_mute(&play.core,clr,set);
}

long player_core(void)
{
  return (intptr_t) &play.core;
}

long player_status(void)
{
  return play.core.code;
}

long player_driver(void)
{
  return (intptr_t) play.core.mixer;
}

long player_sampling(void)
{
  return play.core.spr;
}

void player_kill(void)
{
  play.ready = 0;
  zz_core_kill(&play.core);
}

long player_blend(uint8_t cmap, uint16_t blend)
{
  return zz_core_blend(play.ready ? &play.core : 0, cmap, blend);
}

long player_rate(uint16_t rate)
{
  if (rate) {
    if (rate < RATE_MIN)
      rate = RATE_MIN;
    else if (rate > RATE_MAX)
      rate = RATE_MAX;
    play.rate = rate;
    play.ppt  = divu(play.core.spr+(rate>>1),rate);
    dmsg("rate:%huhz spr:%luhz ppt:%hu\n",
	 HU(rate), LU(play.core.spr), HU(play.ppt));
  }
  return play.rate;
}

long player_init(bin_t * song, bin_t * vset, uint32_t dri, uint32_t spr)
{
  mixer_t * mixer = 0;
  zz_err_t err = 0;
#ifdef DEBUG
  zz_log_fun(logfunc,0);
#endif

  /* specific m68k checks */
  zz_assert( sizeof(inst_t) == 16 );

  BRKMSG("player_init()");

  if (play.ready)
    player_kill();

  if (dri >= 0x100) {
    play.dri = ZZ_MIXER_XTN;
    mixer = (mixer_t*) dri;
  }
  else {
    play.dri = (uint8_t) (dri-1);
    mixer = zz_mixer_get(&play.dri);
  }

  /* Skip the headers because that's how it is. */

  if (!song->ptr) song->ptr = song->_buf;
  if (!vset->ptr) vset->ptr = vset->_buf;

  if (play.set != vset->ptr
      ||
      U32(vset->ptr) != FCC('V','S','E','T'))
  {
    play.core.vset.bin = set_bin(&vsetbin, vset, 222);
    err = err
      || vset_init_header(&play.core.vset, vset->ptr)
      || vset_init(&play.core.vset)
      ;
    if (!err) {
      *(uint32_t *)vset->ptr = FCC('V','S','E','T');
      play.set = vset->ptr;
    }
  }

  play.core.song.bin = set_bin(&songbin, song, 16);
  err = err
    || song_init_header(&play.core.song, song->ptr)
    || song_init(&play.core.song)
    ;
  zz_assert( ! play.core.code );

  err = err
    || zz_core_init(&play.core, mixer, spr)
    ;
  zz_assert( ! play.core.code );

  if (!play.rate)
    play.rate = 200;
  player_rate(play.core.song.rate ? play.core.song.rate : play.rate);

  dmsg("init: mixer#%hu:%s code:%hx tic:%hu ppt:%hu spr:%hu\n",
       HU(dri),play.core.mixer->name,
       HU(play.core.code),
       HU(play.rate), HU(play.ppt), HU(play.core.spr));

  zz_assert( ! play.core.code );
  zz_assert( ! err );

  play.ready = play.core.code == ZZ_OK;
  return play.core.code;
}

void player_play(void)
{
  if (play.ready) {
    i16_t ret = zz_core_play(&play.core, (void*)-1, play.ppt);
    play.ready = ret > 0;
    if (!play.ready)
      dmsg("tick#%lu / pcm:%hi / code:%hu\n",
	   LU(play.core.tick), HI(ret), HU(play.core.code));
    zz_assert( ret == play.ppt );
    zz_assert( !play.core.code );
  }
}
