/**
 * @file   zz_play.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  Quartet player.
 */

#define ZZ_DBG_PREFIX "(pla) "
#include "zz_private.h"

/* ---------------------------------------------------------------------- */

static void chan_init(play_t * const P, u8_t k)
{
  chan_t * const C = P->chan+k;

  zz_memclr(C,sizeof(*C));              /* bad boy */
  C->msk = 0x11 << k;                   /* channel bit duplicated */
  C->num = k;
  C->cur = C->seq = P->song.seq[k];
  C->loop_sp = C->loops;
}

/* ---------------------------------------------------------------------- */

static zz_err_t play_init(play_t * const P)
{
  u8_t k;

  for (k=0; k<4; ++k)
    chan_init(P, k);
  P->has_loop = 0x0F & P->muted_voices; /* set ignored voices */
  P->done = 0;
  P->tick = 0;
  P->ms_pos = 0;
  P->ms_end = 0;
  P->ms_err = 0;
  P->pcm_cnt = 0;
  P->pcm_err = 0;

  return P->code = ZZ_OK;
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

#ifndef NDEBUG
static int is_valid_note(const i32_t stp)
{
  return stp >= SEQ_STP_MIN && stp <= SEQ_STP_MAX;
}
#endif

int play_chan(play_t * const P, chan_t * const C)
{
  sequ_t * seq;

  zz_assert( C->trig == TRIG_NOP );
  C->trig = TRIG_NOP;

  /* Ignored voices ? */
  if ( 0x0F & P->muted_voices & C->msk )
    return E_OK;

  /* Portamento */
  if (C->note.stp) {
    zz_assert(is_valid_note(C->note.cur));
    zz_assert(is_valid_note(C->note.aim));
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

  seq = C->cur;
  if (C->wait) --C->wait;
  while (!C->wait) {
    /* This could be an endless loop on empty track but it should
     * have been checked earlier ! */
    u16_t const cmd = U16(seq->cmd);
    u16_t const len = U16(seq->len);
    u32_t const stp = U32(seq->stp);
    u32_t const par = U32(seq->par);
    ++seq;

    /* dmsg("%c cmd:%c len:%04hX stp:%08lX par:%08lX\n", */
    /*      'A'+C->num, (int)cmd, HU(len), LU(stp), LU(par)); */

    switch (cmd) {

    case 'F':                           /* Finish */
      if ( ! ( P->has_loop & C->msk ) )
        dmsg("%c[%hu] loops @%lu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(P->tick));

      seq = C->seq;
      P->has_loop |= C->msk;            /* Set has_loop flags */
      C->loop_sp = C->loops;            /* Safety net */
      break;

    case 'V':                           /* Voice-Set */
      C->curi = par >> 2;
      break;

    case 'P':                           /* Play-Note */
      if (C->curi < 0 || C->curi >= P->vset.nbi) {
        dmsg("%c[%hu]@%lu: using invalid instrument number -- %hu/%hu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(P->tick),
             HU(C->curi+1),HU(P->vset.nbi));
        return E_SNG;
      }
      if (!P->vset.inst[C->curi].len) {
        dmsg("%c[%hu]@%lu: using tainted instrument -- I#%02hu\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(P->tick),
             HU(C->curi+1));
        return E_SNG;
      }

      C->trig     = TRIG_NOTE;
      C->note.ins = P->vset.inst + C->curi;
      C->note.cur = C->note.aim = stp;
      C->note.stp = 0;
      C->wait     = len;
      break;

    case 'S':                       /* Slide-to-note */

      /* GB: This actually happened (e.g. Wrath of the demon - tune 1)
       *     I'm not sure what the original quartet player does.
       *     Here I'm just starting the goal note.
       *
       * zz_assert(C->note.cur);
       */
      if (!C->note.cur) {
        C->trig     = TRIG_NOTE;
        C->note.ins = P->vset.inst + C->curi;
        C->note.cur = stp;
      }
      C->wait     = len;
      C->note.aim = stp;
      C->note.stp = (int32_t)par; /* sign extend, slides are signed */
      break;

    case 'R':                       /* Rest */
      C->trig     = TRIG_STOP;
      C->wait     = len;
      C->note.stp = 0;
      C->note.cur = 0;
      /* GB: The original singsong.prg actually set the current note
       *     to 0.
       */
      break;

    case 'l':                       /* Set-Loop-Point */
      if (C->loop_sp < C->loops+MAX_LOOP) {
        struct loop_s * const l = C->loop_sp++;
        l->off = seq_off(C,seq);
        l->cnt = 0;
      } else {
        dmsg("%c[%hu]@%lu -- loop stack overflow\n",
             'A'+C->num, HU(seq_idx(C,seq-1)), LU(P->tick));
        return E_PLA;
      }
      break;

    case 'L':                       /* Loop-To-Point */
    {
      struct loop_s * l = C->loop_sp-1;

      if (l < C->loops) {
        C->loop_sp = (l = C->loops) + 1;
        l->cnt = 0;
        l->off = 0;
      }

      if ( ( l->cnt = l->cnt ? l->cnt-1 : (par >> 16) ) )
        seq = loop_seq(C,l->off);
      else {
        --C->loop_sp;
        zz_assert(C->loop_sp >= C->loops);
      }

    } break;

    default:
      dmsg("%c[%hu]@%lu command <%c> #%04hX\n",
           'A'+C->num, HU(seq_idx(C,seq-1)), LU(P->tick),
           isalpha(cmd) ? (char)cmd : '^', HU(cmd));
      return E_PLA;
    } /* switch */
  } /* while !wait */
  C->cur = seq;

  /* Muted voices ? */
  if (0xF0 & P->muted_voices & C->msk)
    C->trig = TRIG_STOP;

  return E_OK;
}

/* ---------------------------------------------------------------------- */
zz_err_t
zz_tick(play_t * const P)
{
  chan_t * C;

  /* Increment tick and ms */
  ++ P->tick;
  P->ms_pos  = P->ms_end;
  P->ms_end += P->ms_per_tick;
  P->ms_err += P->ms_err_tick;
  if (P->ms_err >= P->rate) {
    P->ms_err -= P->rate;
    ++ P->ms_end;
  }

  P->has_loop &= 0x0F; /* clear non-persistent part */
  for ( C=P->chan; C<P->chan+4; ++C ) {
    zz_err_t ecode = play_chan(P,C);
    if (E_OK != ecode) {
      P->done |= C->msk & 0xF0;
      emsg("play: %c[%hu]@%lu: failed\n",
           'A'+C->num, HU(seq_idx(C,C->cur)), LU(P->tick));
      return ecode;
    }
  }

  if ( P->ms_max == ZZ_EOF )
    P->done |= ((15 & P->has_loop) == 15) | ((P->ms_pos > P->ms_len) << 1);
  else if ( P->ms_max != 0 )
    P->done |= (P->ms_pos > P->ms_max) << 2;

  /* PCM this frame/tick */
  P->pcm_cnt  = P->pcm_per_tick;
  P->pcm_err += P->pcm_err_tick;
  if (P->pcm_err >= P->rate) {
    P->pcm_err -= P->rate;
    ++P->pcm_cnt;
  }

  return E_OK;
}

/* ---------------------------------------------------------------------- */

i16_t
zz_play(play_t * restrict P, void * restrict pcm, const i16_t n)
{
  i16_t ret = 0;

  zz_assert( P );
  zz_assert( P->mixer || !pcm );

  /* Already done ? */
  if (P->done)
    return -P->code;

  do {
    i16_t cnt;

    if (!P->pcm_cnt) {
      P->code = zz_tick(P);
      if (P->code != E_OK) {
        ret = -P->code;
        break;
      }
      if (P->done)
        break;
    }

    if (n == 0) {
      /* n == 0 :: request remaining pcm to close the current tick */
      zz_assert( P->pcm_cnt > 0 );
      zz_assert( ! ret );
      ret = P->pcm_cnt;
      break;
    }

    if ( n < 0 ) {
      /* n < 0 :: finish the tick but no more than -n */
      zz_assert( !ret );
      cnt = -n;
    } else {
      /* remain to mix */
      cnt = n-ret;
    }
    if (cnt > P->pcm_cnt)
      cnt = P->pcm_cnt;
    P->pcm_cnt -= cnt;

    if (pcm) {
      i16_t written = P->mixer->push(P, pcm, cnt);
      if (written < 0) {
        ret = -(P->code = E_MIX);
        break;
      } else if (written == 0) {
        /* GB: Reversed for future use. Currently should not happen.
         *     A mixer only returning 0 causes an infinite loop.
         */
        zz_assert( ! "mixer should not return 0" );
      }

      /* GB: currently mixers should have mix it all. */
      zz_assert( cnt == written );
      cnt = written;
      /* GB: currently assuming all mixers returns 16bit pcm */
      pcm = (int16_t *) pcm + cnt;
    }
    ret += cnt;

    /* Reset triggers for all channels. */
    P->chan[0].trig = P->chan[1].trig =
      P->chan[2].trig = P->chan[3].trig = TRIG_NOP;

    zz_assert (  ret <= (n<0?-n:n) );
  } while ( ret < n );

  return ret;
}

/* ---------------------------------------------------------------------- */

static void rt_check(void)
{
#ifndef NDEBUG
  static uint8_t bytes[4] = { 1, 2, 3, 4 };
#endif

  /* constant check */
  zz_assert( ZZ_FORMAT_UNKNOWN == 0 );
  zz_assert( ZZ_OK == 0 );
  zz_assert( ZZ_MIXER_DEF > 0 );

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

  /* GB: When using m68k with -mnoshort the default int size is
   *     16bit. It is a problem for some of our bit-masks like
   *     instrument used for instance. We have to cast integer to a
   *     32bit type such as long integer with the a 'l' prefix. This
   *     test that.
   */
  zz_assert( (1l<<20) == 0x100000 );

}

zz_err_t
zz_init(play_t * P, u16_t rate, u32_t ms)
{
  zz_err_t ecode;

  rt_check();
  if (!P)
    return E_ARG;

  P->ms_max = ms;
  P->rate = 0;
  ecode = play_init(P);
  if (E_OK != ecode)
    goto error;

  if (!rate)
    rate = P->song.rate ? P->song.rate : RATE_DEF;
  if (rate < RATE_MIN) rate = RATE_MIN;
  if (rate > RATE_MAX) rate = RATE_MAX;
  P->rate = rate;

  P->ms_len = divu32( mulu32(P->song.ticks,1000), P->rate );
  dmsg("ms-len: %lu (%lu@%huhz)\n",
       LU(P->ms_len),LU(P->song.ticks),HU(P->rate));
  xdivu(1000u, P->rate, &P->ms_per_tick, &P->ms_err_tick);
  dmsg("rate=%huhz ms:%hu(+%hu)\n",
       HU(P->rate), HU(P->ms_per_tick), HU(P->ms_err_tick));
  P->pcm_per_tick = 1; /* in case zz_setup() is not call for measuring only */
  P->pcm_err_tick = 0;

error:
  return P->code = ecode;
}

zz_err_t
zz_setup(play_t * P, u8_t mid, u32_t spr)
{
  zz_err_t ecode;

  if (!P)
    return E_ARG;
  if (P->code)
    return P->code;

  ecode = E_PLA;
  if (!P->rate)
    goto error;

  ecode = E_SET;
  if (!P->vset.iref)
    goto error;

  ecode = E_MIX;
  if (zz_mixer_set(P, mid) == ZZ_MIXER_ERR)
    goto error;

  P->spr = 0;
  if (ecode = P->mixer->init(P, spr), ecode)
    goto error;
  zz_assert( P->spr );

  xdivu(P->spr, P->rate, &P->pcm_per_tick, &P->pcm_err_tick);
  dmsg("spr=%luhz pcm:%hu(+%hu)\n",
       LU(P->spr), HU(P->pcm_per_tick), HU(P->pcm_err_tick));

error:
  P->done = -!!ecode;
  return P->code = ecode;
}

uint8_t zz_mute(play_t * P, uint8_t clr, uint8_t set)
{
  const uint8_t old = P->muted_voices;
  P->muted_voices = ((P->muted_voices) & ~clr) | set;
  return old;
}

/* ---------------------------------------------------------------------- */

#ifndef PACKAGE_STRING
#define PACKAGE_STRING PACKAGE_NAME " " PACKAGE_VERSION
#endif

const char * zz_version(void)
{
  return PACKAGE_STRING;
}

zz_u32_t zz_position(const zz_play_t P)
{
  return !P ? ZZ_EOF : P->ms_pos;
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

static void song_wipe(song_t * song)
{
  memb_wipe((struct memb_s *)song, sizeof(*song));
}

static void vset_wipe(vset_t * vset)
{
  memb_wipe((struct memb_s *)vset, sizeof(*vset));
}

static void info_wipe(info_t * info)
{
  memb_wipe((struct memb_s *)info, sizeof(*info));
}

void zz_wipe(zz_play_t P)
{
  song_wipe(&P->song);
  vset_wipe(&P->vset);
  info_wipe(&P->info);
}

zz_err_t zz_close(zz_play_t P)
{
  zz_err_t ecode = E_ARG;

  if (P) {
    if (P->mixer) {
      P->mixer->free(P);
      P->mixer = 0;
    }
    zz_assert ( ! P->mixer_data ); /* mixer::free() should have clean that */
    P->mixer_data = 0;

    zz_wipe(P);

    zz_strdel(&P->songuri);
    zz_strdel(&P->vseturi);
    zz_strdel(&P->infouri);

    P->st_idx = 0;
    P->pcm_per_tick = 0;
    P->format = ZZ_FORMAT_UNKNOWN;
    P->rate = 0;

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
  zz_assert( pP );
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
  zz_assert( ! "unexpected format" );
  return "?";
}

zz_err_t zz_info(zz_play_t P, zz_info_t * pinfo)
{
  zz_assert(pinfo);
  if (!pinfo)
    return E_ARG;

  if (!P || P->format == ZZ_FORMAT_UNKNOWN) {
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
    pinfo->len.rate = P->rate = P->rate ? P->rate : P->song.rate;
    pinfo->mix.spr  = P->spr;
    pinfo->len.ms   = P->ms_len;

    /* mixer */
    pinfo->mix.num = P->mixer_id;
    if (P->mixer) {
      pinfo->mix.name = P->mixer->name;
      pinfo->mix.desc = P->mixer->desc;
    } else {
#ifndef SC68
      zz_mixer_info(pinfo->mix.num, &pinfo->mix.name, &pinfo->mix.desc);
#else
      pinfo->mix.name = pinfo->mix.desc = "?";
#endif
    }

    pinfo->sng.uri = ZZSTR_SAFE(P->songuri);
    pinfo->set.uri = ZZSTR_SAFE(P->vseturi);
    pinfo->sng.khz = P->song.khz;
    pinfo->set.khz = P->vset.khz;

    /* meta-tags */
    pinfo->tag.album  = P->info.album;
    pinfo->tag.title  = P->info.title;
    pinfo->tag.artist = P->info.artist;
    pinfo->tag.ripper = P->info.ripper;
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
