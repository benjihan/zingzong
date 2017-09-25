/**
 *
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

void player_init(bin_t * song, bin_t * vset)
{
  ready = 0;
  zz_memclr(&play,sizeof(play));

  /* Skip the headers */
  if (!song->ptr) song->ptr = song->_buf;
  play.song.bin = set_bin(&songbin, song, 16);
  if (!vset->ptr) vset->ptr = vset->_buf;
  play.vset.bin = set_bin(&vsetbin, vset, 222);

  zz_log_fun(0,0);
  zz_mem(newf,delf);

  ready =
    1
    && ! song_init_header(&play.song, song->ptr)
    && ! song_init(&play.song)
    && ( play.vset.iused = play.song.iused )
    && ! vset_init_header(&play.vset, vset->ptr)
    && ! vset_init(&play.vset)
    && ! zz_setup(&play, ZZ_DEFAULT_MIXER, 0, 0, 0, 0, 0)
    && ! zz_init(&play)
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
    zz_u16_t npcm = play.pcm_per_tick;
    int16_t * ptr;
    ready = !! ( ptr = zz_play(&play, &npcm) );
    if (!ready) {
      ecode = npcm;
      BREAKP;
    }
  }
}

