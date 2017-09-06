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
 * Copyright (c) 2017 Benjamin Gerard AKA Ben/OVR.
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
#include "config.h"
#endif

#define ZZ_DBG_PREFIX "(amp) "

/* windows */
#include "in_zingzong.h"

/* zingzong */
#include "../zz_private.h"

/* libc */
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <malloc.h>                     /* malloca */

/* winamp 2 */
#include "winamp/in2.h"
#undef   _MSC_VER                       /* fix intptr_t redefinition */
#define  _MSC_VER 2000
#include "winamp/wa_ipc.h"
#include "winamp/ipc_pe.h"

/*****************************************************************************
 * Plugin private data.
 ****************************************************************************/

typedef unsigned int uint_t;
ZZ_EXTERN_C
zz_vfs_dri_t zz_file_vfs(void);

const char me[] = "in_zingzong";

#define DEFAULT_SEC 150u
#define USE_LOCK 1

#ifdef USE_LOCK
static HANDLE g_lock;                   /* mutex handle           */
#endif

struct xinfo_s {
  HANDLE lock;                          /* mutex for extended info */
  char * uri;                           /* load uri (in data[])    */
  char * title;                         /* title (in data[])       */
  char * artist;                        /* artist (in data[])      */
  int  used;                            /* currently used in data */
  char data[4096];                      /* Buffer for infos       */
} g_info;

static HANDLE g_thdl;                   /* thread handle          */
static DWORD  g_tid;                    /* thread id              */
static play_t g_play;                   /* play emulator instance */
static int    g_spr = SPR_DEF;          /* sampling rate in hz    */
static int    g_maxlatency;             /* max latency in ms      */
static volatile LONG g_playing;         /* true while playing     */
static volatile LONG g_stopreq;         /* stop requested         */
static volatile LONG g_paused;          /* pause status           */
static int16_t g_pcm[576*2];            /* buffer for DSP filters */
static zz_u8_t g_mixerid = ZZ_DEFAULT_MIXER;
/*****************************************************************************
 * Declaration
 ****************************************************************************/

/* The decode thread */
static DWORD WINAPI playloop(LPVOID b);
static void init();
static void quit();
static void config(HWND);
static void about(HWND);
static  int infobox(const char *, HWND);
static  int isourfile(const char *);
static void pause();
static void unpause();
static  int ispaused();
static  int getlength();
static  int getoutputtime();
static void setoutputtime(int);
static void setvolume(int);
static void setpan(int);
static  int play(const char *);
static void stop();
static void getfileinfo(const in_char *, in_char *, int *);
static void seteq(int, char *, int);

/*****************************************************************************
 * LOCKS
 ****************************************************************************/
#ifdef USE_LOCK

static inline int lock(HANDLE h)
{ return WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0; }

static inline int lock_noblock(HANDLE h)
{ return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }

static inline void unlock(HANDLE h)
{ ReleaseMutex(h); }

#else

static inline int lock(HANDLE h) { return 1; }
static inline int lock_noblock(HANDLE h) { return 1; }
static inline HANDLE h unlock(HANDLE h) { }

#endif

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

static inline
struct xinfo_s * info_lock()
{ return lock(g_info.lock) ? &g_info : 0; }

static inline
void info_unlock(struct xinfo_s * xinfo)
{ if (xinfo == &g_info) unlock(g_info.lock); }


static
/*****************************************************************************
 * THE INPUT MODULE
 ****************************************************************************/
In_Module g_mod =
{
  IN_VER,               /* Input plugin version as defined in in2.h */
  (char*)
  "ZinZong (Quartet music player) v" PACKAGE_VERSION, /* Description */
  0,                          /* hMainWindow (filled in by winamp)  */
  0,                          /* hDllInstance (filled in by winamp) */
  (char*)
  "play\0" "play file (*.play)\0"
  "4q\0" "Quartet bundle (*.4q)\0" "4v\0" "Quartet score (*.4v)\0",
  0,                                  /* is_seekable */
  1,                                  /* uses output plug-in system */

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

  0,0,0,0,0,0,0,0,0,     /* visualization calls filled in by winamp */
  0,0,                   /* dsp calls filled in by winamp */
  seteq,                 /* set equalizer */
  NULL,                  /* setinfo call filled in by winamp */
   0                      /* out_mod filled in by winamp */
};

static
/*****************************************************************************
 * CONFIG DIALOG
 ****************************************************************************/
void config(HWND hwnd)
{
  /* $$$ XXX TO DO */
}

static
/*****************************************************************************
 * ABOUT DIALOG
 ****************************************************************************/
