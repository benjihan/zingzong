/**
 * @file   zz_load.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-04
 * @brief  quartet files loader.
 */

#define ZZ_DBG_PREFIX "(lod) "
#include "zz_private.h"
#include "zingzong.h"

#ifdef __MINGW32__
# include <libgen.h>     /* no GNU version of basename() with mingw */
#endif

ZZ_EXTERN_C int song_init_header(zz_song_t const song, const void * hd);
ZZ_EXTERN_C int song_init(zz_song_t const song);
ZZ_EXTERN_C int vset_init_header(zz_vset_t const vset, const void * hd);
ZZ_EXTERN_C int vset_init(zz_vset_t const vset);

zz_err_t
song_parse(song_t *song, vfs_t vfs, uint8_t *hd, u32_t size)
{
  zz_err_t ecode;

  zz_assert( sizeof(songhd_t) ==  16 );
  zz_assert( sizeof(sequ_t)   ==  12 );

  if ( (ecode = song_init_header(song,hd)) ||
       (ecode = bin_load(&song->bin, vfs, size, 12, SONG_MAX_SIZE)) ||
       (ecode = song_init(song)) )
    bin_free(&song->bin);
  return ecode;
}

zz_err_t
song_load(song_t *song, const char *uri)
{
  uint8_t hd[16];
  vfs_t vfs = 0;
  zz_err_t ecode;

  if ( 0
       || (ecode = vfs_open_uri(&vfs,uri))
       || (ecode = vfs_read_exact(vfs,hd,16))
       || (ecode = song_parse(song,vfs,hd,0))
    );
  vfs_del(&vfs);
  return ecode;
}

/* ----------------------------------------------------------------------
 * quartet voiceset
 * ----------------------------------------------------------------------
 */

#ifdef NO_LIBC
static int
vset_guess(zz_play_t const P, const char * songuri)
{
  return -1;
}
#else

static zz_err_t
try_vset_load(vset_t * vset, const char * uri, int method)
{
  const zz_u8_t old_log = zz_log_bit(1<<ZZ_LOG_ERR,0);
  zz_err_t ecode;
  dmsg("testing voiceset method #%hu -- \"%s\"\n",
       HU(method), uri);
  ecode = vset_load(vset, uri);
  zz_log_bit(0, old_log & (1<<ZZ_LOG_ERR)); /* restore ZZ_LOG_ERROR  */
  if (ecode)
    dmsg("unable to load voice set (%hu) -- \"%s\"\n", HU(ecode), uri);
  return ecode;
}

/* This function is supposed to tell us a given URI might be case
 * sensitive. It is used by the voice guess function to generate
 * either 1 or 2 versions for each checked file extension.
 *
 * Right now it's just a hard-coded value based on host system
 * traditions.
 */
static int
uri_case_sensitive(const char * songuri)
{
#if defined WIN32 || defined _WIN32 || defined __SYMBIAN32__ || defined __MINT__
  return 0;
#else
  return 1;
#endif
}

static const char *
fileext(const char * const s)
{
  int i, l = strlen(s);
  for ( i=l-2; i>=1; --i )
    if (s[i] == '.') { l=i; i=0; }
  dmsg("extension of \"%s\" is \"%s\"\n", s, s+l);
  return s+l;
}

