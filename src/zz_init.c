/**
 * @file   zz_init.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet song and voice-set parser.
 */

#define ZZ_DBG_PREFIX "(ini) "
#include "zz_private.h"
#include <ctype.h>

/* [0,50,200] not official, it's an extension of qts file. */
static int is_valid_tck(const uint16_t tck) {
  return tck == 0 || (tck >= RATE_MIN && tck <= RATE_MAX);
}

/* {4..20} as defined as by the original replay routine. */
static int is_valid_khz(const uint16_t khz) {
  return khz >= 4 && khz <= 20;
}

/* Encountered values so far [8,12,16,24]. */
static int is_valid_bar(const uint16_t bar) {
  return bar >= 4 && bar <= 48 && !(bar & 3) ;
}

/* Encountered values so far {04..36}. */
static int is_valid_spd(const uint16_t tempo) {
  return tempo >= 1 && tempo <= 64;
}

/* Encountered values so far [2:4,3:4,4:4]. */
static int is_valid_sig(const uint8_t sigm, const uint8_t sigd) {
  return sigm >= 1 && sigd <= 4 && sigm <= sigd;
}

/* Instruments {1..20} */
static int is_valid_ins(const uint8_t ins) {
  return ins >= 1 && ins <= 20;
}

/* ----------------------------------------------------------------------
 * quartet song
 * ----------------------------------------------------------------------
 */

zz_err_t
song_init_header(song_t * song, const void *_hd)
{
  const uint8_t * hd = _hd;
  const uint16_t khz = U16(hd+0), bar = U16(hd+2), spd = U16(hd+4);
  uint16_t tck = U16(hd+12) == 0x5754 ? U16(hd+14) : 0;
  zz_err_t ecode;

  /* Parse song header */
  song->rate  = tck;
  song->khz   = khz;
  song->barm  = bar;
  song->tempo = spd;
  song->sigm  = hd[6];
  song->sigd  = hd[7];

  dmsg("song: rate:%huHz spr:%hukHz, bar:%hu, tempo:%hu, signature:%hu/%hu\n",
       HU(song->rate), HU(song->khz),
       HU(song->barm), HU(song->tempo), HU(song->sigm), HU(song->sigd));

  ecode = E_SNG & -!( is_valid_tck(tck) &&
                      is_valid_khz(khz) &&
                      is_valid_bar(bar) &&
                      is_valid_spd(spd) &&
                      is_valid_sig(song->sigm,song->sigd) );
  if (ecode != E_OK)
    emsg("invalid song header\n");
  return ecode;
}

static inline
u16_t seq_idx(const sequ_t * const org, const sequ_t * seq)
{
  return divu( (intptr_t)seq-(intptr_t)org, sizeof(sequ_t) );
}

struct loop_stack_s {
  uint32_t len;
};
typedef struct loop_stack_s loop_stack_t[MAX_LOOP+1];

/* collapse loop stack */
static uint32_t loopstack_collapse(loop_stack_t loops, u8_t ssp)
{
  zz_assert( ssp <= MAX_LOOP );
  for ( ; ssp ; --ssp )
    loops[ssp-1].len += loops[ssp].len;
  return loops[0].len;
}

