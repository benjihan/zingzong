/**
 * @file   zz_play.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet player.
 */

#include "zz_private.h"

/* ---------------------------------------------------------------------- */

void zz_chan_init(play_t * P, int k)
{
  chan_t * const C = P->chan+k;
  sequ_t * seq;
  int cmd;

  assert(k >= 0 && k < 4);
  zz_memclr(C,sizeof(*C));
  C->id  = 'A'+k;
  C->num = k;
  C->cur = C->seq = P->song.seq[k];
  for ( seq = C->cur; (cmd=u16(seq->cmd)) != 'F' ; ++seq ) {
    switch(cmd) {
    case 'P': case 'R': case 'S':
      /* Scoot first and last note sequence */
      if (!C->sq0) C->sq0 = seq;
      C->sqN = seq;
    }
  }
  C->end = seq;
  assert(C->sq0);
  assert(C->sqN);
}

/* ---------------------------------------------------------------------- */

int zz_play_init(play_t * const P)
{
  int k;

  for (k=0; k<4; ++k)
    zz_chan_init(P, k);
  P->has_loop = P->muted_voices;
  P->done = 0;
  P->mix_ptr = P->mix_buf;

  return 0;
}

/* ---------------------------------------------------------------------- */

int zz_play_chan(play_t * const P, const int k)
{
  chan_t * const C = P->chan+k;
  sequ_t * seq = C->cur;

  if ( P->muted_voices & (1<<k) )
    return E_OK;

  C->trig = TRIG_NOP;

  /* Portamento */
  if (C->note.stp) {
    assert(C->note.cur);
    C->trig = TRIG_SLIDE;
    C->note.cur += C->note.stp;
    if (C->note.stp > 0) {
      if (C->note.cur >= C->note.aim) {
        C->note.cur = C->note.aim;
        C->note.stp = 0;
      }
    } else if (C->note.cur <= C->note.aim) {
      C->note.cur = C->note.aim;
      C->note.stp = 0;
    }
  }

  if (C->wait) --C->wait;
  while (!C->wait) {
    /* This could be an endless loop on empty track but it should
     * have been checked earlier ! */
    uint_t const cmd = u16(seq->cmd);
    uint_t const len = u16(seq->len);
    uint_t const stp = u32(seq->stp);
    uint_t const par = u32(seq->par);
    ++seq;

    switch (cmd) {

    case 'F':                       /* End-Voice */
      seq = C->seq;
      P->has_loop |= 1<<k;
      C->has_loop++;
      C->loop_level = 0;            /* Safety net */
      break;

    case 'V':                       /* Voice-Change */
      C->curi = par >> 2;
      break;

    case 'P':                       /* Play-Note */
      if (C->curi < 0 || C->curi >= P->vset.nbi) {
        emsg("%c[%u]@%u: using invalid instrument number -- %d/%d\n",
             'A'+k, (uint_t)(seq-1-C->seq), P->tick,
             C->curi+1,P->vset.nbi);
        return E_SNG;
      }
      if (!P->vset.inst[C->curi].len) {
        emsg("%c[%u]@%u: using tainted instrument -- I#%02u\n",
             'A'+k, (uint_t)(seq-1-C->seq), P->tick,
             C->curi+1);
        return E_SET;
      }

      C->trig     = TRIG_NOTE;
      C->note.ins = P->vset.inst + C->curi;
      C->note.cur = C->note.aim = stp;
      C->note.stp = 0;
      C->wait     = len;

      /* triggered |= 1<<(k<<3); */
      break;

    case 'R':                       /* Rest */
      C->trig     = TRIG_STOP;
      C->wait     = len;
      C->note.cur = 0;

      /* GB: Should I ? What if a slide happens after a rest ? It does
       *     not really make sense but technically it's
       *     possible. We'll see if it triggers the assert.
       */
      break;

    case 'S':                       /* Slide-to-note */
      assert(C->note.cur);
      C->wait     = len;
      C->note.aim = stp;
      C->note.stp = (int32_t)par;   /* sign extend, slide are signed */
      break;

    case 'l':                       /* Set-Loop-Point */
      if (C->loop_level < MAX_LOOP) {
        const int l = C->loop_level++;
        C->loop[l].seq = seq;
        C->loop[l].cnt = 0;
      } else {
        emsg("%c off:%u tick:%u -- loop stack overflow\n",
             'A'+k, (uint_t)(seq-C->seq-1), P->tick);
        return E_PLA;
      }
      break;

    case 'L':                       /* Loop-To-Point */
    {
      int l = C->loop_level - 1;

      if (l < 0) {
        C->loop_level = 1;
        l = 0;
        C->loop[0].cnt = 0;
        C->loop[0].seq = C->seq;
      }

      if (!C->loop[l].cnt) {
        /* Start a new loop unless the loop point is the start of
         * the sequence and the next row is the end.
         *
         * This (not so) effectively removes useless loops on the
         * whole sequence that messed up the song duration.
         */

        /* if (seq >= C->sqN) */
        /*   dmsg("'%c: seq:%05u loop:%05u sq:[%05u..%05u]\n", */
        /*        'A'+k, */
        /*        seq - C->seq, */
        /*        C->loop[l].seq - C->seq, */
        /*        C->sq0 - C->seq, */
        /*        C->sqN - C->seq); */

        if (C->loop[l].seq <= C->sq0 && seq > C->sqN) {
          C->loop[l].cnt = 1;
        } else {
          C->loop[l].cnt = (par >> 16) + 1;
        }
      }

      if (--C->loop[l].cnt) {
        seq = C->loop[l].seq;
      } else {
        --C->loop_level;
        assert(C->loop_level >= 0);
      }


    } break;

    default:
      emsg("invalid seq-%c command #%04X\n",'A'+k,cmd);
      return E_PLA;
    } /* switch */
  } /* while !wait */
  C->cur = seq;

  return E_OK;
}