static zz_err_t
vset_guess(zz_play_t P, const char * songuri)
{
  const int
    lc_to_uc = 'a' - 'A',
    songlen = strlen(songuri),
    case_sensitive = uri_case_sensitive(songuri);
  const char *nud, *dot, *suf_ext;
  int idx, inud, idot, method, i, c, tr;

  enum {
    e_type_4v = 1,                    /* .4v */
    e_type_qt = 2,                    /* .qts or .qta */
    e_type_lc = 4,                    /* has lowercase char */
    e_type_uc = 8,                    /* has uppercase char */
  } e_type = 0;

  /* $$$ IMPROVE ME.
   *
   * GB: Tilde (~) is being used by Windows short names (8.3). However
   * most short names probably won't work anyway.
   */
  static const char num_sep[] = " -_~.:,#"; /* prefix song num */
#if 0
  static const char ext_brk[] = ".:/\\"; /* breaks extension lookup */
  static const char pre_sep[] = "/\\.:-_([{";  /* prefix "4v" */
  static const char suf_sep[] = "/\\.:-_)]}";  /* suffix "4v" */
#endif
  if (!P || !songuri || !*songuri)
    return E_ARG;

  dmsg("URI is%s case sensitive on this system\n",
       case_sensitive ? "" : " NOT");

  zz_assert ( ! P->vseturi );
  if ( P->vseturi ) {
    wmsg("delete previous voiceset URI -- \"%s\"\n", P->vseturi->ptr);
    zz_strdel( &P->vseturi );
    zz_assert ( ! P->vseturi );
  }

  nud = basename((char*)songuri);       /* cast for mingw */
  /* GB: we should be going with gnu_basename(). If anyother version
   *     of basename() is used it has to keep the return value inside
   *     songuri[]. for some reason even gnu_basename() sometime
   *     returns a static empty string. While I understand the reason
   *     when the path is nil, any other case it could return the
   *     address of the trailing zero rather than this static
   *     string. Anyway things being what they are we'll do it this
   *     way on our own.
   */
  if (!*nud) nud = songuri+songlen;
  zz_assert( nud >= songuri && nud <= songuri+songlen );
  if ( ! (nud >= songuri && nud <= songuri+songlen ) )
    return E_666;
  inud = nud-songuri;                /* index of basename in songuri[] */
  dot  = fileext(nud);
  idot = !*dot ? 0 : dot-songuri;       /* index of '.' in songuri[] */
  if (idot) {
    for (i=idot; !!(c = songuri[i]); ++i)
      if (islower(c)) e_type |= e_type_lc;
      else if (isupper(c)) e_type |= e_type_uc;
    if (!strcasecmp(dot+1,"4v"))
      e_type |= e_type_4v;
    else if (!strcasecmp(dot+1,"qts") || !strcasecmp(dot+1,"qta") )
      e_type |= e_type_qt;
    dmsg("extension \"%s\" of type $%X\n",dot,e_type);
  }

  P->vseturi = zz_strnew(songlen+32);
  if (unlikely(!P->vseturi))
    return E_MEM;

  for (idx=method=0; ++method; ) {
    char * const s = P->vseturi->ptr;

    *s = 0;
    tr = (e_type & e_type_uc) ? lc_to_uc : 0; /* translation */

    switch (method) {

      /* 1&2: change original extension starting with the natural
       *      buddy extension (4v->set qt?->smp). Try removing song
       *      number suffix as well.
       */
    case 1:
      /* No extension ? ignore this method */
      if (!idot) { method = 2; break; } /* *3* on next loop */
      idx = 0;                          /* restart the method */
    case 2: {
      if (idx < 7)
        method = 1;                     /* *2* on next loop */
      zz_assert(idx < 8);

      /* GB: Every 2nd idx is the same as its predecessor but using
       *     the other case. The predecessor case depends on the case
       *     of the song file extension. It follows this rule --
       *     lowercase unless there is a single uppercase char in the
       *     original file extension.
       */
      if (idx & 1) {
        tr ^= lc_to_uc;
        zz_assert ( case_sensitive );
      }

      /* 0->.set 1->.SET 2->*.set 3->*.SET
       * 4->.smp 5->.SMP 6->*.smp 7->*.SMP
       */

      /* starts with ".set" for ".4v" songs else uses ".smp" first */
      suf_ext = !(e_type & e_type_4v) ^ (idx >= 4) ? "smp" : "set";

      /* Copy up to the extension. */
      zz_memcpy(s, songuri, idot+1);

      i = idot;
      if ( ( idx & 2 ) ) {
        /* Try to remove the song number suffix. */
        if (i > inud+1 && isdigit(s[i-1])) {
          --i;
          i -= (i > inud+1 && isdigit(s[i-1]));
          i -= (i > inud+1 && strchr(num_sep,s[i-1]));
        } else {
         /* No song number suffix. Skip this one and the next which
           * is the same with the alternate case. */
          idx += 2;
          *s = 0;
          break;
        }
      }

      /* Copy the new extension with case transformation. */
      for (s[i++] = s[idot]; !!(c = *suf_ext++); ++i)
        s[i] = c - (islower(c) ? tr : 0);
      s[i] = 0;
      zz_assert(i < P->vseturi->max);
      idx += 2 - case_sensitive;        /* next index */
    } break;

      /* 3&4: Append extension. */
    case 3:
      idx = 0;
    case 4:
      if (idx < 3)
        method = 3;                       /* *4* on next loop */
      zz_assert(idx < 4);

      /* Get extension at this index. */
      suf_ext = !(e_type & e_type_4v) ^ (idx >= 2) ? "smp" : "set";

      /* Get case translation. */
      if (idx & 1) {
        if (!case_sensitive) {
          ++idx; break;
        }
        tr ^= lc_to_uc;
      }
      zz_assert( tr == 0 || tr == lc_to_uc );

      zz_memcpy(s, songuri, songlen);
      s[songlen] = '.';
      zz_memcpy(s+songlen+1, suf_ext, 4);
      ++idx;
      break;


    default:
      idx    = -1;                      /* restart next method */
      method = -2;                      /* continue on last resort */
    case -1: {
      /* Last resort filenames. */
      static const char * vnames[] = { "voice.set", "SMP.set" };
      ++idx;
      suf_ext = 0;

      if ( e_type & e_type_4v )
        suf_ext = vnames[0];            /* ".4v" => "voice.set"  */
      else if ( e_type & e_type_qt )
        suf_ext = vnames[1];            /* ".qt?" => "SMP.set" */
      else
        suf_ext = vnames[idx & 1];      /* .* => Alternate */

      if (!case_sensitive) {
        if ( ! (e_type & (e_type_4v|e_type_qt) ) && idx == 0 )
          method = -2;                   /* *-1* on next loop */
      }
      else {
        if (idx & 2) tr ^= lc_to_uc;
        if ( ((e_type & (e_type_4v|e_type_qt)) && idx == 0) || idx < 3 )
          method = -2;                    /* *-1* on next loop */
      }

      if (suf_ext) {
        zz_memcpy(s,songuri,inud);
        for (i=0; (c = suf_ext[i]); ++i)
          s[inud+i] = c - (islower(c) ? tr : 0);
        s[inud+i] = 0;
      }
    } break;

    /* case 7: */
    /*   /\* Replace the latest "4v" word by "set" *\/ */
    /*   c = i = (idot ? idot : songlen); */
    /*   while (i>0 && songuri[--i] != '4'); */

    /*   dmsg("we are : %d \"%s\"\n", i, songuri+i); */
    /*   if (songuri[i] == '4' && tolower(songuri[i+1]) == 'v' && */
    /*       (i+0 == 0 || strchr(pre_sep,songuri[i-1])) && */
    /*       (i+2 == c || strchr(suf_sep,songuri[i+2]))) { */
    /*     const int shift = songuri[i+1] - 'v'; */
    /*     zz_memcpy(s,songuri,i); */
    /*     s[i++] = 's' + shift; */
    /*     s[i++] = 'e' + shift; */
    /*     s[i++] = 't' + shift; */
    /*     while ( (s[i] = songuri[i-1]) != 0 ) ++i; */
    /*   } */
    /*   break; */
      break;

    }

    if (*s && E_OK == try_vset_load(&P->vset, s, method) )
      return E_OK;
  }

  emsg("unable to find a voice set for -- \"%s\"\n", songuri);

  if (P->vset.bin)
    bin_free(&P->vset.bin);
  zz_assert( ! P->vset.bin );

  if (P->vseturi)
    zz_strdel(&P->vseturi);
  zz_assert( ! P->vseturi );

  return E_SET;
}

