/**
 * @file   zz_play.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Quartet player.
 */

#define ZZ_DBG_PREFIX "(pla) "
#include "zz_private.h"



static zz_u32_t never_inline
ms_to_ticks(zz_u32_t ms, zz_u16_t rate)
{
  zz_u32_t ticks;
  if (!ms)
    return 0;                           /* trivial no other check */

  if (!rate) {
    rate = RATE_DEF;
    wmsg("rate not set -- assuming %huhz -- %lu ms\n", HU(rate), LU(ms));
  }

  zz_assert( rate >= RATE_MIN );
  zz_assert( rate <= RATE_MAX );
  if (rate < RATE_MIN || rate > RATE_MAX) {
    wmsg("rate out of range -- %huhz\n", HU(rate));
    return 0;
  }

  /* TICKS = MS x RATE / 1000 */
  switch (rate) {
  case 50:                              /* div 20 */
    ms >>= 2;                           /* div 4 */
  case 200:                             /* div 5 */
    ticks = divu32(ms,5);
    break;
  default: {
    zz_u16_t r = rate, d = 1000;
    while ( ! ( (r|d) & 1 ) ) {
      r >>= 1; d >>= 1;
    }
    ticks = divu32(mulu32(ms,r), d);
#ifndef SC68                            /* $$$ GB: FIXME */
    zz_assert ( ticks == ms * rate / 1000u );
#endif
  } break;
  }

  return ticks;
}


/**
 * @retval ticks*1000/rate
 */
static zz_u32_t never_inline
tick_to_ms(zz_u32_t ticks, zz_u16_t rate)
{
  zz_u32_t ms, acu;              /* TEMP TO TEST OVERFLOW ON 32 bit */

  if (!ticks)
    return 0;                           /* trivial no other check */

  if (!rate) {
    rate = RATE_DEF;
    wmsg("rate not set -- assuming %hu-hz -- %lu-ticks\n",
         HU(rate),LU(ticks));
  }
  ms = ticks;

  switch ( rate ) {
  case 50:                              /* x20 */
    ms <<= 2;
  case 200:                             /* x5 */
    ms += (ms << 2);
    break;
  default:
    acu  = ms <<= 3;                    /* ms = x8  acu = x8 */
    acu += ms += ms;                    /* ms = x16 acu = x24 */
    ms <<= 6;                           /* ms = x1024 */
    ms -= acu;                          /* ms = x1000 */
  }

  /* dmsg("ticks: %lu rate:%hu ms:%lu == %lu\n", */
  /*      LU(ticks),HU(rate),LU(ms), */
  /*      LU(ticks*1000ull/rate)); */

#ifndef SC68                            /* $$$ GB: FIXME  */
  zz_assert ( ms == ( ticks*1000ull / rate ) );
#endif
  return ms;
}

zz_err_t zz_setup(zz_play_t P, zz_u8_t mixerid,
                  zz_u32_t spr, zz_u16_t rate,
                  zz_u32_t max_ticks, zz_u32_t max_ms, zz_u8_t end_detect)
{
  if ( zz_mixer_set(P, mixerid) == ZZ_DEFAULT_MIXER )
    return ZZ_EARG;
  if (!P)
    return ZZ_OK;

  zz_assert( ! rate );                  /* XXX $$$ TEMP to catch */

  if (!rate) rate = P->song.rate ? P->song.rate : RATE_DEF;
  if (!spr)  spr = mulu(P->song.khz,1000);
  if (!spr)  spr = SPR_DEF;

  if (rate < RATE_MIN || rate > RATE_MAX || spr < SPR_MIN || spr > SPR_MAX)
    return ZZ_EARG;

  P->rate       = rate;
  P->spr        = spr;

  if (!max_ticks && max_ms) {
    max_ticks = ms_to_ticks(max_ms, rate);
    dmsg("setup: %lu-ms@%hu-hz -> %lu-ticks\n",
         LU(max_ms), HU(rate), LU(max_ticks));
  }

  P->max_ticks  = max_ticks;
  P->end_detect = !!end_detect;

  dmsg("setup: rate:%hu spr:%lu max-ticks:%lu end-detect:%s\n",
       HU(P->rate), LU(P->spr),
       LU(P->max_ticks), P->end_detect?"yes":"no" );

  return ZZ_OK;
}

