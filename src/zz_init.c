/**
 * @file   zz_init.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet song and voice-set parser.
 */

#define ZZ_DBG_PREFIX "(ini) "
#include "zz_private.h"
#include <ctype.h>

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
  const uint16_t khz = u16(hd+0), bar = u16(hd+2), spd = u16(hd+4);

  /* Parse song header */
  song->khz   = khz;
  song->barm  = bar;
  song->tempo = spd;
  song->sigm  = hd[6];
  song->sigd  = hd[7];
  dmsg("song: spr:%ukHz, bar:%u, tempo:%u, signature:%u/%u\n",
       song->khz, song->barm, song->tempo, song->sigm, song->sigd);

  return E_SNG & -!( is_valid_khz(khz) &&
                     is_valid_bar(bar) &&
                     is_valid_spd(spd) &&
                     is_valid_sig(song->sigm,song->sigd) );
}

zz_err_t
song_init(song_t * song)
{
  uint8_t ins, has_note, k, ecode=E_SNG;
  int off, size;

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
  for (k=has_note=ins=0, off=11, size=song->bin->len, song->iused=0;
       k<4 && off<size;
       off += 12)
  {
    sequ_t * const seq = (sequ_t *)(song->bin->buf+off-11);
    u16_t    const cmd = u16(seq->cmd);
    u32_t    const stp = u32(seq->stp);
    u32_t    const par = u32(seq->par);

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
      song->iused |= 1<<ins;
    case 'S':
      if (stp < SEQ_STP_MIN || stp > SEQ_STP_MAX)
        goto error;
      if (!song->stepmax)
        song->stepmax = song->stepmin = stp;
      else if (stp > song->stepmax)
        song->stepmax = stp;
      else if (stp < song->stepmin)
        song->stepmin = stp;
      break;
    case 'R': case 'l': case 'L':
      break;
    case 'V':
      if ( (ins = par >> 2) < 20u && ! (par & ~(31u<<2) ) )
        break;
      goto error;

    default:
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
      sequ_t * const seq = (sequ_t *) (song->bin->buf+off);
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
      vset->inst[i].len = u32(&hd[142+4*i]);
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
  uint8_t * const beg = bin->buf;
  uint8_t * const end = beg + bin->max;
  uint8_t * e;
  i32_t tot, unroll, j, imask;
  u8_t nused, i;
  uint8_t idx[20];

  zz_assert( nbi > 0 && nbi <= 20 );

  /* imask is best set before when possible so that unused (but valid)
   * instruments can be ignored. This give a little more space to
   * unroll loops.
   *
   * Unfortunately the way the loader work at the moment does not
   * really help with that.
   */
  imask = vset->iused ? vset->iused : (1 << nbi) - 1;

  for (i=nused=vset->iused=0; i<20; ++i) {
    const i32_t off = vset->inst[i].len - 222 + 8;
    u32_t len = 0, lpl = 0;
    uint8_t * pcm = 0;

    do {
      /* Sanity tests */
      if (!(imask & (1<<i))) break;     /* Used ? */
      if (off < 8)           break;     /* inside ?  */
      if (off >= bin->len)   break;     /* inside ?  */

      /* Get sample info */
      pcm = bin->buf + off;
      len = u32(pcm-4);
      lpl = u32(pcm-8);
      if (lpl == 0xFFFFFFFF) lpl = 0;

      /* Some of these tests might be a tad conservative but as the
       * format is not very robust It might be a nice safety net.
       * Currently I haven't found a music file that is valid and does
       * not pass them.
       */
      if (len & 0xFFFF)       break;
      if (lpl & 0xFFFF)       break;
      if ((len >>= 16) == 0)  break;
      if ((lpl >>= 16) > len) break;
      if (off+len > bin->len) break;

      vset->iused |= 1<<i;
      ++nused;
      dmsg("I#%02hu [$%05lX:$%05lX:$%05lX] [$%05lX:$%05lX:$%05lX]\n",
           HU(i+1),
           LU(0), LU(len-lpl), LU(len),
           LU(off), LU(off+len-lpl), LU(off+len));
    } while (0);

    if ( ! ( vset->iused & (1<<i) ) )
      pcm = 0, len = lpl = 0;           /* If not used mark dirty */

    zz_assert( (!!len) == !!( vset->iused & (1<<i) ) );
    zz_assert( len < 0x10000 );
    zz_assert( lpl <= len );

    vset->inst[i].end = len;
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }
  dmsg("Instruments: used:%hu/%hu ($%06lX/$%06lX)\n",
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
    wmsg("Have less unroll space than expected -- %lu\n", LU(unroll-VSET_UNROLL));

  /* -3- Unroll starting from bottom to top */
  for (i=0, e=end; i<nbi; ++i) {
    inst_t * const inst = vset->inst + idx[i];
    const u32_t r_len = inst->len;        /* real length */
    const u32_t a_len = (r_len + 1) & -2; /* aligned length */
    uint8_t * const pcm = (void *)( (intptr_t) (e-unroll-a_len) & -2);

    /*
    dmsg("[%02hu] I#%02hu rlen=%5lu alen:%5lu pcm<%p>%s..<%p>%s inst:<%p>%s\n",
         HU(i), HU(idx[i]), LU(r_len), LU(a_len),
         pcm,       pcm       <  beg ? " < beg" : "",
         pcm+a_len, pcm+a_len <= end ? "" : " > end",
         inst->pcm, pcm <= inst->pcm ? "fwd":"bwd"
      );
    */

    if (!a_len)
      continue;
    zz_assert( (vset->iused & (1<<idx[i])) );
    zz_assert( a_len < 0x10000 );
    zz_assert( pcm >= beg );
    zz_assert( pcm+a_len <= end  );

    inst->end = a_len+unroll;

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
      int8_t v = (int8_t)pcm[r_len-1];
      for (j=r_len; j<inst->end && (pcm[j++] = v = 3*v/4););
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

  return E_OK;
}