#endif

zz_err_t (*zz_guess_vset)(zz_play_t const, const char *) = vset_guess;

zz_err_t
vset_parse(vset_t *vset, vfs_t vfs, uint8_t *hd, u32_t size)
{
  zz_err_t ecode;
  if (0
      /* Parse header and instruments */
      || (ecode = vset_init_header(vset, hd))
      || (ecode = bin_load(&vset->bin, vfs, size, VSET_XSIZE, VSET_MAX_SIZE))
      || (ecode = vset_init(vset)))
    bin_free(&vset->bin);
  return ecode;
}

zz_err_t
vset_load(vset_t *vset, const char *uri)
{
  uint8_t hd[222];
  vfs_t vfs = 0;
  zz_err_t ecode;

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
zz_err_t
q4_load(vfs_t vfs, q4_t *q4)
{
  zz_err_t ecode = E_SNG;
  uint8_t hd[222];

  zz_assert( vfs_tell(vfs) == 20 );

  ecode = E_SNG;
  if (q4->songsz < 16 + 12*4) {
    dmsg("invalid .4q song size (%lu) -- %s", LU(q4->songsz), vfs_uri(vfs));
    goto error;
  }

  ecode = E_SET;
  if (q4->vset && q4->vsetsz < 222) {
    dmsg("invalid .4q set size (%lu) -- %s", LU(q4->vsetsz), vfs_uri(vfs));
    goto error;
  }

  /* Read song */
  if (q4->song) {
    ecode = vfs_read_exact(vfs, hd, 16);
    if (E_OK == ecode)
      ecode = song_parse(q4->song, vfs, hd, q4->songsz-16);
    if (E_OK == ecode && q4->vset)
      q4->vset->iused = q4->song->iused;
  } else {
    ecode = vfs_seek(vfs, q4->songsz, ZZ_SEEK_CUR);
  }
  if (E_OK != ecode)
    goto error;

  /* Voice set */
  if (q4->vset) {
    ecode = vfs_read_exact(vfs,hd,222);
    if (E_OK == ecode)
      ecode = vset_parse(q4->vset, vfs, hd, q4->vsetsz-222);
  } else
    ecode = vfs_seek(vfs, q4->vsetsz, ZZ_SEEK_CUR);

  if (E_OK != ecode)
    goto error;

  /* Info (ignoring errors) */
  if (q4->info && q4->infosz > 0) {
    if (E_OK ==
        bin_load(&q4->info->bin,vfs,q4->infosz, q4->infosz+2,INFO_MAX_SIZE)
        && q4->info->bin->len > 1 && *q4->info->bin->buf)
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
      int i,j,len = q4->info->bin->len;
      uint8_t * s = q4->info->bin->buf;

      for (i=0; i<len && s[i]; ++i)
        ;

      j = q4->info->bin->max;
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
        zz_assert(j >= i);
      }
      q4->info->comment = (char *)q4->info->bin->buf+j;
      dmsg("COMMENT:\n%s\n",q4->info->comment);
    }
  }

  ecode = E_OK;
