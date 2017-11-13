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

zz_err_t
song_init(song_t * song)
{
  zz_err_t ecode = E_SNG;
  u8_t  ins, has_note, has_wait, k;
  u16_t off, size;
  u32_t iref = 0;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"in */
  };

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  has_note = has_wait = ins = 0;
  for (k=0, off=11, size=song->bin->len, song->iused=0;
       k<4 && off<size;
       off += 12)
  {
    sequ_t * const seq = (sequ_t *)(song->bin->ptr+off-11);
    u16_t    const cmd = U16(seq->cmd);
    u16_t    const len = U16(seq->len);
    u32_t    const stp = U32(seq->stp);
    u32_t    const par = U32(seq->par);

    if (!song->seq[k])
      song->seq[k] = seq;               /* Sequence */

    switch (cmd) {
    case 'F':                           /* End-Voice */
      if (!has_note)
        song->seq[k] = (sequ_t *) nullseq;
      has_note = 0;
      ++k;
      break;

    case 'P':                           /* Play-Note */
      has_note = 1;

      if ( ! (song->iused & (1l << ins)) )
        dmsg("I#%02hu used for the first time @%c[%hu]\n",
             HU(ins+1), 'A'+k, HU(seq_idx(song->seq[k],seq)));
      song->iused |= 1l << ins;

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
      break;

    case 'l':                           /* Set-Loop-Point */
    case 'L':                           /* Loop-To-Point */
      break;
    case 'V':                           /* Voice change */
      if ( (ins = par >> 2) < 20u && ! (par & ~(31u<<2) ) ) {
        if (! (iref & (1l << ins)) )
          dmsg("I#%02hu refenced for the first time @%c[%hu]\n",
               HU(ins+1), 'A'+k, HU(seq_idx(song->seq[k],seq)));
        iref |= 1l << ins;
        break;
      }
      emsg("song: %c[%hu] instrument out of range -- %08lx\n",
           'A'+k, HU(seq_idx(song->seq[k],seq)), LU(par));
      goto error;

    default:
      emsg("song: %c[%hu] invalid sequence -- %04hx-%04hx-%08lx-%08lx\n",
           'A'+k, HU(seq_idx(song->seq[k],seq)),
           HU(cmd), HU(len), LU(stp), LU(par));
      goto error;
    }
  }

  if ( (off -= 11) != song->bin->len) {
    dmsg("garbage data after voice sequences -- %lu bytes\n",
         LU(song->bin->len) - off);
  }

  for ( ;k<4; ++k) {
    if (has_note) {
      /* Add 'F'inish mark */
      sequ_t * const seq = (sequ_t *) (song->bin->ptr+off);
      seq->cmd[0] = 0; seq->cmd[1] = 'F';
      off += 12;
      has_note = 0;
      dmsg("channel %c is truncated\n", 'A'+k);
    } else {
      dmsg("channel %c is MIA\n", 'A'+k);
      song->seq[k] = (sequ_t *) nullseq;
    }
  }

  dmsg("song steps: %08lx .. %08lx\n", LU(song->stepmin), LU(song->stepmax));
  dmsg("song iused: %05lx iref:%05lx diff:%lx\n",
       LU(song->iused), LU(iref), LU(iref^song->iused));
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
  dmsg("vset: spr:%hukHz, ins:%hu/0x%hx\n",
       HU(vset->khz), HU(vset->nbi), HU(vset->iused));

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

static void sort_inst(const inst_t inst[], uint8_t idx[], int nbi)
{
  uint8_t i,j,perm;
  /* bubble sort */
  for (perm=1, i=0; perm && i<nbi-1; ++i)
    for (perm=0, j=i+1; j<nbi; ++j)
      if (inst[idx[i]].pcm < inst[idx[j]].pcm) {
        perm = 1;
        idx[i] ^= idx[j];               /* i^j */
        idx[j] ^= idx[i];               /* j^i^j => i */
        idx[i] ^= idx[j];               /* i^i^j => j */
      }
}

