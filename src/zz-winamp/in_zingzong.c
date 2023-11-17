/**
 * @file    in_zingzong.c
 * @date    2017-07-29
 * @brief   zingzong plugin for winamp 5.5
 * @author  https://github.com/benjihan
 *
 * ----------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2017-2023 Benjamin Gerard AKA Ben/OVR.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/* generated config header */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define ZZ_DBG_PREFIX "(amp) "

/* windows */
#include "in_zingzong.h"

/* zingzong */
#include "../zz_private.h"

/* libc */
#include <malloc.h>			/* malloca */
#include <libgen.h>
#include <limits.h>

/* winamp 2 */
#include "winamp/in2.h"
#undef	 _MSC_VER			/* fix intptr_t redefinition */
#define	 _MSC_VER 2000
#include "winamp/wa_ipc.h"
#include "winamp/ipc_pe.h"

/*****************************************************************************
 * Plugin private data.
 ****************************************************************************/

typedef unsigned int uint_t;
ZZ_EXTERN_C
zz_vfs_dri_t zz_file_vfs(void);

const char me[] = "in_zingzong";

enum {
  NO_MEASURE=0, MEASURE, MEASURE_ONLY, MEASURE_CONFIG
};

static HANDLE g_lock;			/* mutex handle            */
struct xinfo_s {
  HANDLE lock;				/* mutex for extended info */
  unsigned int ready:1, fail:1;		/* status flags.           */
  unsigned int ms;			/* duration.               */
  char * format;			/* file format (static)    */
  char * uri;				/* load uri (in data[])    */
  char * album;				/* album (in data[])       */
  char * title;				/* title (in data[])       */
  char * artist;			/* artist (in data[])      */
  char * ripper;			/* ripper (in data[])      */
  char data[4096];			/* Buffer for infos        */
};
typedef struct xinfo_s xinfo_t;

static DWORD	 g_tid;			/* thread id               */
static HANDLE	 g_thdl;		/* thread handle           */
static play_t	 g_play;		/* play emulator instance  */
static zz_info_t g_info;		/* current file info       */
static xinfo_t	 x_info;		/* unique x_info           */
static zz_u32_t	 g_spr = SPR_DEF;	/* sampling rate (hz)      */
static int	 g_maxlatency;		/* max latency in ms       */
static zz_u8_t	 g_mid = ZZ_MIXER_DEF;	/* mixer id                */
static zz_u8_t	 g_tr_measure = MEASURE_CONFIG;
static zz_u8_t	 g_map = 0;		/* channel mapping         */
static zz_u16_t	 g_lr8 = BLEND_DEF;	/* channel blending        */
static zz_i32_t	 g_dms;			/* default play time (ms)  */
static int32_t	 g_pcm[576*2];		/* buffer for DSP filters  */
static volatile LONG g_playing;		/* true while playing      */
static volatile LONG g_stopreq;		/* stop requested          */
static volatile LONG g_paused;		/* pause status            */

/*****************************************************************************
 * Declaration
 ****************************************************************************/

/* The decode thread */
static DWORD WINAPI playloop(LPVOID b);
static void init();
static void quit();
static void config(HWND);
static void about(HWND);
static	int infobox(const char *, HWND);
static	int isourfile(const char *);
static void pause();
static void unpause();
static	int ispaused();
static	int getlength();
static	int getoutputtime();
static void setoutputtime(int);
static void setvolume(int);
static void setpan(int);
static	int play(const char *);
static void stop();
static void getfileinfo(const in_char *, in_char *, int *);
static void seteq(int, char *, int);

/*****************************************************************************
 * LOCKS
 ****************************************************************************/

static inline int lock(HANDLE h)
{ return WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0; }

static inline int lock_noblock(HANDLE h)
{ return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }

static inline void unlock(HANDLE h)
{ ReleaseMutex(h); }

static inline LONG atomic_set(LONG volatile * ptr, LONG v)
{ return InterlockedExchange(ptr,v); }

static inline LONG atomic_get(LONG volatile * ptr)
{ return *ptr; }

static inline
play_t * play_lock(void)
{ return lock(g_lock) ? &g_play : 0; }

static inline
void play_unlock(play_t * play)
{ if (play == &g_play) unlock(g_lock); }