/* ---------------------------------------------------------------------- */
int zz_play_tick(play_t * const P)
{
  int ecode = E_OK, k;

  ++P->tick;
  for (k=0; k<4; ++k)
    if (E_OK != (ecode = zz_play_chan(P,k)))
      break;

  P->done =
    ( P->end_detect && P->has_loop == 15 ) ||
    ( P->max_ticks && P->tick > P->max_ticks );

  return ecode;
}

/* ---------------------------------------------------------------------- */

int zz_play(play_t * P)
{
  int ecode;
  assert(P);

  ecode = zz_play_init(P);
  if (ecode == E_OK) {
    for (;;) {
      int n;
      ecode = zz_play_tick(P);
      if (ecode || P->done)
        break;
      ecode = E_MIX;
      if (P->mixer->push(P))
        break;
      ecode = E_OUT;
      n = P->pcm_per_tick << 1;
      if (P->out->write(P->out, P->mix_buf, n) != n)
        break;
    }
  }

  return ecode;
}

/* ---------------------------------------------------------------------- */

int16_t * zz_pull(play_t * P, int * ptr_n)
{
  int16_t * ptr = 0;
  int n = *ptr_n;
  int ecode;

  if (!P->mix_ptr) {
    ecode = zz_play_init(P);
    if (ecode)
      goto error;
  }

  ecode = E_OK;
  assert(P->mix_ptr);
  if (n > 0) {
    int have = P->mix_buf + P->pcm_per_tick - P->mix_ptr;

    if (!have) {
      /* refill mix_buf[] */
      ecode = zz_play_tick(P);
      if (ecode != E_OK)
        goto error;
      ecode = E_MIX;
      if (P->mixer->push(P))
        goto error;
      P->mix_ptr = P->mix_buf;
      have = P->pcm_per_tick;
    }

    ptr = P->mix_ptr;
    if (n > have)
      n = have;
    P->mix_ptr += n;
  }

error:
  *ptr_n = ptr ? n : ecode;
  return ptr;
}

/* ---------------------------------------------------------------------- */

int zz_init(play_t * P)
{
  int ecode = E_OK;

  if (!P->rate)
    P->rate = 200;
  if (!P->spr)
    P->spr = P->song.khz * 1000u;

  /* ----------------------------------------
   *  Mix buffers
   * ---------------------------------------- */

  P->pcm_per_tick = (P->spr + (P->rate>>1)) / P->rate;
  dmsg("pcm per tick: %u (%ux%u+%u) %u:%u\n",
       P->pcm_per_tick,
       P->pcm_per_tick/MIXBLK,
       MIXBLK, P->pcm_per_tick%MIXBLK,
       P->spr, P->rate);

  ecode = E_PLA;
  if (P->pcm_per_tick < MIXBLK)
    emsg("not enough pcm per tick -- %d\n", P->pcm_per_tick);
  else if (!P->mixer)
    emsg("no mixer\n");
  else
    ecode = P->mixer->init(P);

  if (ecode)
    goto error;

  P->mix_ptr = 0;
  P->mix_buf = (int16_t *) zz_malloc("mix-buf",2*P->pcm_per_tick);
  if (!P->mix_buf) {
    ecode = E_SYS; goto error;
  }

error:
  return ecode;
}

/* ---------------------------------------------------------------------- */

int zz_kill(play_t * P)
{
  int ecode = E_OK;

  if (P->mixer) {
    P->mixer->free(P);
    P->mixer = 0;
  }

  if (P->out) {
    if (P->out->close(P->out))
      ecode = E_OUT;
    P->out = 0;
  }

  zz_free("pcm-buffer",&P->mix_buf);
  P->mix_ptr = 0;

  zz_strfree(&P->vseturi);
  zz_strfree(&P->songuri);
  zz_strfree(&P->infouri);
  zz_strfree(&P->waveuri);

  bin_free(&P->vset.bin);
  bin_free(&P->song.bin);
  bin_free(&P->info.bin);

  return ecode;
}
