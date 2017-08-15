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
  C->loop_sp = C->loops;
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
  P->tick = 0;
  P->mix_ptr = P->mix_buf + P->pcm_per_tick;

  return 0;
}

/* ---------------------------------------------------------------------- */

static inline
uint16_t loop_off(const chan_t * const C, const sequ_t * const seq)
{
  assert( seq - C->seq < 0x10000 );
  return (const int8_t *)seq - (const int8_t *)C->seq;
}

static inline
sequ_t * loop_seq(const chan_t * const C, const uint16_t off)
{
  return (sequ_t *)&off[(int8_t *)C->seq];
}

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
    if (!C->note.cur)
      C->note.cur = C->note.aim; /* safety net */
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

    case 'F':                           /* End-Voice */
      seq = C->seq;
      P->has_loop |= 1<<k;
      C->has_loop++;
      C->loop_sp = C->loops;            /* Safety net */
      break;

    case 'V':                           /* Voice-Change */
      C->curi = par >> 2;
      break;

    case 'P':                           /* Play-Note */
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

    case 'S':                       /* Slide-to-note */

      /* GB: This actually happen (e.g. Wrath of the demon - tune 1)
       *     I'm not sure what the original quartet player does.
       *     Here I'm just starting the goal note.
       */
      /* assert(C->note.cur); */
      if (!C->note.cur) {
        C->trig     = TRIG_NOTE;
        C->note.ins = P->vset.inst + C->curi;
        C->note.cur = stp;
      }
      C->wait     = len;
      C->note.aim = stp;
      C->note.stp = (int32_t)par;   /* sign extend, slide are signed */
      break;

    case 'R':                       /* Rest */
      C->trig     = TRIG_STOP;
      C->wait     = len;

      /* GB: Should I ? What if a slide happens after a rest ? It does
       *     not really make sense but technically it's
       *     possible. We'll see if it triggers the assert.
       *
       * GB: See note above. It happens so I shouldn't !
       */
      /* C->note.cur = 0; */
      break;

    case 'l':                       /* Set-Loop-Point */
      if (C->loop_sp < C->loops+MAX_LOOP) {
        struct loop_s * const l = C->loop_sp++;
        l->off = loop_off(C,seq);
        l->cnt = 0;
      } else {
        emsg("%c off:%u tick:%u -- loop stack overflow\n",
             'A'+k, (uint_t)(seq-C->seq-1), P->tick);
        return E_PLA;
      }
      break;

    case 'L':                       /* Loop-To-Point */
    {
      struct loop_s * l = C->loop_sp-1;

      if (l < C->loops) {
        ;
        C->loop_sp = (l = C->loops) + 1;
        l->cnt = 0;
        l->off = 0;
      }

      if (!l->cnt) {
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

        if ( loop_seq(C,l->off) <= C->sq0 && seq > C->sqN) {
          l->cnt = 1;
        } else {
          l->cnt = (par >> 16) + 1;
        }
      }

      if (--l->cnt) {
        seq = loop_seq(C,l->off);
      } else {
        --C->loop_sp;
        assert(C->loop_sp >= C->loops);
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
  ptr = P->mix_ptr;
  if (n > 0) {
    int have = P->mix_buf + P->pcm_per_tick - P->mix_ptr;

    if (!have) {
      /* refill mix_buf[] */
      ecode = zz_play_tick(P);
      if (ecode != E_OK)
        goto error;
      ecode = E_MIX;
      if (P->mixer && P->mixer->push(P))
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

int zz_measure(play_t * P)
{
  int ecode = E_OK;
  mixer_t * const save_mixer = P->mixer;

  if (!P->vset.bin) {
    /* If there is no voice set we still want to be able to measure
     * time. As the player will complain in case it tries to play a
     * non-existing instrument we just create fake a instruments set.
     */
    int i;
    dmsg("No voice set, assuming measuring only\n");
    P->vset.khz = P->song.khz;
    P->vset.nbi = 20;
    for (i=0; i<20; ++i) {
      P->vset.inst[i].len = 2;
      P->vset.inst[i].lpl = 0;
      P->vset.inst[i].end = 2;
      P->vset.inst[i].pcm = (uint8_t *)&P->vset.inst[i].pcm;
    }
  }

  P->mixer = &mixer_void;
  P->mix_ptr = 0;
  while(!P->done) {
    int n = P->pcm_per_tick;
    if (!zz_pull(P, &n)) {
      ecode = n;
      break;
    }
  }

  /* If everything went as planed and we were able to measure music
   * duration then set plat_t::end_detect and plat_t::max_ticks.
   */
  if (!ecode) {
    P->end_detect = P->tick <= P->max_ticks;
    if (P->end_detect)
      P->max_ticks = P->tick;
    P->done = 0;
    P->tick = 0;
  }
  P->mix_ptr = 0;
  P->mixer = save_mixer;
  if (!P->vset.bin)
    zz_memclr(&P->vset,sizeof(P->vset));

  return ecode;
}

/* ---------------------------------------------------------------------- */

int zz_init(play_t * P)
{
  int ecode = E_OK;

  if (!P->rate) {
    P->rate = 200;
    dmsg("replay rate not set -- default to %u\n", P->rate);
  }

  if (!P->spr) {
    P->spr = P->song.khz * 1000u;
    dmsg("sampling rate not set -- default to %u\n", P->spr);
  }

  /* ----------------------------------------
   *  Mix buffers
   * ---------------------------------------- */

  P->pcm_per_tick = divu(P->spr + (P->rate>>1), P->rate);
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

  bin_free(&P->vset.bin);
  bin_free(&P->song.bin);
  bin_free(&P->info.bin);

  return ecode;
}