void about(HWND hwnd)
{
  char temp[512];
  snprintf(temp,sizeof(temp),
           "ZingZong for Winamp\n"
           "\na Microdeal Quartet music player\n"
           "\n"
#ifdef DEBUG
           "!!! DEBUG !!! "
#endif
           "Version " PACKAGE_VERSION "\n"
           "\n(c) 2017 Benjamin Gerard AKA Ben/OVR");

  MessageBox(hwnd,
             temp,
             "About zingzong for winamp",
             MB_OK);
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
  const char * ext = strrchr(uri,'.');
  return
    (ext &&
     (0
      || ! strcasecmp(ext,".4q")
      || ! strcasecmp(ext,".4v")
       ));
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
  int ms = 0;                           /* 0 or -1 ? */
  if (atomic_get(&g_playing)) {
    play_t * const P = play_lock();
    if (P) {
      if (P->end_detect)
        ms = P->max_ticks * (1000u / P->rate);
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
/* Not supported ATM.
 * It has to signal the play thread it has to seek.
 */
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
  zz_close(&g_play);

  g_mod.outMod->Close();                /* Close output system */
  g_mod.SAVSADeInit();                  /* Shutdown visualization */
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

enum {
  NO_MEASURE=0, MEASURE, MEASURE_ONLY
};

static int load(play_t * const P, const char * uri, int measure)
{
  int ecode = E_INP;

  zz_memclr(P,sizeof(*P));
  ecode = zz_load(P, uri, measure == MEASURE_ONLY ? "" : 0, 0);

  if (!ecode) {
    ecode = zz_setup(P,g_mixerid,g_spr,200,DEFAULT_SEC*P->rate,1);
    if (!ecode)
      ecode = zz_init(P);
    dmsg("inited song:%ukhz vset:%ukhz\n",
         P->song.khz, P->vset.khz);
  }

  if (!ecode && measure != NO_MEASURE) {
    zz_u32_t max_ticks = 12u * 60u * 200u;
    dmsg("measuring for max %u ticks ...\n", max_ticks);
    ecode = zz_measure(P,&max_ticks,0);
  }

  dmsg("load -- '%s' => %d\n", uri, ecode);
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
  if (g_thdl)
    goto inused;

  /* cleanup */
  g_maxlatency = 0;
  g_stopreq    = 0;
  g_paused     = 0;

  err = -1;
  if (load(&g_play,uri,MEASURE))
    goto exit;

  /* Init output module */
  g_maxlatency = g_mod.outMod->Open(g_play.spr, 1, 16, 0, 0);
  if (g_maxlatency < 0)
    goto exit;

  /* set default volume */
  g_mod.outMod->SetVolume(-666);

  /* Init info and visualization stuff */
  g_mod.SetInfo(0, g_play.spr/1000, 1, 1);
  g_mod.SAVSAInit(g_maxlatency, g_play.spr);
  g_mod.VSASetInfo(g_play.spr, 1);

  /* Init play thread */
  g_thdl = (HANDLE)
    CreateThread(NULL,                  /* Default Security Attributs */
                 0,                     /* Default stack size         */
                 (LPTHREAD_START_ROUTINE)playloop, /* Thread function */
                 (LPVOID) 0,          /* Thread Cookie              */
                 0,                     /* Thread status              */
                 &g_tid                 /* Thread Id                  */
      );

  if (err = !g_thdl, !err)
    atomic_set(&g_playing,1);
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
  play_t * P=0, tmp;

  if (!*uri) uri = 0;
  if (!uri)
    P = play_lock();
  else if (load(P=&tmp, uri, MEASURE_ONLY)) {
    dmsg("load(%s) to measure only failed ?\n", uri);
    P = 0;
  }

  if (P) {
    if (msptr && P->end_detect && P->max_ticks) {
      *msptr = P->max_ticks * (1000u / P->rate);
      dmsg("'%s' ms=%u\n", uri?uri:"<current>", *msptr);
    }
    if (title && P->info.comment) {
      int i,j;
      dmsg("'%s' comment=\n%s\n",
           uri?uri:"<current>", P->info.comment);
      for (j=0; isspace(P->info.comment[j]); ++j)
        ;
      if (P->info.comment[j]) {
        for (i=0; i<GETFILEINFO_TITLE_LENGTH-1; ++i, ++j) {
          title[i] = P->info.comment[j];
          if (title[i] < 32) break;
        }
        title[i] = 0;
      }
    }
  } else {
    if (title) *title = 0;
    if (msptr) *msptr = 0;
  }
  if (P && !uri)
    play_unlock(P);
}

static
/*****************************************************************************
 * LOOP
 ****************************************************************************/
DWORD WINAPI playloop(LPVOID cookie)
{
  zz_u8_t filling = 1;
  zz_u16_t npcm = 0, n;
  int16_t * pcm;

  for (;;) {

    if (atomic_get(&g_stopreq))
      break;

    if (filling) {
      /* filling */
      n = 576 - npcm;
      if (n < 0) break;
      pcm = zz_play(&g_play, &n);
      if (!pcm || !n) break;

      memcpy(g_pcm+npcm, pcm, n*2);
      npcm += n;
      if (npcm >= 576) {
        int vispos = g_mod.outMod->GetWrittenTime();
        g_mod.SAAddPCMData (g_pcm, 1, 16, vispos);
        g_mod.VSAAddPCMData(g_pcm, 1, 16, vispos);
        if (g_mod.dsp_isactive()) {
          npcm = g_mod.dsp_dosamples(g_pcm, n = npcm, 16, 1, g_play.spr);
        }
        filling = 0;
        pcm = g_pcm;
      }
    } else {
      /* flushing */
      n = g_mod.outMod->CanWrite() >> 1;
      if (!n) {
        Sleep(10);
        continue;
      }
      if (n > npcm)
        n = npcm;

      /* Write the pcm data to the output system */
      g_mod.outMod->Write((char*)pcm, n*2);
      pcm += n;
      npcm -= n;
      filling = !npcm;
    }

  }
  atomic_set(&g_playing,0);

  dmsg("end of playloop\n");
  /* Wait buffered output to be processed */
  while (!atomic_get(&g_stopreq)) {
    g_mod.outMod->CanWrite();           /* needed by some out mod */
    if (!g_mod.outMod->IsPlaying()) {
      /* Done playing: tell Winamp and quit the thread */
      PostMessage(g_mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0);
      break;
    } else {
      Sleep(15);
    }
  }
  dmsg("end of waitloop\n");


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
  char  * s;
  DWORD i, len;
  HANDLE hdl;
  va_list list_bis;

  if (chan >= 4 || ! fmt) return;       /* safety first */

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
#ifdef NDEBUG
  zz_log_fun(0,0);
#else
  zz_log_fun(msg_handler,0);
#endif
  zz_vfs_add(zz_file_vfs());
  dmsg("init\n");
  /* clear and init private */
#ifdef USE_LOCK
  g_lock = CreateMutex(NULL, FALSE, NULL);
#endif
  g_info.lock = CreateMutex(NULL, FALSE, NULL);
}

static
/*****************************************************************************
 * PLUGIN SHUTDOWN
 ****************************************************************************/
void quit()
{
#ifdef USE_LOCK
  CloseHandle(g_lock); g_lock = 0;
#endif
  CloseHandle(g_info.lock); g_info.lock = 0;
  dmsg("quit\n");
}


/*****************************************************************************
 * TRANSCODER
 ****************************************************************************/

struct transcon {
  play_t play;                       /* zingzong play instance      */
  uint_t duration;                   /* run duration                */
  size_t pcm;                        /* pcm counter                 */
  int    done;                       /* 0:not done, 1:done -1:error */
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
 * @return  Transcoding context struct (sc68_t right now)
 * @retval  0 on errror
 */
intptr_t winampGetExtendedRead_open(
  const char *uri,int *siz, int *bps, int *nch, int *spr)
{
  struct transcon * trc = 0;

  dmsg("transcode -- '%s'\n", uri);
  zz_calloc(&trc,sizeof(struct transcon));
  if (!trc)
    goto error_no_free;
  if (load(&trc->play,uri,MEASURE))
    goto error;
  trc->duration = trc->play.end_detect ? trc->play.max_ticks : 0;

  *nch = 1;
  *spr = trc->play.spr;
  *bps = 16;
  *siz = trc->duration * trc->play.pcm_per_tick * 2;

  dmsg("transcode -- %u ticks, %u bytes\n", trc->duration, *siz);

  return (intptr_t)trc;

error:
  zz_free(&trc);
error_no_free:
  return 0;
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
  volatile int * end = _end;
  int cnt;

  zz_assert(end);

  for (cnt = 0; len >= 2; ) {
    int16_t * pcm;
    zz_u16_t n;

    if (*end || trc->done) {
      dmsg("transcoder -- notify end\n");
      cnt = 0; break;
    }
    if (trc->play.done) {
      dmsg("transcoder -- play done\n");
      trc->done = 1; break;
    }

    n = trc->play.pcm_per_tick;
    if (n > (len>>1) ) n = len>>1;
    pcm = zz_play(&trc->play, &n);
    if (!pcm) {
      dmsg("transcoder -- notify failure :(\n");
      trc->done = -1; break;
    }
    n <<= 1;                            /* in bytes */
    zz_memcpy(dst+cnt,pcm,n);
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
    zz_close(&trc->play);
    zz_free(&trc);
  }
}


#if 0
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
  if (!uri)
    dmsg("winampGetExtendedFileInfo '%s' (null)\n", data);
  else if (!*uri)
    dmsg("winampGetExtendedFileInfo '%s' (empty)\n", data);
  else
    dmsg("winampGetExtendedFileInfo '%s' (%s)\n", data, uri);

  /* play_t * P; */

  /* if (!*uri) uri = 0; */
  /* if (!uri) */
  /*   P = play_lock(); */



  /* else { */
  /*   info_lock(); */

  /*   info_unlock(); */
  /* } */



  return 0;
}

#endif

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
