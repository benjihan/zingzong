/**
 * @file    mix_ata.h
 * @author  Benjamin Gerard AKA Ben^OVR
 * @date    2018-01-15
 * @brief   Atari mixer (common code).
 */

#ifndef MIX_ATA_H
#define MIX_ATA_H 1

typedef struct mix_fast_s fast_t;
/* !!! change in m68k_mix.S as well !!! */
struct mix_fast_s {
  uint8_t *end;			/*  0: sample end                   */
  uint8_t *cur;			/*  4: sample current pointer       */
  uint32_t xtp;			/*  8: read step (FP16)             */
  uint16_t dec;			/* 12: FP16 precision               */
  uint16_t lpl;			/* 14: Loop length (0:no-loop)      */
  chan_t  *chn;			/* 16: attached play channel        */
  uint8_t  usr[12];		/* 20: free space for driver to use */
};				/* 32: bytes                        */

typedef struct mix_fifo_s fifo_t;
typedef int16_t (*pb_play_t)(void);
typedef void	(*pb_stop_t)(void);
struct mix_fifo_s {
  pb_play_t pb_play;		       /* Playback start/run        */
  pb_stop_t pb_stop;		       /* Playback stop             */
  void *    pb_user;		       /* Playback cookie           */
  int16_t sz;			       /* FIFO buffer size          */
  int16_t ro;			       /* Previous read position    */
  int16_t rp;			       /* Current read position     */
  int16_t wp;			       /* Last write position       */
  int16_t i1,n1;		       /* Index and size of part #1 */
  int16_t i2,n2;		       /* Index and size of part #2 */
};

typedef struct mix_ata_s ata_t;
struct mix_ata_s {
  fifo_t   fifo;			/* Generic FIFO       */
  uint16_t step;			/* Step scale (pitch) */
  uint8_t  swap;			/* Xor L/R swap       */
  fast_t   fast[4];			/* Fast channel info  */
};

void play_ata(ata_t * ata, chan_t * chn, int16_t n);
void stop_ata(ata_t * ata);

#define init_ata(SIZE, STEP)				    \
  do {							    \
    M->ata.fifo.sz = SIZE;				    \
    M->ata.fifo.wp = -1;				    \
    M->ata.fifo.pb_play = pb_play;			    \
    M->ata.fifo.pb_stop = pb_stop;			    \
    M->ata.fifo.pb_user = M;				    \
    M->ata.step = STEP;					    \
    M->ata.swap = 0;					    \
  } while (0)

#undef FP
#define FP 16

#define DMA	((volatile uint8_t  *)0xFFFF8900)
#define DMAB(X) ((volatile uint8_t  *)0xFFFF8900)[X]
#define DMAW(X) *(volatile uint16_t *)(0xFFFF8900+(X))
#define MFP	((volatile uint8_t  *)0xFFFFFA00)

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
  DMA_CNTL_ON	= 0x01,
  DMA_CNTL_LOOP = 0x02,

  DMA_MODE_MONO	 = 0x80,
  DMA_MODE_16BIT = 0x40,

  DMA_TC_1TRK = 0x00,
  DMA_TC_2TRK = 0x01,
  DMA_TC_3TRK = 0x02,
  DMA_TC_4TRK = 0x03,

};

static inline
uint16_t enter_critical(void)
{
  uint16_t ipl;
  asm volatile (
    "  move.w  %%sr,%[ipl]   \n"
    "  ori     #0x0700,%%sr  \n\t"
    : [ipl] "=md" (ipl));
  return ipl;
}

static inline
void leave_critical(uint16_t ipl)
{
  asm volatile (
    "  move %[ipl],%%sr  \n\t"
    :
    : [ipl] "imd" (ipl));
}

static inline
void dma8_write_ptr(volatile uint8_t * reg, const void * adr)
{
  asm volatile (
    "   swap %[adr]              \n"
    "   move.b %[adr],(%[hw])    \n"
    "   swap %[adr]              \n"
    "   movep.w %[adr],2(%[hw])  \n\t"
    :
    : [hw] "a" (reg), [adr] "d" (adr)
    : "cc");
}

static inline
void dma8_stop(void)
{
  DMA[DMA_CNTL] = 0;
}

static inline
void dma8_start(void)
{
  DMA[DMA_CNTL] = DMA_CNTL_ON|DMA_CNTL_LOOP;
}

static inline
int16_t dma8_running(void)
{
  return (DMA[DMA_CNTL] & (DMA_CNTL_ON|DMA_CNTL_LOOP))
    == (DMA_CNTL_ON|DMA_CNTL_LOOP);
}

ZZ_EXTERN_C
void dma8_setup(void * adr, void * end, uint16_t mode);

ZZ_EXTERN_C
void * dma8_position(void);

#endif
