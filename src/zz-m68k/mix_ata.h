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
  chan_t  *chn;                         /* 16 */
  uint8_t  usr[12];                     /* 20 */
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
  uint16_t step;                        /* Step scale (pitch) */
  fast_t   fast[4];                     /* Fast channel info */
};

void play_ata(ata_t * ata, int16_t n);
void stop_ata(ata_t * ata);

#define init_ata(SIZE, STEP, MI)                            \
  do {                                                      \
    int16_t i;                                              \
    M->ata.fifo.sz = SIZE;                                  \
    M->ata.fifo.wp = -1;                                    \
    M->ata.fifo.pb_play = pb_play;                          \
    M->ata.fifo.pb_stop = pb_stop;                          \
    M->ata.fifo.pb_user = M;                                \
    M->ata.step = STEP;                                     \
    for(i=0; i<4; ++i)                                      \
      M->ata.fast[i].chn = &P->chan[3&(i+MI)];              \
  } while (0)

#undef FP
#define FP 16

#define DMA     ((volatile uint8_t  * )0xFFFF8900)
#define DMAB(X) ((volatile uint8_t  * )0xFFFF8900)[X]
#define DMAW(X) *(volatile uint16_t *)(0xFFFF8900+(X))

enum {
  /* STE Sound DMA registers map. */

  DMAW_CNTL = 0x00,
  DMA_CNTL  = 0x01,
  /**/
  DMA_ADR   = 0x03,
  DMA_ADR_H = 0x03,
  DMA_ADR_M = 0x05,
  DMA_ADR_L = 0x07,
  /**/
  DMA_CNT   = 0x09,
  DMA_CNT_H = 0x09,
  DMA_CNT_M = 0x0B,
  DMA_CNT_L = 0x0D,
  /**/
  DMA_END   = 0x0F,
  DMA_END_H = 0x0F,
  DMA_END_M = 0x11,
  DMA_END_L = 0x13,
  /**/
  DMAW_MODE = 0x20,
  DMA_MODE  = 0x21,

  /* Constants */
  DMA_CNTL_ON   = 0x01,
  DMA_CNTL_LOOP = 0x02,

  DMA_MODE_MONO  = 0x80,
  DMA_MODE_16BIT = 0x40,

  DMA_TC_1TRK = 0x00,
  DMA_TC_2TRK = 0x01,
  DMA_TC_3TRK = 0x02,
  DMA_TC_4TRK = 0x03,

};

static inline
void * dma_position(void)
{
  void * pos;
  asm volatile(
    "   lea 0x8909.w,%%a0      \n"
    "   moveq #-1,%[pos]       \n"
    "0:"
    "   move.l %[pos],%%d1     \n"
    "   moveq #0,%[pos]        \n"
    "   move.b (%%a0),%[pos]   \n"
    "   swap %[pos]            \n"
    "   movep.w 2(%%a0),%[pos] \n"
    "   cmp.l %[pos],%%d1      \n"
    "   bne.s 0b               \n\t"
    : [pos] "=d" (pos)
    :
    : "cc","d1","a0");
  return pos;
}

static inline
void dma_write_ptr(volatile uint8_t * reg, const void * adr)
{
  asm volatile(
    "   swap %[adr]              \n"
    "   move.b %[adr],(%[hw])    \n"
    "   swap %[adr]              \n"
    "   movep.w %[adr],2(%[hw])  \n\t"
    :
    : [hw] "a" (reg), [adr] "d" (adr)
    : "cc");
}

static inline
void dma_stop(void)
{
  DMAW(DMAW_CNTL) = 0;
  DMAW(DMAW_MODE) = 0;
}

static inline
void dma_start(void * adr, void * end, uint16_t mode)
{
  dma_stop();
  dma_write_ptr(DMA+DMA_ADR, adr);
  dma_write_ptr(DMA+DMA_END, end);
  DMAW(DMAW_MODE) = mode;
  DMAW(DMAW_CNTL) = DMA_CNTL_ON|DMA_CNTL_LOOP;
}

#endif
