/**
 * @file   mix_ata.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-08-28
 * @brief  Atari mixer (common code).
 */


#include "../zz_private.h"
#include "mix_ata.h"

static void ata_trig(chan_t * restrict C, fast_t * restrict K, uint16_t scl)
{
  chan_t * const E = C+4;

  for (; C<E; ++C, ++K) {
    const u8_t trig = C->trig;

    C->trig = TRIG_NOP;
    switch (trig) {
    case TRIG_NOTE:
      zz_assert(C->note.ins);
      K->cur = C->note.ins->pcm;
      K->end = K->cur + C->note.ins->len;
      K->dec = 0;
      K->lpl = C->note.ins->lpl;

    case TRIG_SLIDE:
      K->xtp = mulu(scl, C->note.cur>>4) >> 8;
      break;
    case TRIG_STOP: K->cur = 0;
    case TRIG_NOP:  break;
    default:
      zz_assert(!"wtf");
    }
  }
}

static void fifo_play(fifo_t * const F, int16_t n)
{
  register int16_t i1,n1,i2,n2,rp;

  zz_assert( n+n <= F->sz );

  if ( unlikely(F->wp == -1) ) {
    F->rp = rp = i1 = 0;                /* F->rp is F->ro */
  } else {
    rp = F->pb_play();
    i1 = F->wp;
  }

  zz_assert( rp >= 0 );
  zz_assert( rp < F->sz );
  zz_assert( i1 >= 0 );
  zz_assert( i1 < F->sz );

  n1 = F->rp - i1;
  if (n1 <= 0) n1 += F->sz;             /* n1 = free */
  if (n1 >  n) n1 = n;                  /* n1 = min(want,free) */

  n2 = 0;
  i2 = i1 + n1;
  if ( unlikely(i2 >= F->sz) ) {      /* larger FIFO => less likely */
    n2 = i2 - F->sz;
    i2 = 0;
    n1 -= n2;
  }

  F->ro = F->rp;
  F->rp = rp;
  F->i1 = i1;
  F->n1 = n1;
  F->wp = (F->i2 = i2) + (F->n2 = n2);

  if (1) {
    int16_t read = F->rp - F->ro;
    if (read < 0) read += F->sz;
    dmsg("XXX: "
         "+%-3lu R:%-3lu W:%-3lu %3lu:%-3lu %3lu:%-3lu %3lu/%3lu/%3lu\n",
         LU(read), LU(F->rp), LU(F->wp),
         LU(F->i1), LU(F->n1),
         LU(F->i2), LU(F->n2),
         LU(F->n1+F->n2), LU(n), LU(F->sz)
    );
  }

  zz_assert( F->wp >= 0 );
  zz_assert( F->wp < F->sz );
  zz_assert( F->i1 >= 0 );
  zz_assert( F->i2 >= 0 );
  zz_assert( F->i1+F->n1 <= F->sz );
  zz_assert( F->i2+F->n2 <= F->sz );
  zz_assert( n <= F->sz );
  zz_assert( F->n1 + F->n2 <= n );
  zz_assert( F->n1 <= n );
  zz_assert( F->n2 <= n );
  zz_assert( F->i2 == 0 || F->n2 == 0);
}

void play_ata(ata_t * restrict ata, chan_t * restrict C, int16_t n)
{
  ata_trig(C, ata->fast, ata->step);
  fifo_play(&ata->fifo, n);
}

void stop_ata(ata_t * ata)
{
  ata->fifo.pb_stop();
}
