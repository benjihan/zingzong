/**
 * @file   zz_load.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet files loader.
 */

#include "zz_private.h"
#include "zingzong.h"

/* #include <stdlib.h>                     /\* qsort *\/ */
#include <ctype.h>

#include <alloca.h>

int
song_parse(song_t *song, vfs_t vfs, uint8_t *hd, uint_t size)
{
  int ecode;
  if ( (ecode = song_init_header(song,hd)) ||
       (ecode = bin_load(&song->bin, vfs, size, 12, SONG_MAX_SIZE)) ||
       (ecode = song_init(song)) )
    bin_free(&song->bin);
  return ecode;
}

int
song_load(song_t *song, const char *uri)
{
  uint8_t hd[16];
  vfs_t vfs = 0;
  int ecode;

  if ( 0
       || (ecode = vfs_open_uri(&vfs,uri))
       || (ecode = vfs_read_exact(vfs,hd,16))
       || (ecode = song_parse(song,vfs,hd,0)))
    ;
  vfs_del(&vfs);
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

static int
vset_guess(zz_play_t const P, const char * songuri)
{
  int method, i, c, songlen = strlen(songuri), ext = 0;
  char *s;

   /* $$$ improve me */
  static const char ext_brk[] = ".:/\\"; /* breaks extension lookup */
  static const char num_sep[] = " -_~.:,#"; /* prefix song num */
  static const char pre_sep[] = "/\\.:-_([{";  /* prefix "4v" */
  static const char suf_sep[] = "/\\.:-_)]}";  /* suffix "4v" */

  P->vseturi = zz_stralloc(songlen+32);
  if (!P->vseturi)
    return E_SYS;
  s = ZZSTR(P->vseturi);
#if defined(WIN32) || defined(SYMBIAN32)
  /* Detect and skip drive letter */
  if (isalpha((int)s[0]) && s[1] == ':') {
    *s++ = *songuri++;
    *s++ = *songuri++;
    songlen -= 2;
  }
#endif

  for (method=0; ++method; ) {
    *s = 0;
    switch (method) {
    case 1: case 2:
      /* Simply add ".set" */
      zz_memcpy(s,songuri,songlen);
      zz_memcpy(s+songlen,method==1?".set":".SET",5);
      break;

    case 3: case 4:
      /* Change extension to ".set" */
      if (songlen < 4)
        break;
      if (ext) {
        c = '.';
        i = ext;
      } else {
        for (c=0, i=songlen-2;
             i >= 1 &&  songlen-i <= 5 && !strchr(ext_brk, c=songuri[i]);
             --i)
          ;
      }

      if (c == '.') {
        ext = i;                        /* Keep that in mind */
        zz_memcpy(s,songuri,i);
        zz_memcpy(s+i,method==3?".set":".SET",5);
      }
      break;

    case 5: case 6:
      /* Eliminate song number just before the extension before adding
       * the .set */
      i = (ext ? ext : songlen) - 1;
      if (i>0 && isdigit(songuri[i])) {
        do { --i; } while (i>0 && isdigit(songuri[i]));
        if (i>0 && strchr(num_sep, songuri[i])) --i;
        zz_memcpy(s,songuri,i+1);
        zz_memcpy(s+i+1,method==5?".set":".SET",5);
      }
      break;

    case 7:
      /* Replace the latest "4v" word by "set" */
      c = i = (ext ? ext : songlen);
      while (i>0 && songuri[--i] != '4');

      dmsg("we are : %d \"%s\"\n", i, songuri+i);
      if (songuri[i] == '4' && tolower(songuri[i+1]) == 'v' &&
          (i+0 == 0 || strchr(pre_sep,songuri[i-1])) &&
          (i+2 == c || strchr(suf_sep,songuri[i+2]))) {
        const int shift = songuri[i+1] - 'v';
        zz_memcpy(s,songuri,i);
        s[i++] = 's' + shift;
        s[i++] = 'e' + shift;
        s[i++] = 't' + shift;
        while ( (s[i] = songuri[i-1]) != 0 ) ++i;
      }
      break;

    default:
      method = -1;
    }

    if (*s) {
      msg_f msgsave = msgfunc;
      int res;
      dmsg("Testing #%i voiceset \"%s\"\n", method, ZZSTR(P->vseturi));
      bin_free(&P->vset.bin);
      zz_memclr(&P->vset,sizeof(P->vset));
      msgfunc = 0;
      res = vset_load(&P->vset,ZZSTR(P->vseturi));
      msgfunc = msgsave;
      if (!res)
        return E_OK;
    }
  }
  wmsg("could not find suitable set for \"%s\"\n", songuri);
  return E_SET;
}

int (*zz_guess_vset)(zz_play_t const, const char *) = vset_guess;

int
vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, uint_t size)
{
  int ecode;
  if (0
    /* Parse header and instruments */
      || (ecode = vset_init_header(vset, hd))
      || (ecode = bin_load(&vset->bin, vfs, size, VSET_XSIZE, VSET_MAX_SIZE))
      || (ecode = vset_init(vset)))
    bin_free(&vset->bin);
  return ecode;
}