/* ---------------------------------------------------------------------- */

void
zz_chan_init(play_t * P, int k)
{
  chan_t * const C = P->chan+k;
  sequ_t * seq;
  int cmd;

  zz_assert(k >= 0 && k < 4);
  zz_memclr(C,sizeof(*C));
  C->id  = 'A'+k;
  C->num = k;
  C->cur = C->seq = P->song.seq[k];
  for ( seq = C->cur; (cmd=U16(seq->cmd)) != 'F' ; ++seq ) {
    switch(cmd) {
    case 'P': case 'R': case 'S':
      /* Scoot first and last note sequence */
      if (!C->sq0) C->sq0 = seq;
      C->sqN = seq;
    }
  }
  C->loop_sp = C->loops;
  C->end = seq;
  zz_assert(C->sq0);
  zz_assert(C->sqN);
}

/* ---------------------------------------------------------------------- */

zz_err_t zz_play_init(play_t * const P)
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

/* Offset between 2 sequences (should be multiple of 12). */
static inline
u16_t ptr_off(const sequ_t * const a, const sequ_t * const b)
{
  const u32_t off = (const int8_t *) b - (const int8_t *) a;
  zz_assert( a <= b );
  zz_assert( off < 0x10000 );
  return off;
}

/* Index (position) of seq in this channel */
static inline
u16_t seq_idx(const chan_t * const C, const sequ_t * const seq)
{
  zz_assert (sizeof(sequ_t) == 12u);
  return divu(ptr_off(C->seq,seq),sizeof(sequ_t));
}

/* Offset of seq in this channel */
static inline
u16_t seq_off(const chan_t * const C, const sequ_t * const seq)
{
  return ptr_off(C->seq, seq);
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
    zz_assert(C->note.cur);
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
    u32_t const cmd = U16(seq->cmd);
    u32_t const len = U16(seq->len);
    u32_t const stp = U32(seq->stp);
    u32_t const par = U32(seq->par);
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
        dmsg("%c[%hu]@%lu: using invalid instrument number -- %hu/%hu\n",
             C->id, HU(seq_idx(C,seq-1)), LU(P->tick),
             HU(C->curi+1),HU(P->vset.nbi));
        return E_SNG;
      }
      if (!P->vset.inst[C->curi].len) {
        dmsg("%c[%hu]@%lu: using tainted instrument -- I#%02hu\n",
             C->id, HU(seq_idx(C,seq-1)), LU(P->tick),
             HU(C->curi+1));
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
      /*zz_assert(C->note.cur); */
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
       *     possible. We'll see if it triggers thezz_assert.
       *
       * GB: See note above. It happens so I shouldn't !
       */
      /* C->note.cur = 0; */
      break;

    case 'l':                       /* Set-Loop-Point */
      if (C->loop_sp < C->loops+MAX_LOOP) {
        struct loop_s * const l = C->loop_sp++;
        l->off = seq_off(C,seq);
        l->cnt = 0;
      } else {
        dmsg("%c[%hu]@%lu -- loop stack overflow\n",
             C->id, HU(seq_idx(C,seq-1)), LU(P->tick));
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
        zz_assert(C->loop_sp >= C->loops);
      }


    } break;

    default:
      dmsg("%c[%hu]@%lu command <%c> #%04hX\n",
           C->id, HU(seq_idx(C,seq-1)), LU(P->tick),
           isalpha(cmd) ? (char)cmd : '^', HU(cmd));
      return E_PLA;
    } /* switch */
  } /* while !wait */
  C->cur = seq;

  return E_OK;
}

/* ---------------------------------------------------------------------- */
zz_err_t
zz_tick(play_t * const P)
{
  zz_err_t ecode;
  u8_t k;

  ++P->tick;
  for (k=0; k<4; ++k)
    if (E_OK != (ecode = zz_play_chan(P,k)))
      break;

  P->done |= 0
    | ( 0x01 & -(P->end_detect && P->has_loop == 15) )
    | ( 0x02 & -(P->max_ticks && P->tick > P->max_ticks) )
    ;

  if (ecode != E_OK) {
    const chan_t * const C = P->chan+k;
    P->done |= 0x10 << k;
    (void)C;
    emsg("play: %c[%hu]@%lu: failed\n",
         C->id, HU(seq_idx(C,C->cur)), LU(P->tick));
  }

  return ecode;
}