zz_err_t
vset_init(zz_vset_t const vset)
{
  const u8_t nbi = vset->nbi;
  bin_t * const bin = vset->bin;
  uint8_t * const beg = bin->ptr;
  uint8_t * const end = beg + bin->max;
  uint8_t * e;
  i32_t tot, unroll, j, imask;
  u8_t nused, i;
  uint8_t idx[20];
  uint16_t sum[20];

  zz_assert( nbi > 0 && nbi <= 20 );

  /* imask is best set before when possible so that unused (but valid)
   * instruments can be ignored. This give a little more space to
   * unroll loops.
   *
   * Unfortunately the way the loader work at the moment does not
   * really help with that.
   */
  imask = vset->iused ? vset->iused : (1l<<nbi)-1;
#ifdef DEBUG
  if (1) {
    char t[21];
    for (i=0; i<20; ++i)
      t[i] = ( 1 & (imask>>i) ) ? '-' : '0'+(i+1)%10u;
    t[20] = 0;
    dmsg("Used mask: %s\n", t);
  }
#endif

  for (i=nused=vset->iused=0; i<20; ++i) {
    const i32_t off = vset->inst[i].len - 222 + 8;
    u32_t len = 0, lpl = 0;
    uint8_t * pcm = 0;

#ifdef DEBUG
    const char * tainted = 0;
#   define TAINTED(COND) if ( COND ) { tainted = #COND; break; } else
#else
#   define TAINTED(COND) if ( COND ) { break; } else
#endif

    do {
      /* Sanity tests */
      TAINTED (!(1&(imask>>i)));        /* Used ? */
      TAINTED (off < 8);                /* inside ?  */
      TAINTED (off >= bin->len);        /* inside ?  */

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
      TAINTED (len & 0xFFFF);
      TAINTED (lpl & 0xFFFF);
      TAINTED ((len >>= 16) == 0);
      TAINTED ((lpl >>= 16) > len);
      TAINTED (off+len > bin->len);

      vset->iused |= 1l << i;
      ++nused;
      dmsg("I#%02hu [$%05lX:$%05lX:$%05lX] [$%05lX:$%05lX:$%05lX]\n",
           HU(i+1),
           LU(0), LU(len-lpl), LU(len),
           LU(off), LU(off+len-lpl), LU(off+len));
    } while (0);

    if ( !(1 & (vset->iused>>i)) ) {
      dmsg("I#%02hu is tainted (%s)\n",HU(i+1),tainted);
      pcm = 0, len = lpl = 0;           /* If not used mark dirty */
    }

    sum[i] = 0;
    if (pcm) {
      int32_t k;
      for (k=0; k<len; ++k)
        sum[i] = sum[i]*3^ ( 255 & pcm[k] );
    }

    zz_assert( (!!len) == (1 & (vset->iused>>i)) );
    zz_assert( len < 0x10000 );
    zz_assert( lpl <= len );

    vset->inst[i].end = len;
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }
  dmsg("Instruments: used:%hu/%hu ($%05lX/$%05lX)\n",
       HU(nused), HU(nbi), LU(vset->iused), LU(imask) );

  /* That's very conservative. we should definitively warn in case any
   * instrument got dismissed but an error might be a little to rough
   */
  if (vset->iused != imask) {
    emsg("(instrument) instrument(s) got dismissed $%06lX != %06lX\n",
         LU(vset->iused), LU(imask));
    return E_SET;
  }

  /* Loops unroll */

  /* -1- Re-order instruments in memory order. */
  for (i=tot=0; i<nbi; ++i) {
    const u32_t len = vset->inst[i].len;
    idx[i] = i;
    tot += (len+1) & -2;
  }
  zz_assert( tot <= end-beg );
  sort_inst(vset->inst, idx, nbi);

  /* -2- Compute the unroll size. */
  unroll = ( divu32(end-beg-tot,nused) - 1 ) & -2;
  dmsg("%hu/%hu instrument using %lu/%lu bytes unroll:%lu\n",
       HU(nused), HU(nbi), LU(tot), LU(end-beg), LU(unroll));
  if (unroll < VSET_UNROLL)
    wmsg("Have less unroll space than expected -- %lu\n",
         LU(VSET_UNROLL-unroll));

  /* -3- Unroll starting from bottom to top */
  for (i=0, e=end; i<nbi; ++i) {
    inst_t * const inst = vset->inst + idx[i];
    const u32_t r_len = inst->len;        /* real length */
    const u32_t a_len = (r_len + 1) & -2; /* aligned length */
    uint8_t * const pcm = (void *)( (intptr_t) (e-unroll-a_len) & -2);

    if (!a_len)
      continue;
    zz_assert( 1 & (vset->iused>>idx[i]) );
    zz_assert( a_len < 0x10000 );
    zz_assert( pcm >= beg );
    zz_assert( pcm+a_len <= end  );

    inst->end = a_len+unroll;

    dmsg("I#%02hu I#%02hu %06lx %c %06lx %lu\n",
         HU(i+1),HU(idx[i]),
         LU(pcm-beg),
         "><"[pcm<=inst->pcm],
         LU(inst->pcm-beg),
         LU(inst->len));

    /* Copy and sign PCMs */
    if (pcm <= inst->pcm)
      for (j=0; j<r_len; ++j) {
        zz_assert( pcm+j >= beg );
        zz_assert( pcm+j < end );
        pcm[j] = 0x80 ^ inst->pcm[j];
      }
    else
      for (j=r_len-1; j>=0; --j) {
        zz_assert( pcm+j >= beg );
        zz_assert( pcm+j < end );
        pcm[j] = 0x80 ^ inst->pcm[j];
      }
    inst->pcm = pcm;

    if (!inst->lpl) {
      /* Instrument does not loop -- smooth to middle point */
      int8_t v = (int8_t)pcm[r_len-1]; const i16_t c = -(v<0) & 3;
      for (j=r_len; j<inst->end && (v=(3*v+c)>>2); ) pcm[j++] = v;
      for (; j<inst->end; ) pcm[j++] = v;
    } else {
      int lpi;
      /* Copy loop */
      for(j=lpi=r_len; j<inst->end; ++j) {
        if (lpi >= r_len)
          lpi -= inst->lpl;
        zz_assert(lpi < r_len);
        zz_assert(lpi >= r_len-inst->lpl);
        pcm[j] = pcm[lpi++];
      }
    }
    e = pcm;
  }

  for (i=0; i<20; ++i) {
    uint16_t s2 = 0;
    inst_t * const inst = vset->inst+i;

    if (inst->len) {
      int32_t k;
      for (k=0; k<inst->len; ++k)
        s2 = s2 * 3 ^ ( 255 & (0x80^inst->pcm[k]) );
    }
    dmsg("I#%02hu sum: %04X %04X\n", HU(i+1), HU(s2), HU(sum[i]));
    zz_assert( s2 == sum[i] );
  }

  return E_OK;
}
