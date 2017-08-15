/*
  player.c

  COPYRIGHT (C) 2014-2017 Benjamin Gerard AKA Ben/OVR

  ----------------------------------------------------------------------
  player header

  void player_init(void)
  void player_play(void)
  void player_kill(void)
  ----------------------------------------------------------------------
*/

void player_init(void);
void player_kill(void);
void player_play(void);

#include "../zz_private.h"

/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59946
 *
 * To avoid this problem we force the player not to access any static
 * variables. Everything will be addressed relative to the bear
 * struct. */

static play_t play;
static bin_t songbin, vsetbin;
static volatile int16_t ready;

EXTERN_C mixer_t * mixer_aga_get(void); /* mix_aga.c */

static void set_bin(bin_t * bin, void * data, int32_t len)
{
  bin->_s = data;
  bin->_n = bin->_l = len;
}

void player_init(void)
{
  struct {
    int32_t songsz;                     /* d0.l */
    int32_t vsetsz;                     /* d1.l */
    uint8_t *song;                      /* a0.l */
    uint8_t *vset;                      /* a1.l */
  } p;

  /* !!! Do that first */
  asm volatile ("movem.l %%d0/%%d1/%%a0/%%a1,%0" : "=m" (p));
  ready = 0;
  zz_memclr(&play,sizeof(play));

  play.song.bin = &songbin;
  play.vset.bin = &vsetbin;
  set_bin(&songbin, p.song+16, p.songsz-16);
  set_bin(&vsetbin, p.vset+222, p.vsetsz-222);

  play.mixer = mixer_aga_get();

  ready =
    1
    && ! song_init_header(&play.song, p.song)
    && ! song_init(&play.song)
    && ! vset_init_header(&play.vset, p.vset)
    && ! vset_init(&play.vset)
    && ! zz_init(&play)
    ;
}

void player_play(void)
{
  if (ready) {
    int n = play.pcm_per_tick;
    ready = 0 != zz_pull(&play, &n);
  }
}

void player_kill(void)
{
  ready = 0;
  zz_kill(&play);
}
