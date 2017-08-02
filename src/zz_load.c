/**
 * @file   zz_load.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet files loader.
 */

#include "zz_private.h"
#include <stdlib.h>                     /* qsort */
#include <ctype.h>

/* ----------------------------------------------------------------------
 * quartet song
 * ----------------------------------------------------------------------
 */

static int song_parse(song_t *song, vfs_t vfs, uint8_t *hd, uint_t size)
{
  int ecode, k, has_note=0;
  uint_t off;

  /* sequence to use in case of empty sequence to avoid endless loop
   * condition and allow to properly measure music length.
   */
  static uint8_t nullseq[2][12] = {
    { 0,'R', 0, 1 },                    /* "R"est 1 tick */
    { 0,'F' }                           /* "F"in */
  };

  /* Parse song header */
  song->khz   = u16(hd+0);
  song->barm  = u16(hd+2);
  song->tempo = u16(hd+4);
  song->sigm  = hd[6];
  song->sigd  = hd[7];
  dmsg("%s: spr: %ukHz, bar:%u, tempo:%u, signature:%u/%u\n", vfs_uri(vfs),
       song->khz, song->barm, song->tempo, song->sigm, song->sigd);

  /* Load data (add 12 extra bytes to close truncated sequence if
   * needed). */
  ecode = bin_load(&song->bin, vfs, size, 12, SONG_MAX_SIZE);
  if (ecode)
    goto error;
  ecode = E_SNG;
  size = (ZZLEN(song->bin) / 12u) * 12u;

  /* Basic parsing of the sequences to find the end. It replaces
   * empty sequences by something that won't loop endlessly and won't
   * disrupt the end of music detection.
   */
  for (k=off=0; k<4 && off<size; off+=12) {
    sequ_t * const seq = (sequ_t *)(ZZBUF(song->bin)+off);
    uint_t   const cmd = u16(seq->cmd);
    uint_t   const stp = u32(seq->stp);

    if (!song->seq[k])
      song->seq[k] = seq;               /* Sequence */

    if (0) {
      dmsg("%c %04u %c %04x %08x %04x-%04x\n",
           k+'A',
           (uint_t)(seq-song->seq[k]),
           isgraph(cmd) ? cmd : '?',
           u16(seq->len),
           u32(seq->stp),
           u16(seq->par),
           u16(seq->par+2));
    }


    switch (cmd) {
    case 'F':                           /* End-Voice */
      if (!has_note) {
        song->seq[k] = (sequ_t *) nullseq;
        dmsg("%c: replaced by default sequence\n", 'A'+k);
      }
      has_note = 0;
      ++k;
      break;
    case 'P':                           /* Play-Note */
      has_note = 1;
    case 'S':
      if (stp < SEQ_STP_MIN || stp > SEQ_STP_MAX) {
        emsg("invalid sequence command step -- %08x<=%08x<=%08x\n",
             SEQ_STP_MIN,stp,SEQ_STP_MAX);
        goto error;
      }
      if (!song->stepmax)
        song->stepmax = song->stepmin = stp;
      else if (stp > song->stepmax)
        song->stepmax = stp;
      else if (stp < song->stepmin)
        song->stepmin = stp;
      break;
    case 'R':
    case 'l': case 'L': case 'V':
      break;

    default:
      /* if (k == 3 && has_note) { */
      /*   wmsg("channel hacking the last sequence to close the deal\n"); */
      /*   seq->cmd[0] = 0; */
      /*   seq->cmd[1] = 'F'; */
      /*   off -= 12; */
      /*   break; */
      /* } */
      emsg("invalid sequence command $%04x('%c') at %c:%u\n",
           cmd, isgraph(cmd)?cmd:'.', 'A'+k, off);
      goto error;
    }
  }

  if (off != ZZLEN(song->bin)) {
    wmsg("garbage data after voice sequences -- %u bytes\n",
         ZZLEN(song->bin) - off);
  }

  for ( ;k<4; ++k) {
    if (has_note) {
      /* Add 'F'inish mark */
      sequ_t * const seq = (sequ_t *) ZZOFF(song->bin,off);
      seq->cmd[0] = 0; seq->cmd[1] = 'F';
      off += 12;
      has_note = 0;
      wmsg("channel %c is truncated\n", 'A'+k);
    } else {
      wmsg("channel %c is MIA\n", 'A'+k);
      song->seq[k] = (sequ_t *) nullseq;
    }
  }

  dmsg("song steps: %08x .. %08x\n", song->stepmin, song->stepmax);

  ecode = E_OK;
error:
  if (ecode)
    bin_free(&song->bin);
  return ecode;
}