play_t * PlayLock() { return play_lock(); }
void	 PlayUnlock(play_t *play) { play_unlock(play); }

static inline
struct xinfo_s * info_lock()
{ return lock(x_info.lock) ? &x_info : 0; }

static inline
void info_unlock(struct xinfo_s * xinfo)
{ if (xinfo == &x_info) unlock(x_info.lock); }

static
/*****************************************************************************
 * THE INPUT MODULE
 ****************************************************************************/
In_Module g_mod =
{
  IN_VER,		/* Input plugin version as defined in in2.h */
  (char*)
  "ZingZong (Quartet music player) v" PACKAGE_VERSION, /* Description */
  0,			      /* hMainWindow (filled in by winamp)  */
  0,			      /* hDllInstance (filled in by winamp) */
  (char*)
  "4q\0"   "Quartet bundle (*.4q)\0"
  "4v\0"   "Quartet score (*.4v)\0"
  "qts\0"  "Quartet score (*.qts)\0"
  "qta\0"  "Quartet score (*.qta)\0"
  ,
  0,				      /* is_seekable */
  1,				      /* uses output plug-in system */

  config,
  about,
  init,
  quit,
  getfileinfo,
  infobox,
  isourfile,
  play,
  pause,
  unpause,
  ispaused,
  stop,

  getlength,
  getoutputtime,
  setoutputtime,

  setvolume,
  setpan,

  0,0,0,0,0,0,0,0,0,	 /* visualization calls filled in by winamp */
  0,0,			 /* dsp calls filled in by winamp */
  seteq,		 /* set equalizer */
  NULL,			 /* setinfo call filled in by winamp */
   0			  /* out_mod filled in by winamp */
};

static
/*****************************************************************************
 * CONFIG DIALOG
 ****************************************************************************/
void config(HWND hwnd)
{
  config_t cfg;

  cfg.mid = g_mid;
  cfg.spr = g_spr;
  cfg.dms = g_dms;
  cfg.map = MAKELPARAM(g_map,g_lr8);

  if (0x1337 == ConfigDialog(g_mod.hDllInstance, hwnd, &cfg)) {
    ConfigSave(&cfg);		   /* Save might re-check the value */
    g_mid = cfg.mid;
    g_spr = cfg.spr;
    g_dms = cfg.dms;
    g_map = LOWORD(cfg.map);
    g_lr8 = HIWORD(cfg.map);;
  }
}

static
/*****************************************************************************
 * ABOUT DIALOG
 ****************************************************************************/
void about(HWND hwnd)
{
  AboutDialog(g_mod.hDllInstance, hwnd);
}

static
/*****************************************************************************
 * INFO DIALOG
 ****************************************************************************/
int infobox(const char * uri, HWND hwnd)
{
  /* $$$ XXX TO DO */
  return INFOBOX_UNCHANGED;
}

static
/*****************************************************************************
 * FILE DETECTION
 ****************************************************************************/
int isourfile(const char * uri)
{
  char * s;
  const char * nud, *ext;
  int len = uri ? strlen(uri)+1 : 0;

  if ( !len || !(s = _malloca(len)) )
    return 0;
  memcpy(s,uri,len);
  len = (nud = basename(s)) && (ext = strrchr(nud,'.')) &&
    (
      0
      || ! strcasecmp(ext,".4q")
      || ! strcasecmp(ext,".4v")
      || ! strcasecmp(ext,".qts")
      || ! strcasecmp(ext,".qta")
      || ! strncasecmp(nud,"4q.",3)
      || ! strncasecmp(nud,"4v.",3)
      || ! strncasecmp(nud,"qts.",4)
      || ! strncasecmp(nud,"qta.",4)
      );
  _freea(s);
  return len;
}

/*****************************************************************************
 * PAUSE
 ****************************************************************************/
static void pause() {
  atomic_set(&g_paused,1);
  g_mod.outMod->Pause(1);
}

static void unpause() {
  atomic_set(&g_paused,0);
  g_mod.outMod->Pause(0);
}

static int ispaused() {
  return atomic_get(&g_paused);
}

static
/*****************************************************************************
 * GET LENGTH (MS)
 ****************************************************************************/
