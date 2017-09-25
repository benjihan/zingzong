/**
 * @file   mix_stf.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari STf mixer (YM-2149).
 */

#include "../zz_private.h"

static zz_err_t init_stf_cb(play_t * const P);
static void     free_stf_cb(play_t * const P);
static zz_err_t push_stf_cb(play_t * const P);

mixer_t * mixer_stf(mixer_t * const M)
{
  M->name = "ym2149";
  M->desc = "Atari ST via YM-2149";
  M->init = init_stf_cb;
  M->free = free_stf_cb;
  M->push = push_stf_cb;
  return M;
}

#include "ym10_pack.h"

static const struct {
  uint8_t  tcr, tdr;
  uint16_t frq;
}
timer_table[] =
{
  { 1, 0x98,  4042 },
  { 1, 0x7a,  5036 },
  { 1, 0x66,  6023 },
  { 1, 0x58,  6981 },
  { 1, 0x4c,  8084 },
  { 1, 0x44,  9035 },
  { 1, 0x3d, 10072 },
  { 1, 0x38, 10971 },
  { 1, 0x33, 12047 },
  { 1, 0x2f, 13072 },
  { 1, 0x2c, 13963 },
  { 1, 0x29, 14985 },
  { 1, 0x26, 16168 },
  { 1, 0x24, 17066 },
  { 1, 0x22, 18070 },
  { 1, 0x20, 19200 },
  { 1, 0x1e, 20480 }
};

static uint8_t Tpcm[1024*4];

static void init_replay_table(void)
{
  const uint8_t * pack = ym10_pack;
  uint8_t * table = Tpcm;

  do {
    uint8_t XY;

    XY = *pack++;
    *table++ = XY >> 4;
    *table++ = XY & 15;
    XY = *pack++;
    *table++ = XY >> 4;
    ++ table;
    *table++ = XY & 15;
    XY = *pack++;
    *table++ = XY >> 4;
    *table++ = XY & 15;
    ++ table;

  } while (pack < ym10_pack+sizeof(ym10_pack));

}

static zz_err_t push_stf_cb(play_t * const P)
{
  zz_assert( 0 );
  return E_MIX;
}

typedef void (* volatile interrupt_func)(void);

static zz_err_t init_stf_cb(play_t * const P)
{
  init_replay_table();

  zz_assert( 0 );
  return E_666;
}

static void free_stf_cb(play_t * const P)
{
  zz_assert( 0 );
}