zz_err_t
song_init(song_t * song)
{
  zz_err_t ecode=E_SNG;
  u16_t off, size;
  u8_t k, has=0, cur=0, ssp=0/* , max_ssp=0 */;

  loop_stack_t loops;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"inish      */
  };

  /* Clean-up */
  song->seq[0] = song->seq[1] = song->seq[2] = song->seq[3] = 0;
  song->stepmin = song->stepmax = 0;
  song->iuse = song->iref = 0;
  song->ticks = 0;

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  for (k=0, off=11, size=song->bin->len;
       k<4 && off<size;
       off += 12)
  {
    sequ_t * const seq = (sequ_t *)(song->bin->ptr+off-11);
    u16_t    const cmd = U16(seq->cmd);
    u16_t    const len = U16(seq->len);
    u32_t    const stp = U32(seq->stp);
    u32_t    const par = U32(seq->par);

    if (!song->seq[k]) {
      /* new sequence starts */
      song->seq[k] = seq;           /* new channel sequence       */
      has = 0;                      /* #0:note #1:wait            */
      cur = 0;                      /* current instrument (0:def) */
      ssp = 0;                      /* loop stack pointer         */
      loops[0].len = 0;
    }

    switch (cmd) {

    case 'F':                           /* Finish */
      if (!has) {
        song->seq[k] = (sequ_t *) nullseq;
        loops[0].len  = 1;
      }
      if (ssp)
        dmsg("(song) %c[%hu] loop not closed -- %hu\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), HU(ssp));
      loopstack_collapse(loops,ssp);
      dmsg("%c duration: %lu ticks\n",'A'+k, LU(loops[0].len));
      if ( loops[0].len > song->ticks)
        song->ticks = loops[0].len;
      ++k;
      break;

    case 'P':                           /* Play-Note */
      has = 1;
      song->iuse |= 1l << cur;

    case 'S':                           /* Slide-To-Note */
      if (stp < SEQ_STP_MIN || stp > SEQ_STP_MAX) {
        emsg("song: %c[%hu] step out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(stp));
        goto error;
      }
      if (!song->stepmax)
        song->stepmax = song->stepmin = stp;
      else if (stp > song->stepmax)
        song->stepmax = stp;
      else if (stp < song->stepmin)
        song->stepmin = stp;

    case 'R':                           /* Rest */
      if (!len) {
        emsg("song: %c[%hu] length out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(len));
        goto error;
      }
      loops[ssp].len += len;
      break;

    case 'l':                           /* Set-Loop-Point */
      if (ssp == MAX_LOOP) {
        emsg("song: %c[%hu] loop stack overflow\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)));
        goto error;
      }
      ++ssp;
      /* if (ssp > max_ssp) max_ssp = ssp; */
      zz_memclr( loops+ssp, sizeof(*loops) );
      break;

    case 'L':                           /* Loop-To-Point */
    {
      u32_t len = loops[ssp].len;
      if (ssp > 0) --ssp;
      loops[ssp].len += len + mulu32(len,par>>16);
    } break;

    case 'V':                           /* Voice-Set */
      cur = par >> 2;
      if ( cur >= 20 || (par & 3) ) {
        emsg("song: %c[%hu] instrument out of range -- %08lx\n",
             'A'+k, HU(seq_idx(song->seq[k],seq)), LU(par));
        goto error;
      }
      song->iref |= 1l << cur;
      break;

    default:
      emsg("song: %c[%hu] invalid sequence -- %04hx-%04hx-%08lx-%08lx\n",
           'A'+k, HU(seq_idx(song->seq[k],seq)),
           HU(cmd), HU(len), LU(stp), LU(par));
      goto error;
    }
  }

  /* dmsg("loop-depth: %hu\n",HU(max_ssp)); */

  if ( (off -= 11) != song->bin->len) {
    dmsg("garbage data after voice sequences -- %lu bytes\n",
         LU(song->bin->len) - off);
  }

  for ( ;k<4; ++k) {
    if (has) {
      /* Add 'F'inish mark */
      sequ_t * const seq = (sequ_t *) (song->bin->ptr+off);
      seq->cmd[0] = 0; seq->cmd[1] = 'F';
      off += 12;

      loopstack_collapse(loops,ssp);
      dmsg("%c duration: %lu ticks\n",'A'+k, LU(loops[0].len));
      if ( loops[0].len > song->ticks)
        song->ticks = loops[0].len;

      dmsg("channel %c is truncated\n", 'A'+k);
      has = 0;
    } else {
      dmsg("channel %c is MIA\n", 'A'+k);
      song->seq[k] = (sequ_t *) nullseq;
    }
  }

  dmsg("song steps: %08lx .. %08lx\n", LU(song->stepmin), LU(song->stepmax));
  dmsg("song instruments: used: %05lx referenced:%05lx difference:%05lx\n",
       LU(song->iuse), LU(song->iref), LU(song->iref^song->iuse));
  ecode = E_OK;

error:
  if (ecode)
    bin_free(&song->bin);

  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

zz_err_t
vset_init_header(zz_vset_t vset, const void * _hd)
{
  const uint8_t *hd = _hd;
  int ecode = E_SET;

  /* header */
  vset->khz = hd[0];
  vset->nbi = hd[1]-1;
  dmsg("vset: spr:%hukHz, ins:%hu/0x%05hx\n",
       HU(vset->khz), HU(vset->nbi), HU(vset->iref));

  if (is_valid_khz(vset->khz) && is_valid_ins(vset->nbi)) {
    i8_t i;
    for (i=0; i<20; ++i) {
      /* vset->inst[i].num = i; */
      vset->inst[i].len = U32(&hd[142+4*i]);
      dmsg("I#%02i [%7s] %08lx\n", i+1, hd+2+7*i, LU(vset->inst[i].len));
    }
    ecode = E_OK;
  }
  return ecode;
}

static u8_t
sort_inst(const inst_t inst[], uint8_t idx[], u32_t iuse)
{
  uint8_t nbi;

  /* while instruments remaining */
  for ( nbi=0; iuse; ) {
    u8_t i, j; u32_t irem;

    /* find first */
    for (i=0; ! ( iuse & (1l<<i) )  ; ++i)
      ;
    irem = iuse & ~(1l<<i);
    zz_assert ( inst[i].pcm );
    zz_assert ( inst[i].len );

    /* find best */
    for (j=i+1; irem; ++j) {
      if (irem & (1l<<j)) {
        irem &= ~(1l<<j);
        zz_assert ( inst[j].pcm );
        zz_assert ( inst[j].len );
        if ( inst[j].pcm < inst[i].pcm )
          i = j;
      }
    }
    idx[nbi++] = i;
    iuse &= ~(1l<<i);
  }

  return nbi;
}


static void
change_sign(uint8_t * dst, uint8_t * src, u16_t len)
{
  zz_assert( dst >= src );
  zz_assert( len );

  dst += len; src += len;
  do {
    *--dst = 0x80 ^ *--src;
  } while (--len);
}

static void
unroll_loop(uint8_t * dst, i32_t max,
            uint8_t * src, u16_t len, const u16_t lpl)
{
  max -= len;

  zz_assert( len );
  zz_assert( max >= 0 );
  zz_assert( dst <= src );

  if (src == dst)
    src = dst += len;
  else
    do { *dst++ = *src++; } while ( --len );

  if (lpl)
    for ( src=dst-lpl; max; --max)
      *dst++ = *src++;
  else {
    i16_t v, r;
    for (v=(int8_t)dst[-1], r=0 ; max; --max) {
      v = 3*v+r; r = v & 3; v >>= 2;
      *dst++ = v;
    }
  }
}

#ifdef DEBUG
static uint16_t sum_part(uint16_t sum, uint8_t * pcm, i32_t len, uint8_t sig)
{
  while (len--)
    sum = (sum+sum+sum) ^ sig ^ *pcm++;
  return sum;
}
#endif

static inline u32_t int_align(const u32_t ptr, const u8_t align)
{
  const u32_t mask = (u32_t) align - 1;
  return (ptr + mask) & ~mask;
}

#define INT_ALIGN(ADR) int_align((ADR),sizeof(int))

zz_err_t
vset_init(zz_vset_t const vset)
{
  u8_t nbi = vset->nbi, i;
  bin_t   * const bin = vset->bin;
  uint8_t * const beg = bin->ptr;
  uint8_t * const end = beg + bin->max;
  uint8_t * e;

  u32_t tot, unroll, imsk;

  uint8_t idx[20];
#ifdef DEBUG
  uint16_t sum[20];
#endif
  zz_assert( nbi > 0 && nbi <= 20 );

  /* imsk is best set before when possible so that unused (yet valid)
   * instruments can be ignored. This gives a little more space to
   * unroll loops and save a bit of processing during init. However it
   * might destroy instrument used by other songs using the same
   * voiceset.
   *
   * GB: Currently using the referenced instrument mask instead of
   * used instrument mask. That's because in some cases where an
   * instrument is selected at the end of a loop it can be used
   * without being properly detected (due to init code not following
   * loops). It's possible (yet unlikely) for an instrument to be
   * referenced but not being used. That's the lesser of two evils.
   *
   */
  imsk = vset->iref ? vset->iref : (1l<<nbi)-1;

  for (i=vset->iref=0; i<20; ++i) {
    const i32_t off = vset->inst[i].len - 222 + 8;
    u32_t len = 0, lpl = 0;
    uint8_t * pcm = 0;

#ifdef DEBUG_LOG
    const char * tainted = 0;
#   define TAINTED(COND) if ( COND ) { tainted = #COND; break; } else
#else
#   define TAINTED(COND) if ( COND ) { break; } else
#endif

    do {
      /* Sanity tests */
      TAINTED (!(1&(imsk>>i)));        /* instrument in used ? */
      TAINTED (off < 8);                /* offset in range ? */
      TAINTED (off >= bin->len);        /* offset in range ? */

      /* Get sample info */
      pcm = bin->ptr + off;
      len = U32(pcm-4);
      lpl = U32(pcm-8);
      if (lpl == 0xFFFFFFFF) lpl = 0;

      /* Some of these tests might be a tad conservative but as the
       * format is not very robust It might be a nice safety net.
       * Currently I haven't found a music file that is valid and does
       * not pass them.
       */
      TAINTED (len & 0xFFFF);           /* clean length ? */
      TAINTED (lpl & 0xFFFF);           /* clean loop ? */
      TAINTED ((len >>= 16) == 0);      /* have data ? */
      TAINTED ((lpl >>= 16) > len);     /* loop inside sample ? */
      TAINTED (off+len > bin->len);     /* sample in range ? */

      vset->iref |= 1l << i;
      dmsg("I#%02hu [$%05lX:$%05lX:$%05lX] [$%05lX:$%05lX:$%05lX]\n",
           HU(i+1),
           LU(0), LU(len-lpl), LU(len),
           LU(off), LU(off+len-lpl), LU(off+len));
    } while (0);

    if ( !(1 & (vset->iref>>i)) ) {
      dmsg("I#%02hu was tainted by (%s)\n",HU(i+1),tainted);
      pcm = 0;
      len = lpl = 0;                   /* If not used mark as dirty */
    }

#ifdef DEBUG
    sum[i] = sum_part(0xDEAD, pcm, len, 0);
#endif

    zz_assert( (!!len) == (1 & (vset->iref>>i)) );
    zz_assert( len < 0x10000 );
    zz_assert( lpl <= len );

    vset->inst[i].end = len;
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }

  nbi = sort_inst(vset->inst, idx, imsk );
  if (!nbi)
    return E_SET;
  dmsg("Instruments: used:%hu/%hu ($%05lX/$%05lX)\n",
       HU(nbi), HU(vset->nbi), LU(vset->iref), LU(imsk));

#ifdef DEBUG_LOG
  dmsg("-------------------------------\n");
  for (i=0; i<nbi; ++i) {
    inst_t * const I = vset->inst + idx[i];
    dmsg("#%02hu I#%02hu [$%05lX:$%05lX:$%05lX]\n",
         HU(i+1),HU(idx[i]+1),
         LU(I->pcm-beg), LU(&I->pcm[I->len-I->lpl]-beg),
         LU(&I->pcm[I->len]-beg));
  }
  dmsg("-------------------------------\n");
#endif

  /* Use available space before sample (AVR header) to unroll
   * loops. The standard mixers don't use unrolled loop anymore but it
   * might still be useful for other mixers.
   */
#if 0

  /* Just change sign */
  for (i=0; i<nbi; ++i) {
    inst_t * const I = vset->inst + idx[i];
    unroll_loop(I->pcm, I->len, 0x80,
                I->pcm, I->len, I->lpl);
  }

#else
  /* Stack all samples at the bottom of the buffer, changing sign in
   * the process.
   */
  for (i=1, e=end, tot=0; i<=nbi; ++i) {
    inst_t * const inst = &vset->inst[idx[nbi-i]];
    change_sign(e -= inst->len, inst->pcm, inst->len);
    inst->pcm = e;
    tot += inst->len;
  }

  unroll = divu32(bin->max-tot, nbi);
  dmsg("total space is: %lu/%lu +%lu/spl\n",
       LU(tot), LU(bin->max),LU(unroll));

  zz_assert(unroll >= 8);
  if (unroll > 1024)
    unroll = 1024;

  for (i=0, e=beg; i<nbi; ++i) {
    inst_t * const inst = &vset->inst[idx[i]];
    uint8_t * const l = e + INT_ALIGN( inst->len + unroll );;
    zz_assert( inst->pcm );
    zz_assert( inst->len );
    unroll_loop(e, l-e, inst->pcm, inst->len, inst->lpl);
    inst->pcm = e;
    inst->end = l-e;
    e = l;
  }
#endif

#ifdef DEBUG
  for (i=0; i<20; ++i) {
    zz_assert( sum_part(0xDEAD, vset->inst[i].pcm, vset->inst[i].len, 0x80)
               == sum[i] );
  }
#endif

  return E_OK;
}