int
song_load(song_t *song, const char *path)
{
  uint8_t hd[16];
  vfs_t vfs = 0;

  int ecode = vfs_open_uri(&vfs, path);
  if (ecode)
    goto error;

  ecode = vfs_read_exact(vfs,hd,16);
  if (ecode)
    goto error;
  ecode = song_parse(song,vfs,hd,0);

error:
  vfs_del(&vfs);
  if (ecode) {
    if (ecode != E_SYS)
      ecode = E_SNG;
    bin_free(&song->bin);
  }
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

static int cmpadr(const void * a, const void * b)
{
  return (*((inst_t **)b))->pcm - (*((inst_t **)a))->pcm;
}

static void prepare_vset(vset_t *vset, const char *path)
{
  const int n = vset->nbi;
  uint8_t * const beg = ZZBUF(vset->bin);
  uint8_t * const end = beg+ZZMAX(vset->bin);
  uint8_t * e;
  int i,j,tot,unroll;
  inst_t *pinst[20];

  assert(n>0 && n<=20);

  /* Re-order instrument in memory order. */
  for (i=tot=0; i<n; ++i) {
    uint_t len = vset->inst[i].len;
    if (len >= 0x10000)
      wmsg("I%02u length > 64k -- %08x\n", i, len);
    assert( len < 0x10000 );
    pinst[i] = vset->inst+i;
    tot += len;
  }
  assert(tot <= end-beg);
  unroll = (end-beg-tot) / n;
  dmsg("%u instrument using %u/%u bytes unroll:%i\n",
       n, tot, (uint_t)(end-beg), unroll);

  if (unroll < VSET_UNROLL)
    wmsg("Have less unroll space than expected -- %d\n", unroll-VSET_UNROLL);

  qsort(pinst, n, sizeof(*pinst), cmpadr);

  for (i=0, e=end; i<n; ++i) {
    inst_t * const inst = pinst[i];
    const uint_t len = inst->len;
    uint8_t * const pcm = e - unroll - len;

    if (len == 0) {
      dmsg("i#%02u is tainted -- ignoring\n", (uint_t)(pinst[i]-vset->inst));
      continue;
    }
    inst->end = len + unroll;

    dmsg("i#%02u copying %05x to %05x..%05x\n",
         (uint_t)(pinst[i]-vset->inst),
         (uint_t)(pinst[i]->pcm-beg),
         (uint_t)(pcm-beg),
         (uint_t)(pcm-beg+len-1));

    if (pcm <= pinst[i]->pcm)
      for (j=0; j<len; ++j)
        pcm[j] = 0x80 ^ pinst[i]->pcm[j];
    else
      for (j=len-1; j>=0; --j)
        pcm[j] = 0x80 ^ pinst[i]->pcm[j];
    pinst[i]->pcm = pcm;

    if (!inst->lpl) {
      /* Instrument does not loop -- smooth to middle point */
      int v = (int)(int8_t)pcm[len-1] << 8;
      for (j=0; j<unroll; ++j) {
        v = 3*v >> 2;                   /* v = .75*v */
        pcm[len+j] = v>>8;
      }
    } else {
      int lpi = len;
      /* Copy loop */
      for (j=0; j<unroll; ++j) {
        if (lpi >= len)
          lpi -= inst->lpl;
        assert(lpi < len);
        assert(lpi >= len-inst->lpl);
        pcm[len+j] = pcm[lpi++];
      }
    }
    e = pcm;
  }
}

int vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, uint_t size)
{
  int ecode = E_SET, i;
  bin_t * bin;
  const char * path = vfs_uri(vfs);

  /* Check sampling rate */
  vset->khz = hd[0];
  dmsg("%s: %u kHz\n", path, vset->khz);
  if (vset->khz < 4u || vset->khz > 20u) {
    emsg("sampling rate (%u) out of range -- %s\n", vset->khz, path);
    goto error;
  }

  /* Check instrument number */
  vset->nbi = hd[1]-1;
  dmsg("%s: %u instrument\n", path, vset->nbi);

  if (vset->nbi < 1 || vset->nbi > 20) {
    emsg("instrument count (%d) out of range -- %s\n", vset->nbi, path);
    goto error;
  }

  /* Load data */
  ecode = bin_load(&vset->bin, vfs, size, VSET_XSIZE, VSET_MAX_SIZE);
  if (ecode)
    goto error;

  for (i=0, ecode=E_SET, bin=vset->bin; i<vset->nbi; ++i) {
    int off = u32(&hd[142+4*i]), invalid = 0;
    int o = off - 222 + 8;
    uint_t len, lpl;
    uint8_t * pcm;

    pcm = ZZOFF(bin,o);
    len = u32(pcm-4);
    lpl = u32(pcm-8);
    if (lpl == 0xFFFFFFFF)
      lpl = 0;

    /* These 2 tests might be a little pedantic but as the format is
     * not very robust It might be a nice safety net. These 2 words
     * should be 0.
     */
    if (len & 0xFFFF) {
      wmsg("I#%02i length LSW is not 0 [%08X]\n", i+1, len);
      ++invalid;
    }
    if (lpl & 0xFFFF) {
      wmsg("I#%02i loop-length LSW is not 0 [%08X]\n", i+1, lpl);
      ++invalid;
    }
    len >>= 16;
    lpl >>= 16;

    if (!len) {
      wmsg("I#%02u has no data\n", i+1);
      ++invalid;
    }

    if (lpl > len) {
      wmsg("I#%02u loop-length > length [%08X > %08X] \n",
           i+1, lpl, len);
      ++invalid;
    }

    /* Is the sample inside out memory range ? */
    if ( o < 8 || o >= ZZLEN(bin) || (o+len > ZZLEN(bin)) ) {
      wmsg("I#%02u data out of range [%05x..%05x] not in [%05x..%05x] +%u\n",
           i+1, o,o+len, 8, ZZLEN(bin), o+len-ZZLEN(bin));
      ++invalid;
    }

    if (invalid) {
      pcm = 0;
      len = lpl = 0;
    } else {
      dmsg("I#%02u \"%7s\" [$%05X:$%05X:$%05X] [$%05X:$%05X:$%05X]\n",
           i+1, hd+2+7*i,
           0, len-lpl, len,
           o, o+len-lpl, o+len);
    }

    vset->inst[i].end = len;
    vset->inst[i].len = len;
    vset->inst[i].lpl = lpl;
    vset->inst[i].pcm = pcm;
  }

  prepare_vset(vset, path);
  ecode = E_OK;
error:
  if (ecode)
    bin_free(&vset->bin);
  return ecode;
}