int
vset_load(vset_t *vset, const char *uri)
{
  uint8_t hd[222];
  vfs_t vfs = 0;
  int ecode;

  if ( 0
       || (ecode = vfs_open_uri(&vfs,uri))
       || (ecode = vfs_read_exact(vfs,hd,222))
       || (ecode = vset_parse(vset,vfs,hd,0)))
    ;
  vfs_del(&vfs);
  return ecode;
}


/**
 * Load .4q file (after header).
 */
int
q4_load(vfs_t vfs, q4_t *q4)
{
  int ecode = E_SNG;
  uint8_t hd[222];

  assert(vfs_tell(vfs) == 20);

  ecode = E_SNG;
  if (q4->songsz < 16 + 12*4) {
    emsg("invalid .4q song size (%u) -- %s", q4->songsz, vfs_uri(vfs));
    goto error;
  }

  ecode = E_SET;
  if (q4->vset && q4->vsetsz < 222) {
    emsg("invalid .4q set size (%u) -- %s", q4->vsetsz, vfs_uri(vfs));
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
    if (q4->vset)
      q4->vset->iused = q4->song->iused;
  } else if (vfs_seek(vfs, q4->songsz, ZZ_SEEK_CUR))
    goto error;

  /* Voice set */
  ecode = E_INP;
  if (q4->vset) {
    if (vfs_read_exact(vfs,hd,222) < 0)
      goto error;
    ecode = vset_parse(q4->vset, vfs, hd, q4->vsetsz-222);
    if (ecode)
      goto error;
  } else if (vfs_seek(vfs, q4->vsetsz, ZZ_SEEK_CUR))
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

int
zz_load(play_t * P, const char * songuri, const char * vseturi)
{
  uint8_t hd[20];
  vfs_t inp = 0;
  int ecode;

  for (ecode = !P || !songuri ? E_ARG : 0; !ecode;) {
    ecode = E_SYS;
    if (vfs_open_uri(&inp, songuri))
      break;

    /* Read song header */
    ecode = E_INP;
    if (vfs_read_exact(inp, hd, 16))
      break;

    /* Check for .4q "QUARTET" magic id */
    if (!zz_memcmp(hd,"QUARTET",8)) {
      q4_t q4;

      ecode = E_INP;
      if (vfs_read_exact(inp, hd+16, 4))
        break;

      P->format = ZZ_FORMAT_4Q;
      ecode = E_SYS;
      P->songuri = P->infouri = P->vseturi = zz_strdup(songuri);
      if (!P->songuri)
        break;
      q4.song = &P->song; q4.songsz = u32(hd+8);
      q4.vset = &P->vset; q4.vsetsz = u32(hd+12);
      q4.info = &P->info; q4.infosz = u32(hd+16);
      dmsg("QUARTET header [sng:%u set:%u inf:%u]\n",
           q4.songsz, q4.vsetsz, q4.infosz);
      ecode = q4_load(inp,&q4);
      vfs_del(&inp);
      break;
    }

    /* Load song */
    ecode = E_SNG;
    if (song_parse(&P->song, inp, hd, 0))
      break;
    P->format = ZZ_FORMAT_4V;
    vfs_del(&inp);

    ecode = E_SYS;
    if (P->songuri = zz_strdup(songuri), !P->songuri)
      break;

    assert(inp == 0);
    if (vseturi) {
      ecode = E_SYS;
      if (vset_load(&P->vset,vseturi))
        break;
      if (P->vseturi = zz_strdup(vseturi), !P->vseturi)
        break;
      ecode = E_OK;
    } else if (ecode = vset_guess(P,songuri), ecode) {
      if (ecode < 0)
        ecode = E_SET;
    }
    break;
  }

  vfs_del(&inp);
  if (ecode) {
    bin_free(&P->song.bin);
    zz_memclr(&P->song,sizeof(P->song));
    zz_strfree(&P->songuri);

    bin_free(&P->vset.bin);
    zz_memclr(&P->vset,sizeof(P->vset));
    zz_strfree(&P->vseturi);
  }

  return ecode;
}