int getlength()
{
  int ms = 0;				/* 0 or -1 ? */
  if (atomic_get(&g_playing)) {
    play_t * const P = play_lock();
    if (P) {
      ms = (g_info.len.ms > INT_MAX) ? INT_MAX : g_info.len.ms;
      play_unlock(P);
    }
  }
  return ms;
}

static
/*****************************************************************************
 * GET CURRENT POSITION (MS)
 ****************************************************************************/
int getoutputtime()
{
  return g_mod.outMod->GetOutputTime();
}

static
/*****************************************************************************
 * SET CURRENT POSITION (MS)
 ****************************************************************************/
void setoutputtime(int ms)
{
  /* Not supported ATM. */
}

static
/*****************************************************************************
 * SET VOLUME
 ****************************************************************************/
void setvolume(int volume)
{
  g_mod.outMod->SetVolume(volume);
}

static
/*****************************************************************************
 * SET PAN
 ****************************************************************************/
void setpan(int pan)
{
  g_mod.outMod->SetPan(pan);
}

static
/*****************************************************************************
 * SET EQUALIZER : Do nothing to ignore
 ****************************************************************************/
void seteq(int on, char data[10], int preamp) {}

static void clean_close(void)
{
  if (g_thdl) {
    TerminateThread(g_thdl,1);
    CloseHandle(g_thdl);
    g_thdl = 0;
  }
  atomic_set(&g_playing,0);
  zz_info(0,&g_info);
  zz_close(&g_play);
  g_mod.outMod->Close();		/* Close output system */
  g_mod.SAVSADeInit();			/* Shutdown visualization */
}

static
/*****************************************************************************
 * STOP
 ****************************************************************************/
void stop()
{
  play_t * P;
  if (P = play_lock(), P) {
    atomic_set(&g_stopreq,1);
    if (g_thdl) {
      switch (WaitForSingleObject(g_thdl,10000)) {
      case WAIT_OBJECT_0:
	CloseHandle(g_thdl);
	g_thdl = 0;
      }
    }
    clean_close();
    play_unlock(P);
  }
}

static zz_err_t
load(zz_play_t P, zz_info_t *I, const char * uri, zz_u8_t measure)
{
  zz_err_t ecode = E_INP;
  zz_i32_t dms=g_dms, spr=g_spr, mid=g_mid, map=g_map, lr8=g_lr8;

  zz_assert( P );
  zz_assert( I );
  zz_assert( (P==&g_play && I==&g_info) || (P!=&g_play && I!=&g_info) );
  zz_assert( uri );
  zz_assert( *uri );

  dmsg("load: <%hu>(%s) fmt:%hu mid:%hu spr:%lu %sdms:%lu\n    \"%s\"\n",
       HU(measure),
       P == &g_play ? "play" : measure == MEASURE_ONLY ? "info" : "code",
       HU(P->format), HU(mid), LU(spr),
       dms<0?"forced-":"", LU(dms<0?~dms:dms),
       uri);

  zz_assert( P->format == ZZ_FORMAT_UNKNOWN );
  zz_assert( P->core.vset.bin == 0 );
  zz_assert( P->core.song.bin == 0 );
  zz_assert( P->info.bin == 0 );
  zz_memclr(P,sizeof(*P));

  /* Measure-only mode don't need to load voice-set */
  ecode = zz_load(P, uri, measure == MEASURE_ONLY ? "" : 0, 0);
  if (ecode)
    goto error_no_close;
  zz_assert( P->format != ZZ_FORMAT_UNKNOWN );

  /* Run config dialog (transcoder) */
  if (measure == MEASURE_CONFIG) {
    config_t cfg;
    cfg.mid = mid;
    cfg.spr = spr;
    cfg.dms = dms;
    cfg.map = MAKELPARAM(map,lr8);
    if (0x1337 == ConfigDialog(DLGHINST, DLGHWND, &cfg)) {
      mid = cfg.mid;
      spr = cfg.spr;
      dms = cfg.dms;
      map = LOWORD(cfg.map);
      lr8 = HIWORD(cfg.map);
    }
  }

  if (dms < 0) {
    dms = ~dms;
    dmsg("FORCED TIME: %lu-ms\n", LU(dms));
  } else {
    dms = ZZ_EOF;
  }
  ecode = zz_init(P, 0, dms);
  dmsg("zz_init(rate:0 -> [%hu]\n", HU(ecode));

  if (!ecode && measure != MEASURE_ONLY) {
    ecode = zz_setup(P, mid, spr);
    dmsg("zz_setup(mid:%hu spr:%lu) -> [%hu]\n",
	 HU(mid), LU(spr), HU(ecode));
    zz_core_blend(&P->core, map, lr8);
    dmsg("zz_blend(map:A%c lr8:%hu\n", 'B'+map, HU(lr8));
  }

  if (ecode)
    goto error_close;

error_close:
  if (ecode)
    zz_close(P);

error_no_close:
  if (I) {
    zz_info(ecode ? 0 : P, I);
    if (dms == ZZ_EOF)
      dms = I->len.ms;
    else
      I->len.ms = dms;
  }

  dmsg("load (%s) -- <%s> \"%s\" %lu ms \n", ecode ? "ERR" :"OK",
       I?I->fmt.str:"?", uri ? uri : "(nil)", LU(dms));

  return ecode;
}