/**
 * Load .4q file (after header).
 */
int
q4_load(vfs_t vfs, q4_t *q4)
{
  int ecode;
  uint8_t hd[222];
  const char * path = vfs_uri(vfs);

  assert(vfs_tell(vfs) == 20);

  if (q4->songsz < 16 + 12*4) {
    emsg("invalid .4q song size (%u) -- %s", q4->songsz, path);
    ecode = E_SNG;
    goto error;
  }

  if (q4->vset && q4->vsetsz < 222) {
    emsg("invalid .4q set size (%u) -- %s", q4->vsetsz, path);
    ecode = E_SNG;
    goto error;
  }

  /* Read song */
  ecode = E_INP;
  if (q4->song) {
    if (vfs_read_exact(vfs, hd, 16))
      goto error;

    ecode = song_parse(q4->song, vfs, hd, q4->songsz-16);
    if (ecode)
      goto error;
  } else if (vfs_seek(vfs, q4->songsz, VFS_SEEK_CUR))
    goto error;

  /* Voice set */
  ecode = E_INP;
  if (q4->vset) {
    if (vfs_read_exact(vfs,hd,222) < 0)
      goto error;
    ecode = vset_parse(q4->vset, vfs, hd, q4->vsetsz-222);
    if (ecode)
      goto error;
  } else if (vfs_seek(vfs, q4->vsetsz, VFS_SEEK_CUR))
    goto error;

  /* Info (ignoring errors) */
  ecode = 0;
  if (q4->info && q4->infosz > 0) {
    if (!bin_load(&q4->info->bin, vfs, q4->infosz, q4->infosz+2, INFO_MAX_SIZE)
        && ZZLEN(q4->info->bin) > 1
        && *ZZSTR(q4->info->bin))
    {
      /* Atari to UTF-8 conversion */
      static const uint16_t atari_to_unicode[128] = {
        0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
        0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
        0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
        0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x00df,0x0192,
        0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
        0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
        0x00e3,0x00f5,0x00d8,0x00f8,0x0153,0x0152,0x00c0,0x00c3,
        0x00d5,0x00a8,0x00b4,0x2020,0x00b6,0x00a9,0x00ae,0x2122,
        0x0133,0x0132,0x05d0,0x05d1,0x05d2,0x05d3,0x05d4,0x05d5,
        0x05d6,0x05d7,0x05d8,0x05d9,0x05db,0x05dc,0x05de,0x05e0,
        0x05e1,0x05e2,0x05e4,0x05e6,0x05e7,0x05e8,0x05e9,0x05ea,
        0x05df,0x05da,0x05dd,0x05e3,0x05e5,0x00a7,0x2227,0x221e,
        0x03b1,0x03b2,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
        0x03a6,0x0398,0x03a9,0x03b4,0x222e,0x03c6,0x2208,0x2229,
        0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
        0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x00b3,0x00af
      };
      int i,j,len = ZZLEN(q4->info->bin);
      uint8_t * s = ZZBUF(q4->info->bin);

      for (i=0; i<len && s[i]; ++i)
        ;

      j = ZZMAX(q4->info->bin);
      s[--j] = 0;
      while (i>0 && j>=i) {
        const uint8_t c = s[--i];
        uint16_t u = c < 128 ? c : atari_to_unicode[c&127];
        if (u < 0x80) {
          s[--j] = u;                   /* TODO: CR/LF conversion */
        } else if (u < 0x800 && j >= 2 ) {
          s[--j] = 0x80 | (u & 63);
          s[--j] = 0xC0 | (u >> 6);
        } else if ( j >= 3) {
          s[--j] = 0x80 | (u & 63);
          s[--j] = 0x80 | ((u >> 6) & 63);
          s[--j] = 0xE0 | (u >> 12);
        }
        assert(j >= i);
      }
      q4->info->comment = ZZSTR(q4->info->bin) + j;
      dmsg("COMMENT:\n%s\n",q4->info->comment);
    }
  }

error:
  return ecode;
}