error:
  return ecode;
}

/**
 * @param  vseturi  0:guess "":song only
 */
zz_err_t
zz_load(play_t * P, const char * songuri, const char * vseturi, zz_u8_t * pfmt)
{
  uint8_t hd[20];
  vfs_t inp = 0;
  zz_err_t ecode;
  zz_u8_t format = ZZ_FORMAT_UNKNOWN;

  for (ecode = !P || !songuri ? E_ARG : E_OK; ecode == E_OK ; ) {

    ecode = vfs_open_uri(&inp, songuri);
    if (ecode == E_OK)
      /* Read song header */
      ecode = vfs_read_exact(inp, hd, 16);
    if (ecode != E_OK)
      break;

    /* Check for .4q "QUARTET" magic id */
    if (!zz_memcmp(hd,"QUARTET",8)) {
      q4_t q4;

      ecode = vfs_read_exact(inp, hd+16, 4);
      if (E_OK != ecode)
        break;

      format = ZZ_FORMAT_4Q;
      ecode = E_SYS;
      zz_assert ( ! P->songuri );
      zz_assert ( ! P->vseturi );
      zz_assert ( ! P->infouri );

      ecode = E_MEM;
      P->songuri = zz_strset(P->songuri, songuri);
      if (!P->songuri)
        break;
      P->vseturi = zz_strdup(P->songuri);
      P->infouri = zz_strdup(P->songuri);

      q4.song = &P->song; q4.songsz = u32(hd+8);
      q4.vset = &P->vset; q4.vsetsz = u32(hd+12);
      q4.info = &P->info; q4.infosz = u32(hd+16);
      dmsg("QUARTET header [sng:%lu set:%lu inf:%lu]\n",
           LU(q4.songsz), LU(q4.vsetsz), LU(q4.infosz));
      ecode = q4_load(inp,&q4);
      vfs_del(&inp);
      break;
    }

    /* Load song */
    ecode = song_parse(&P->song, inp, hd, 0);
    if (unlikely(ecode))
      break;
    format = ZZ_FORMAT_4V;
    vfs_del(&inp);

    ecode = E_MEM;
    P->songuri = zz_strset(P->songuri, songuri);
    if (unlikely(!P->songuri))
      break;
    ecode = E_OK;

    zz_assert(inp == 0);
    if (!vseturi)
      ecode = vset_guess(P, songuri);
    else {
      if (*vseturi) {
        ecode = vset_load(&P->vset,vseturi);
        if (ecode)
          break;
        ecode = E_MEM;
        P->vseturi = zz_strset(P->vseturi,vseturi);
        if (unlikely(!P->vseturi))
          break;
      } else {
        dmsg("skipped voice set as requested\n");
        ecode = E_OK;
      }
    }

    /* Finally */
    zz_assert( ecode == E_OK );
    P->format = format;
    break;
  }

  vfs_del(&inp);
  if (ecode) {
    zz_song_wipe(&P->song);
    zz_vset_wipe(&P->vset);
    zz_info_wipe(&P->info);
  }

  if (pfmt)
    *pfmt = format;

  return ecode;
}


/* ----------------------------------------------------------------------
 *   Clean and Free
 * ----------------------------------------------------------------------
 */

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

void zz_song_wipe(zz_song_t song)
{
  memb_wipe((struct memb_s *)song, sizeof(*song));
}

void zz_vset_wipe(zz_vset_t vset)
{
  memb_wipe((struct memb_s *)vset, sizeof(*vset));
}

void zz_info_wipe(zz_info_t info)
{
  memb_wipe((struct memb_s *)info, sizeof(*info));
}