static
/*****************************************************************************
 * PLAY
 *
 * @retval  0 on success
 * @reval  -1 on file not found
 * @retval !0 stopping winamp error
 ****************************************************************************/
int play(const char * uri)
{
  int err = 1;

  if (!uri || !*uri)
    return -1;

  dmsg("play -- '%s'\n", uri);

  if (!lock_noblock(g_lock))
    goto cantlock;

  /* Safety net */
  if (g_thdl || atomic_get(&g_playing))
    goto inused;

  /* cleanup */
  g_maxlatency = 0;
  g_stopreq    = 0;
  g_paused     = 0;

  /* Load and initialize file */
  err = -1;
  if (load(&g_play, &g_info, uri, MEASURE))
    goto exit;

  /* Init output module */
  g_maxlatency = g_mod.outMod->Open(g_play.core.spr, 2, 16, 0, 0);
  if (g_maxlatency < 0)
    goto exit;

  /* Set default volume */
  g_mod.outMod->SetVolume(-666);

  /* Init info and visualization stuff */
  g_mod.SetInfo(g_info.sng.khz, g_play.core.spr/1000, 2, 1);
  g_mod.SAVSAInit(g_maxlatency, g_play.core.spr);
  g_mod.VSASetInfo(g_play.core.spr, 2);

  /* Init play thread */
  g_thdl = (HANDLE)
    CreateThread(NULL,		      /* Default Security Attributs */
		 0,		      /* Default stack size         */
		 (LPTHREAD_START_ROUTINE)playloop, /* Thread call   */
		 (LPVOID) 0,	      /* Thread cookie              */
		 0,		      /* Thread status              */
		 &g_tid		      /* Thread id                  */
      );
  err = !g_thdl;

exit:
  if (err)
    clean_close();

inused:
  unlock(g_lock);

cantlock:
  return err;
}


static
/*****************************************************************************
 * GET FILE INFO
 ****************************************************************************/
/*
 * this is an odd function. It is used to get the title and/or length
 * of a track.  If filename is either NULL or of length 0, it means
 * you should return the info of lastfn. Otherwise, return the
 * information for the file in filename.  if title is NULL, no title
 * is copied into it.  if msptr is NULL, no length is copied
 * into it.
 ****************************************************************************/
