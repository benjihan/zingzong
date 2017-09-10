/*
  player.c

  COPYRIGHT (C) 2014-2017 Benjamin Gerard AKA Ben/OVR

  ----------------------------------------------------------------------
  player header

  void player_init(void);
  void player_play(void);
  void player_kill(void);
  ----------------------------------------------------------------------
*/

#include "../zz_private.h"

int song_init_header(zz_song_t const song, const void * hd);
int song_init(zz_song_t const song);
int vset_init_header(zz_vset_t const vset, const void * hd);
int vset_init(zz_vset_t const vset);

/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59946
 *
 * To avoid this problem we force the player not to access any static
 * variables. Everything will be addressed relative to the bear
 * struct.
 */

static play_t play;
static bin_t songbin, vsetbin;
static volatile zz_err_t ready, ecode;

static void set_bin(bin_t * bin, void * data, int32_t len)
{
  bin->ptr = data;
  bin->len = bin->max = len;
}

static void* newf(zz_u32_t n) {
  return (void *) 0xdeadbeef;
}
static void  delf(void * p)   {  }

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

  zz_log_fun(0,0);
  zz_mem(newf,delf);

  ready =
    1
    && ! song_init_header(&play.song, p.song)
    && ! song_init(&play.song)
    && ( play.vset.iused = play.song.iused )
    && ! vset_init_header(&play.vset, p.vset)
    && ! vset_init(&play.vset)
    && ! zz_setup(&play, ZZ_DEFAULT_MIXER, 0, 0, 0, 0)
    && ! zz_init(&play)
    ;
}

void player_play(void)
{
  if (ready) {
    zz_u16_t npcm = play.pcm_per_tick;
    int16_t * ptr;
    ready = !! ( ptr = zz_play(&play, &npcm) );
    if (!ready)
      ecode = npcm;
  }
}

void player_kill(void)
{
  ready = 0;
  zz_close(&play);
}