/* ---------------------------------------------------------------------- */

int16_t * zz_play(play_t * P, zz_u16_t * ptr_n)
{
  int16_t * ptr = 0;
  zz_u16_t n = *ptr_n;
  zz_err_t ecode;

  zz_assert(P);
  zz_assert(ptr_n);

  if (!P->mix_buf) {
    emsg("play: not initialized\n");
    *ptr_n = E_PLA;
    return 0;
  }

  if (!P->mix_ptr) {
    dmsg("play: rate:%huhz spr:%luhz pcm:%hu ticks:%lu end:%hu mute:%hX\n",
         HU(P->rate), LU(P->spr), HU(P->pcm_per_tick),
         LU(P->max_ticks),HU(P->end_detect), HU(P->muted_voices));
    ecode = zz_play_init(P);
    if (ecode)
      goto error;
  }
  ecode = E_OK;

  zz_assert( P->mix_ptr );
  if (!n)
    n = P->pcm_per_tick;
  if (n > 0) {
    zz_u16_t have = P->mix_buf + P->pcm_per_tick - P->mix_ptr;

    if (!have) {
      /* refill mix_buf[] */
      ecode = zz_tick(P);
      if (ecode != E_OK)
        goto error;
      if (P->done) {
        n = 0;
        goto error;
      }

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


zz_err_t
zz_measure(play_t * P, zz_u32_t * restrict pticks, zz_u32_t * restrict pms)
{
  mixer_t * const save_mixer = P->mixer;
  zz_err_t ecode = E_OK;
  zz_u32_t ticks = pticks ? *pticks : 0;
  zz_u32_t save_max_ticks = P->max_ticks;

  if (!P->rate && P->song.rate) {
    P->rate = P->song.rate;
    dmsg("set tick-rate to song default -- %huhz\n", HU(P->rate));
  }

  if (!P->vset.bin) {
    /* If there is no voice set we still want to be able to measure
     * time. As the player will complain in case it tries to play a
     * non-existing instrument we just create fake a instruments set.
     */
    zz_u8_t i;
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

  if (!ticks && pms && *pms) {
    ticks = ms_to_ticks(*pms, P->rate);
    dmsg("measure: set current max to -- %lu ms\n", LU(*pms));
  }

  if (ticks) {
    dmsg("measure: set current max to -- %lu ticks\n", LU(ticks));
    P->max_ticks = ticks;
  }

  P->mixer = 0;                         /* Remove mixer */
  P->mix_ptr = 0;                       /* Reset player */
  while (!P->done) {
    zz_u16_t n = 0;                     /* play whole buffer */
    if (!zz_play(P, &n)) {
      ecode = n;
      break;
    }
    zz_assert ( n > 0 || P->done );
  }
  dmsg("measure loop ended [done:%hu code:%hu ticks:%lu/%lu\n",
       HU(P->done), HU(ecode), LU(P->tick), LU(P->max_ticks));

  /* If everything went as planed and we were able to measure music
   * duration then set play_t::end_detect and play_t::max_ticks.
   */
  P->end_detect = !ecode && (P->tick <= P->max_ticks);
  if (!P->end_detect) {
    P->max_ticks = save_max_ticks;
    ticks = 0;
  }
  else {
    ticks = P->max_ticks = P->tick;
  }

  P->mix_ptr = 0;
  P->mixer = save_mixer;
  if (!P->vset.bin)
    P->vset.nbi = 0;

  if (pticks)
    *pticks = ticks;
  if (pms)
    *pms = tick_to_ms(ticks, P->rate);

  dmsg("measured (%s) -> ticks:%lu ms:%lu\n",
        ecode ? "ERR" : P->end_detect ? "OK" : "NOEND",
        LU(ticks), LU(pms?*pms:0));
  return ecode;
}

/* ---------------------------------------------------------------------- */

static void zz_rt_check(void)
{
#ifndef NDEBUG
  static uint8_t bytes[4] = { 1, 2, 3, 4 };
#endif

  /* constant check */
  zz_assert( ZZ_FORMAT_UNKNOWN == 0 );
  zz_assert( ZZ_OK == 0 );
  zz_assert( ZZ_DEFAULT_MIXER > 0 );

  /* built-in type check */
  zz_assert( sizeof(uint8_t)  == 1 );
  zz_assert( sizeof(uint16_t) == 2 );
  zz_assert( sizeof(uint32_t) == 4 );

  zz_assert( sizeof(zz_u8_t)  >= 1 );
  zz_assert( sizeof(zz_u16_t) >= 2 );
  zz_assert( sizeof(zz_u32_t) >= 4 );

  /* compound type check */
  zz_assert( sizeof(struct sequ_s) == 12 );

  /* byte order stuff */
  zz_assert( U32(bytes)   == 0x01020304 );
  zz_assert( U16(bytes)   == 0x00000102 );
  zz_assert( U16(bytes+2) == 0x00000304 );
}

zz_err_t
zz_init(play_t * P)
{
  zz_err_t ecode = E_OK;

  zz_rt_check();

  zz_assert ( ! P->mix_buf );
  zz_assert ( ! P->mixer_data );
  zz_assert ( P->mixer );

  if (!P->rate && P->song.rate) {
    P->rate = P->song.rate;
    dmsg("set tick-rate to song default -- %huhz\n", HU(P->rate));
  }

  if (!P->rate) {
      P->rate = P->song.rate ? P->song.rate :RATE_DEF;
      dmsg("replay rate not set to %sdefault %hu\n",
           P->song.rate ? "song ":"", HU(P->rate));
  }

  if (!P->spr) {
    P->spr = P->song.khz * 1000u;
    dmsg("sampling rate not set -- default to %lu\n", LU(P->spr));
  }

  /* ----------------------------------------
   *  Mix buffers
   * ---------------------------------------- */

  P->pcm_per_tick = divu(P->spr + (P->rate>>1), P->rate);
  dmsg("pcm per tick: %lu (%lux%lu+%lu) %lu:%lu\n",
       LU(P->pcm_per_tick),
       LU(divu32(P->pcm_per_tick,MIXBLK)),
       LU(MIXBLK), LU(P->pcm_per_tick%MIXBLK), /* GB: $$$ no ggod for m68k */
       LU(P->spr), LU(P->rate));

  ecode = E_PLA;
  if (P->pcm_per_tick < MIXBLK)
    emsg("not enough pcm per tick -- %lu\n", LU(P->pcm_per_tick));
  else if (!P->mixer)
    emsg("no mixer\n");
  else
    ecode = P->mixer->init(P);

  if (ecode)
    goto error;

  P->mix_ptr = 0;
  ecode = zz_memnew(&P->mix_buf, 2*P->pcm_per_tick, 0);

error:
  return ecode;
}

/* ---------------------------------------------------------------------- */

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

const char * zz_version(void)
{
  return PACKAGE_STRING;
}

zz_u32_t zz_position(zz_play_t P, zz_u32_t * const pms)
{
  zz_u32_t tick = 0, ms = 0;

  if (P) {
    tick = P->tick;
    if (pms)
      ms = tick_to_ms(tick, P->rate);
  }

  if (pms)
    *pms = ms;

  return tick;
}

static void memb_free(struct memb_s * memb)
{
  if (memb && memb->bin)
    bin_free(&memb->bin);
}

static void memb_wipe(struct memb_s * memb, int size)
{
  if (memb) {
    memb_free(memb);
    zz_memclr(memb,size);
  }
}

void zz_song_wipe(song_t * song)
{
  memb_wipe((struct memb_s *)song, sizeof(*song));
}

void zz_vset_wipe(vset_t * vset)
{
  memb_wipe((struct memb_s *)vset, sizeof(*vset));
}

void zz_info_wipe(info_t * info)
{
  memb_wipe((struct memb_s *)info, sizeof(*info));
}

zz_err_t zz_close(zz_play_t P)
{
  zz_err_t ecode = E_ARG;

  if (P) {
    if (P->mixer) {
      P->mixer->free(P);
      P->mixer = 0;
    }
    zz_assert ( ! P->mixer_data ); /* miser::free() should have clean that */
    P->mixer_data = 0;

    P->mix_ptr = 0;
    zz_memdel(&P->mix_buf);
    zz_assert( !P->mix_buf );

    zz_song_wipe(&P->song);
    zz_vset_wipe(&P->vset);
    zz_info_wipe(&P->info);

    zz_strdel(&P->songuri);
    zz_strdel(&P->vseturi);
    zz_strdel(&P->infouri);

    P->st_idx = 0;
    P->pcm_per_tick = 0;
    P->format = ZZ_FORMAT_UNKNOWN;

    /* GB: $$$ XXX FIXME might have some more stuff to clear. */

    ecode = E_OK;
  }
  return ecode;
}

void zz_del(zz_play_t * pP)
{
  zz_assert( pP );
  if (pP && *pP) {
    zz_close(*pP);
    zz_free(pP);
  }
}

zz_err_t zz_new(zz_play_t * pP)
{
  return zz_calloc(pP,sizeof(**pP));
}


#ifdef ZZ_MINIMAL

zz_err_t zz_info( zz_play_t P, zz_info_t * pinfo)
{
  return E_ERR;
}

#else

static char empty_str[] = "";
#define NEVER_NIL(S) if ( (S) ) {} else (S) = empty_str

const char * zz_formatstr(zz_u8_t fmt)
{
  switch ( fmt )
  {
  case ZZ_FORMAT_UNKNOWN: return "unknown";
  case ZZ_FORMAT_4V:      return "4v";
  case ZZ_FORMAT_4Q:      return "4q";
  case ZZ_FORMAT_QUAR:    return "quar";
  }
  zz_assert( ! "unexpecter format" );
  return "?";
}

zz_err_t zz_info( zz_play_t P, zz_info_t * pinfo)
{
  zz_assert(pinfo);
  if (!pinfo)
    return E_ARG;

  if (!P || P->format == ZZ_FORMAT_UNKNOWN) {
    dmsg("clear info (%s)\n", !P ? "nil" : "empty");
    zz_memclr(pinfo,sizeof(*pinfo));
    pinfo->fmt.str = zz_formatstr(pinfo->fmt.num);
  } else {
    dmsg("set info from <%s> \"%s\"\n",
         zz_formatstr(P->format),
         ZZSTR_NOTNIL(P->infouri));
    /* format */
    pinfo->fmt.num = P->format;
    pinfo->fmt.str = zz_formatstr(pinfo->fmt.num);

    /* rates */
    pinfo->len.rate  = P->rate = P->rate ? P->rate : P->song.rate;
    pinfo->mix.spr   = P->spr;
    pinfo->len.ticks = P->max_ticks;
    pinfo->len.ms    = tick_to_ms(pinfo->len.ticks, pinfo->len.rate);

    /* mixer */
    pinfo->mix.ppt = P->pcm_per_tick;
    pinfo->mix.num = P->mixer_id;
    if (P->mixer) {
      pinfo->mix.name = P->mixer->name;
      pinfo->mix.desc = P->mixer->desc;
    } else {
      zz_mixer_enum(pinfo->mix.num, &pinfo->mix.name, &pinfo->mix.desc);
    }

    pinfo->sng.uri = ZZSTR_SAFE(P->songuri);
    pinfo->set.uri = ZZSTR_SAFE(P->vseturi);
    pinfo->sng.khz = P->song.khz;
    pinfo->set.khz = P->vset.khz;

    /* meta-tags */
    pinfo->tag.album   = P->info.album;
    pinfo->tag.title   = P->info.title;
    pinfo->tag.artist  = P->info.artist;
    pinfo->tag.ripper  = P->info.ripper;
  }

  /* Ensure no strings are nil */
  NEVER_NIL(pinfo->fmt.str);
  NEVER_NIL(pinfo->mix.name);
  NEVER_NIL(pinfo->mix.desc);
  NEVER_NIL(pinfo->set.uri);
  NEVER_NIL(pinfo->sng.uri);
  NEVER_NIL(pinfo->tag.album);
  NEVER_NIL(pinfo->tag.title);
  NEVER_NIL(pinfo->tag.artist);
  NEVER_NIL(pinfo->tag.ripper);

  return E_OK;
}

#endif

#ifndef NO_VFS

zz_err_t zz_vfs_add(zz_vfs_dri_t dri)
{
  return vfs_register(dri);
}

zz_err_t zz_vfs_del(zz_vfs_dri_t dri)
{
  return vfs_unregister(dri);
}

#endif