void getfileinfo(const in_char *uri, in_char *title, int *msptr)
{
  play_t    *P=0;
  zz_info_t *I=0;

  struct play_info_s {
    play_t    play;
    zz_info_t info;
  } * tmp = 0;

  if (title) *title = 0;
  if (msptr) *msptr = 0;

  if (!uri || !*uri) {
    uri = 0;
    if (P = play_lock(), !P) {
      emsg("getfileinfo: play lock failed\n");
      return;
    }
    I = &g_info;

    zz_assert( P == &g_play );
    zz_assert( I == &g_info );

    dmsg("getfileinfo: on current track\n");
  } else if (zz_calloc(&tmp, sizeof(*tmp))) {
    return;
  } else {
    zz_assert( tmp );
    P = &tmp->play;
    I = &tmp->info;
    if (load(P,I,uri,MEASURE_ONLY))
      goto error_exit;
  }

  zz_assert( P );
  zz_assert( I );

  dmsg("getfileinfo: <%s> fmt:%s mixer:<%s> spr:%lu rate:%lu ms:%lu\n",
       uri ? uri : "(current)",
       I->fmt.str ? I->fmt.str : "(nil)",
       I->mix.name ? I->mix.name : "(nil)",
       LU(I->mix.spr), LU(I->len.rate), LU(I->len.ms));

  if (msptr) *msptr = I->len.ms;
  if (title && I->fmt.num != ZZ_FORMAT_UNKNOWN) {
    switch (((!!*I->tag.album) << 1) | !!*I->tag.title) {
    case 1:
      strncpy(title, I->tag.title, GETFILEINFO_TITLE_LENGTH-1);
      break;
    case 2:
      strncpy(title, I->tag.album, GETFILEINFO_TITLE_LENGTH-1);
      break;
    case 3:
      snprintf(title,GETFILEINFO_TITLE_LENGTH-1,
	       "%s - %s",I->tag.album,I->tag.title);
      break;
    }
    title[GETFILEINFO_TITLE_LENGTH-1] = 0;
  }

error_exit:
  if (tmp) {
    dmsg("getfileinfo: releasing temporary play/info\n");
    zz_assert( P == &tmp->play );
    zz_assert( I == &tmp->info );
    zz_close(&tmp->play);
    zz_memdel(&tmp);
    zz_assert( !tmp );
  } else if (P) {
    zz_assert( P == &g_play );
    play_unlock(P);
  }

  dmsg("getfileinfo: <%s> \"%s\" %lums\n",
       uri?uri:"(current)", title?title:"<n/a>", LU(msptr?*msptr:0));
}

static
/*****************************************************************************
 * LOOP
 ****************************************************************************/
