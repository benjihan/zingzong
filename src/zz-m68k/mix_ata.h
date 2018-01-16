/**
 * @file   mix_ata.h
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2018-01-15
 * @brief  Atari mixer (common code).
 */

#ifndef MIX_ATA_H
#define MIX_ATA_H 1

typedef struct mix_fast_s fast_t;
/* !!! change in m68k_mix.S as well !!! */
struct mix_fast_s {
  uint8_t *end;                         /*  0 */
  uint8_t *cur;                         /*  4 */
  uint32_t xtp;                         /*  8 */
  uint16_t dec;                         /* 12 */
  uint16_t lpl;                         /* 14 */
  uint8_t  usr[16];                     /* 16 */
};                                      /* 32 bytes */

typedef struct mix_fifo_s fifo_t;
typedef int16_t (*pb_play_t)(void);
typedef void    (*pb_stop_t)(void);
struct mix_fifo_s {
  pb_play_t pb_play;             /*  0  playback start/run */
  pb_stop_t pb_stop;             /*  4  playback stop      */
  void *    pb_user;             /*  8  playback cookie    */
  int16_t sz;                    /* 12  FIFO buffer size */
  int16_t ro;                    /* 14  previous read position */
  int16_t rp;                    /* 16  current read position  */
  int16_t wp;                    /* 18  last write position */
  int16_t i1,n1;                 /* 20,22 index and size of part #1 */
  int16_t i2,n2;                 /* 24,26 index and size of part #2 */
};

typedef struct mix_ata_s ata_t;
struct mix_ata_s {
  fifo_t   fifo;                        /* Generic FIFO */
  uint16_t step;                        /* step scale (pitch) */
  fast_t   fast[4];                     /* Fast channel info  */
};

void play_ata(ata_t * ata, chan_t * C, int16_t n);
void stop_ata(ata_t * ata);

#define init_ata(SIZE, STEP)                                \
  do {                                                      \
    M->ata.fifo.sz = SIZE;                                  \
    M->ata.fifo.wp = -1;                                    \
    M->ata.fifo.pb_play = pb_play;                          \
    M->ata.fifo.pb_stop = pb_stop;                          \
    M->ata.fifo.pb_user = M;                                \
    M->ata.step = STEP;                                     \
  } while (0)

#undef FP
#define FP 16

#endif