DWORD WINAPI playloop(LPVOID cookie)
{
  zz_u8_t filling = 1;
  zz_u16_t npcm = 0, n;
  int32_t * _pcm;

  atomic_set(&g_playing,1);
  for (;;) {

    if (atomic_get(&g_stopreq))
      break;

    if (filling) {
      /* filling */
      n = 576 - npcm;
      zz_assert( n > 0 );
      if (n <= 0) break;
      n = zz_play(&g_play, g_pcm+npcm, n);
      if (n <= 0) break;

      npcm += n;
      zz_assert( npcm <= 576 );

      if (npcm >= 576) {
	int vispos = g_mod.outMod->GetWrittenTime();
	g_mod.SAAddPCMData (g_pcm, 2, 16, vispos);
	g_mod.VSAAddPCMData(g_pcm, 2, 16, vispos);
	if (g_mod.dsp_isactive())
	  npcm = g_mod.dsp_dosamples((void*)g_pcm, npcm, 16,2, g_play.core.spr);
	filling = 0;
	_pcm = g_pcm;
      }
    } else {
      /* flushing */
      n = g_mod.outMod->CanWrite() >> 2;
      if (!n) {
	Sleep(10);
	continue;
      }
      if (n > npcm)
	n = npcm;

      /* Write the pcm data to the output system */
      g_mod.outMod->Write((char*)_pcm, n << 2);
      _pcm += n;
      npcm -= n;
      filling = !npcm;
    }

  }

  /* Wait buffered output to be processed */
  while (!atomic_get(&g_stopreq)) {
    g_mod.outMod->CanWrite();		/* needed by some out mod */
    if (!g_mod.outMod->IsPlaying()) {
      /* Done playing: tell Winamp and quit the thread */
      PostMessage(g_mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
      break;
    } else {
      Sleep(15);
    }
  }

  atomic_set(&g_playing,0);
  return 0;
}

#ifndef NDEBUG
/* My nice message handler that prints on STDERROR if the program was
 * launch from a console else use windows debug facilities. */
static void msg_handler(zz_u8_t chan, void * user,
			const char * fmt , va_list list)
{
  static const char chans[][4] = { "ERR","WRN","INF","DBG","???" };
  static const char pref[] = "";
  char	* s;
  DWORD i, len;
  HANDLE hdl;
  va_list list_bis;

  if (chan >= 4 || ! fmt) return;	/* safety first */

  /* Copy the va_list and runs snprintf to measure the string length */
  va_copy(list_bis, list);
  len = vsnprintf(NULL, 0, fmt,list_bis);
  va_end(list_bis);
  if (len <= 0)
    return;
  len += 4 + sizeof(pref) + sizeof(chans[0]);
  s = _malloca(len);
  if (!s) return;
  i  = 0;
  i += snprintf(s+i,len-i,"%s%s:",pref,chans[chan]);
  i += vsnprintf(s+i,len-i,fmt,list);
  s[len-1] = 0;
  if (hdl = GetStdHandle(STD_ERROR_HANDLE),
      (hdl != NULL && hdl != INVALID_HANDLE_VALUE)) {
    WriteFile(hdl,s,i,&i,NULL);
    FlushFileBuffers(hdl);
  }
  else
    OutputDebugStringA(s);
  _freea(s);
}
#endif /* NDEBUG */

static
/*****************************************************************************
 * PLUGIN INIT
 ****************************************************************************/
void init()
{
  config_t cfg;
#ifdef NDEBUG
  zz_log_fun(0,0);
#else
  zz_log_fun(msg_handler,0);
#endif
  zz_vfs_add(zz_file_vfs());
  /* clear and init private */
  g_lock = CreateMutex(NULL, FALSE, NULL);
  x_info.lock = CreateMutex(NULL, FALSE, NULL);

  /* Load config from registry */
  ConfigLoad(&cfg);
  g_mid = cfg.mid;
  g_spr = cfg.spr;
  g_dms = cfg.dms;
  g_map = LOWORD(cfg.map);
  g_lr8 = HIWORD(cfg.map);;

  dmsg("init mixer:%hu spr:%lu dms:%lu map:%hu lr8:%hu\n",
       HU(g_mid), LU(g_spr), LU(g_dms), HU(g_map), HU(g_lr8));
}

static
/*****************************************************************************
 * PLUGIN SHUTDOWN
 ****************************************************************************/
void quit()
{
  CloseHandle(g_lock);
  g_lock = 0;
  CloseHandle(x_info.lock);
  x_info.lock = 0;
  dmsg("quit\n");
  if (zz_memchk_calls())
    emsg("memory check has failed\n");
}

/*****************************************************************************
 * TRANSCODER
 ****************************************************************************/

struct transcon {
  play_t    play;		   /* zingzong play instance.       */
  zz_info_t info;		   /* zingzong info instance.       */
  size_t    pcm;		   /* pcm counter.                  */
  int	    done;		   /* 0:running / 1:done / -1:error */
  zz_err_t  code;		   /* error code                    */
};

EXPORT
/**
 * Open zingzong transcoder.
 *
 * @param   uri  URI to transcode.
 * @param   siz  pointer to expected raw output size (progress bar)
 * @param   bps  pointer to output bit-per-sample
 * @param   nch  pointer to output number of channel
 * @param   spr  pointer to output sampling rate
 * @return  Transcoding context struct (pointer to struct transcon)
 * @retval  0 on errror
 */
intptr_t winampGetExtendedRead_open(
  const char *uri,int *siz, int *bps, int *nch, int *spr)
{
  struct transcon * trc = 0;

  dmsg("transcoder: -- '%s'\n", uri);
  if (ZZ_OK == zz_calloc(&trc,sizeof(struct transcon))) {
    zz_assert(trc);
    trc->code = load(&trc->play, &trc->info, uri, g_tr_measure);
    if (trc->code != ZZ_OK) {
      zz_free(&trc);
      zz_assert(!trc);
    } else {
      uint_least64_t bytes;
      *nch = 2;
      *spr = trc->info.mix.spr;
      *bps = 16;
      bytes = (uint_least64_t) trc->info.len.ms*trc->info.mix.spr/1000u;
      bytes <<= 2;
      *siz = (bytes > UINT_MAX ? UINT_MAX : bytes) & ~3;
    }
  }
  return (intptr_t) trc;
}

EXPORT
/**
 * Run transcoder.
 *
 * @param   hdl  Pointer to transcoder context
 * @patam   dst  PCM buffer to fill
 * @param   len  dst size in byte
 * @param   end  pointer to a variable set by winamp to query the transcoder
 *               to kindly exit (abort)
 * @return  The number of byte actually filled in dst
 * @retval  0 to notify the end
 */
intptr_t winampGetExtendedRead_getData(
  intptr_t hdl, char *dst, int len, int *_end)
{
  struct transcon * trc = (struct transcon *) hdl;
  volatile int * end, fake_end = 0;
  int cnt;

  /* safety net. winamp should give us a valid pointers. */
  end = _end ? _end : &fake_end;
  if (!trc || !dst)
    return 0;

  for (cnt = 0; len >= 4; ) {
    int n;

    if (*end) {
      dmsg("transcoder: winamp request to end\n");
      trc->done = 0x04;			/* flag as done */
      trc->code = E_ERR;		/* GB: is it an error ? */
      cnt = 0;				/* discard all */
      break;
    }

    if (trc->done) {
      dmsg("transcoder: marked as done (%02hX)\n", HU(trc->done));
      cnt = 0;
      break;
    }

    n = len >> 2;
    if ( n >= 0x8000 )
      n = 0x7fff;
    n = zz_play(&trc->play, dst+cnt, -n);
    if (n <= 0) {
      trc->code = n;
      trc->done = 1;
      if (!n)
	dmsg("transcoder: play done\n");
      else
	dmsg("transcoder: play failed (%02hX)\n", HU(n));
      break;
    }

    n <<= 2;
    cnt += n;
    len -= n;
  }
  return cnt;
}

EXPORT
/**
 * Close sc68 transcoder.
 *
 * @param   hdl  Pointer to transcoder context
 */
void winampGetExtendedRead_close(intptr_t hdl)
{
  struct transcon * trc = (struct transcon *) hdl;
  if (trc) {
    dmsg("transcoder: close (ecode:%hX done:%hX)\n",
	 HU(trc->code), HU(trc->done));
    zz_close(&trc->play);
    zz_free(&trc);
  }
}

static int is_my_tag(const char * name, const char ** value)
{
  /* Winamp known metatags so far:

     Artist GracenoteExtData GracenoteFileID album albumartist artist
     bpm comment composer disc formatinformation genre length
     publisher replaygain_album_gain replaygain_track_gain streamname
     streamtype title track type year
  */

  /* Tags zingzong supports. If value is set it's a constant and
   * actual file loading is not necessary.
   */
  static struct supported_tag {
    const char * name;
    const char * value;
  } tags[] = {
    /* GB: /!\ Keep it sorted /!\ */
    { "album", 0 },
    { "artist", 0 },
    { "family", "quartet audio file" },
    { "formatinformation",
      "Microdeal Quartet\n"
      "\n"
      "Quartet is a music score editor edited by Microdeal in 1989 for"
      " the Atari ST and the Amiga home computers featuring a 4 digital"
      " channel sample sequencer and DSP software.\n"
      "\n"
      "Illusions Programmers:\n"
      "Rob Povey & Kevin Cowtan" },
    { "genre",	 "Quartet Atari ST Music" },
    { "length",	 0 },
    { "lossless", "1" },
    { "publisher", 0 },
    { "title", 0 },
    { "type", "0" },		      /* "0" = audio format */
    { 0, 0 }
  }, *tag;

  for (tag=tags; tag->name; ++tag) {
    int res = strcasecmp(name, tag->name);
    if (!res) {
      *value = tag->value;
      return 1;
    }
    if (res < 0)
      break;
  }
  return 0;
}

EXPORT
/**
 * Provides the extended meta tag support of winamp.
 *
 * @param  uri   URI or file to get info from (0 or empty for playing)
 * @param  data  Tag name
 * @param  dest  Buffer for tag value
 * @param  max   Size of dest buffer
 *
 * @retval 1 tag handled
 * @retval 0 unsupported tag
 */
int winampGetExtendedFileInfo(const char *uri, const char *data,
			      char *dest, size_t max)
{
  int ret = 0;
  play_t * P = 0;  zz_info_t *I = 0, tmp_info;
  struct xinfo_s * X = 0;
  const char * value;

  /* Safety net */
  if (!data || !*data || !dest || max < 2)
    return 0;

  if (!is_my_tag(data,&value))
    return 0;
  else if (value)
    goto got_value;

  if (!*uri) uri = 0;
  if (!uri) {
    P = play_lock();
    I = &g_info;
  } else {
    int l = 1; const int M = sizeof(X->data)-1;

    if (X = info_lock(), X) {
      zz_err_t ecode = ZZ_OK;

      zz_assert( X == &x_info );
      /* always the empty string at start */
      X->data[0] = 0;
      if (!X->uri || !*X->uri || strcasecmp(uri,X->uri)) {
	dmsg("WA-Xinfo[%s]: new URI \"%s\"\n", data, uri);
	X->ready = 0;			/* not ready yet */
	/* Copy URI */
	X->uri = X->data+(l=1);
	l += 1 + snprintf(X->uri, M-l, "%s", uri);
	/* Reset all other fields */
	X->ms = 0;
	X->format = X->album = X->artist = X->title = X->ripper = X->data;
	ecode = zz_new(&P);
	while (!ecode) {
	  if (ecode = zz_load(P,uri,"",0), ecode) break;
	  if (ecode = zz_info(P,&tmp_info), ecode) break;
	  X->format = (char *)tmp_info.fmt.str; /* always a static string */
	  dmsg("WA-Xinfo[%s]: format is <%s>\n", data, X->format);

#define COPYTAG(TAG)							\
	  if ((l+=snprintf(X->TAG=X->data+l,M-l,"%s",I->tag.TAG))	\
	      >= M) { break; } else ++l

	  I = &tmp_info;
	  COPYTAG(title);
	  COPYTAG(artist);
	  COPYTAG(ripper);
	  COPYTAG(album);
	  I = 0;
	  break;
	}
	X->ready = 1;
	X->fail	 = !!ecode;
	dmsg("WA-Xinfo[%s]: %s\n", data, X->fail?"FAIL":"READY");

	if (P)
	  zz_del(&P);
	zz_assert( !P );
	P = 0;
      } /* uri */

      if (X->ready && !X->fail) {
	I = &tmp_info;
	/* copy xinfo to this temp info to use it below */
	I->tag.album  = X->album;
	I->tag.title  = X->title;
	I->tag.artist = X->artist;
	I->tag.ripper = X->ripper;
      }
      /* GB: We can not unlock X until the data is copied */
      X->data[M] = 0;			/* safety net */
    }
  }

  value = 0;
  if (I) {
    if (!strcasecmp(data,"length")) {
      snprintf(dest, max, "%lu", LU(I->len.ms));
      value = dest;
    }
    else if (!strcasecmp(data,"album"))
      value = I->tag.album;
    else if (!strcasecmp(data,"artist" ))
      value = I->tag.artist;
    else if (!strcasecmp(data,"title"))
      value = I->tag.title;
    else if (!strcasecmp(data,"publisher"))
      value = I->tag.ripper;
  }

got_value:
  ret = value != 0 && *value;
  *dest = 0;
  if (ret) {
    if (value != dest) strncpy(dest, value, max);
    dest[max-1] = 0;
  }

  /* GB: now we can unlock safely */
  if (X)
    info_unlock(X);
  if (P)
    play_unlock(P);

  /* dmsg("%c [%s]=<%s>\n", ret?'*':'!',data,dest); */

  return ret;
}

EXPORT
/**
 * Provides fake support for writing tag to prevent winamp unified
 * file info to complain.
 */
int winampSetExtendedFileInfo(const char *fn, const char *data, char *val)
{
  return 1;
}

EXPORT
/**
 * Provides writing tags to prevent winamp unified file info to
 * complain.
 */
int winampWriteExtendedFileInfo()
{
  return 1;
}

/**
 * Called on file info request.
 *
 * @retval  0  Use plugin file info dialog (if it exists)
 * @retval  1  Use winamp unified file info dialog
 */
EXPORT
int winampUseUnifiedFileInfoDlg(const char * fn)
{
  return 1;
}

EXPORT
/**
 * Called before uninstalling the plugin DLL.
 *
 * @retval IN_PLUGIN_UNINSTALL_NOW     Plugin can be uninstalled
 * @retval IN_PLUGIN_UNINSTALL_REBOOT  Winamp needs to restart to uninstall
 */
int winampUninstallPlugin(HINSTANCE hdll, HWND parent, int param)
{
  return IN_PLUGIN_UNINSTALL_NOW;
}

EXPORT
/**
 * Provides the input module object;
 */
In_Module *winampGetInModule2()
{
  return &g_mod;
}
