/**
 * @file    dialogs.c
 * @date    2017-07-29
 * @brief   zingzong winamp plugin config dialogs
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
# include "config.h"
#endif

#define ZZ_DBG_PREFIX "(amp) dlg: "


/* windows */
#include "in_zingzong.h"

/* resource */
#include "../../rsc/resource.h"

/* zingzong */
#include "../zingzong.h"
#include "../zz_def.h"

/* clib */
#include <stdio.h>

/***********************************************************************
 * ABOUT
 */

static INT_PTR CALLBACK AboutProc(HWND,UINT,WPARAM,LPARAM);


#if 0
static BOOL CALLBACK PrintResourceCb(
  HMODULE hmod, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
  dmsg("HMOD: %p\n", (void*) hmod);
  if (IS_INTRESOURCE(lpszType)) {
    dmsg("TYPE: (int) %d\n", (int) lpszType);
  } else {
    dmsg("TYPE: (str) \"%s\"\n", (char *) lpszType);
  }
  if (IS_INTRESOURCE(lpszName)) {
    dmsg("NAME: (int) %d\n", (int) lpszName);
  } else {
    dmsg("NAME: (str) \"%s\"\n", (char *) lpszName);
  }
  return TRUE;
}

static void PrintResource(HMODULE hmod)
{
  BOOL res =
    EnumResourceNames(hmod,RT_ICON, PrintResourceCb,0);
  dmsg("EnumResourceNames => %d\n",(int)res);
}
#endif

static char about_text[] =
  "ZingZong for Winamp\n"
  "\n"
  "a Microdeal Quartet music player\n"
  "\n"
#ifdef DEBUG
  "!!! DEBUG !!! "
#endif
  "Version " PACKAGE_VERSION "\n"
  "\n"
  "\xa9" "2017 Benjamin Gerard AKA Ben/OVR\n";

void AboutDialog(HINSTANCE hinst, HWND parent)
{
  INT_PTR res =
    DialogBox(
      hinst,                            /* hInstance */
      MAKEINTRESOURCE(IDD_ABOUT),       /* lpTemplate */
      parent,                           /* hWndParent */
      AboutProc);

  dmsg("AboutDialog => %d\n", (int)res);
  if (res == -1 || (!res && GetLastError())) {
    MessageBoxA(
      parent, about_text,
      "About zingzong for winamp",
      MB_OK);
  }
}

static struct {
  unsigned size:8, bold:1, italic:1;
} fontspec[3] = {
  { 20,1,0 },
  { 14,0,0 },
  { 13,0,1 },
};

typedef struct {
  HICON wa_icon;
  HFONT font[ARRAYSIZE(fontspec)];

  short nblines;
  struct {
    uint8_t f,r,g,b,l;
    SIZE sz;
    const char * t;
  } line[5];
  char text[sizeof(about_text)];
} about_t;

static HFONT MyCreateFont(int size, BOOL bold, BOOL italic)
{
  return
    CreateFontA(
      size,0,                           /* h,w */
      0,0,                              /* esc,orientation */
      bold ? FW_BOLD : FW_MEDIUM,       /* weight */
      italic,                           /* italic */
      FALSE,                            /* underline */
      FALSE,                            /* strike-out */
      ANSI_CHARSET,                     /* charset */
      OUT_OUTLINE_PRECIS,               /* precision */
      CLIP_DEFAULT_PRECIS,              /* clipping */
      CLEARTYPE_QUALITY,                /* quality */
      0,                                /* pitch */
      0             /* "arial" */       /* Font */
      );
}

static void CreateFonts(about_t * about)
{
  int i;
  const int max_font = ARRAYSIZE(about->font);
  /* Create fonts */
  for (i=0; i<max_font; ++i) {
    about->font[i] =
      MyCreateFont(fontspec[i].size, fontspec[i].bold, fontspec[i].italic);
    dmsg("font #%d = %p\n", i, about->font[i]);
  }
}

static void CreateIcons(about_t * about)
{
#if 0
  HRSRC hResource;
  HRSRC hMem;
  BYTE *lpResource;
  int   nID;
  const int w=256, h=w;

  hResource = FindResourceA(
    0,
    MAKEINTRESOURCE(102),
    MAKEINTRESOURCE(RT_GROUP_ICON));
  zz_assert(hResource);

  hMem = LoadResource(0, hResource);
  zz_assert(hMem);

  lpResource = LockResource(hMem);
  zz_assert(lpResource);
  nID = LookupIconIdFromDirectoryEx(
    (PBYTE) lpResource, TRUE,
    w, h, LR_DEFAULTCOLOR);
  dmsg("about: winamp icon found #%d\n", nID);

  hResource = FindResource(
    0,
    MAKEINTRESOURCE(nID),
    MAKEINTRESOURCE(RT_ICON));
  zz_assert(hResource);

  hMem = LoadResource(0, hResource);
  zz_assert(hMem);

  lpResource = LockResource(hMem);
  zz_assert(lpResource);

  about->wa_icon
    = CreateIconFromResourceEx((PBYTE) lpResource,
                               SizeofResource(0, hResource),
                               TRUE,
                               0x00030000,
                               w,h,LR_DEFAULTCOLOR|LR_SHARED);
  zz_assert(about->wa_icon);
#endif
}

static INT_PTR CALLBACK
AboutProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
  about_t * about = (about_t*) GetWindowLongPtrA(hdlg,GWLP_USERDATA);

  switch (msg) {

  case WM_INITDIALOG: {
    zz_calloc(&about, sizeof(about_t));
    SetWindowLongPtrA(hdlg, GWLP_USERDATA, (LONG_PTR) about);
    if (about) {
      CreateFonts(about);
      CreateIcons(about);
    }
    SetDlgItemTextA(hdlg, IDC_ABOUTTEXT, about_text);
  } return TRUE;

  case WM_CLOSE:
    EndDialog(hdlg, 1);
    return TRUE;

  case WM_COMMAND:
    switch (LOWORD(wp)) {
    case IDOK:
      EndDialog(hdlg, 1);
      return TRUE;
    case IDC_URI:
      /* HINSTANCE */ ShellExecute(
        0,                              /* parent */
        "open",                         /* function */
        "https://github.com/benjihan/zingzong/",
        0,                              /* parameters */
        0,                              /* directory */
        SW_SHOWDEFAULT);
      return TRUE;
    }
    break;

  case WM_DRAWITEM: {
    LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lp;
    if (wp == IDC_ABOUTTEXT) {

      if (!about)
        break;

      DrawIcon(pDIS->hDC, pDIS->rcItem.left, pDIS->rcItem.top, about->wa_icon);

      if (1) {
        int i, y, x, y2 = 0;
        SIZE sz;

        if (!about->nblines) {
          char *s;

          strcpy(about->text, about_text);

          for (i=0, s=strtok(about->text,"\n");
               i<ARRAYSIZE(about->line);
               s=strtok(0,"\n")) {
            uint8_t f, r, g, b;
            if (!s) break;
            if (!*s) continue;

            if ( i == 0) {
              f = 0; r = g = b = 0;
            } else if (i == 3) {
              f = 2; r = g = b = 0;
            } else {
              f = 1; r = g = b = 0;
            }

            about->line[i].t = s;
            about->line[i].l = strlen(s);
            about->line[i].f = f;
            about->line[i].r = r;
            about->line[i].g = g;
            about->line[i].b = b;

            zz_assert( about->line[i].t );
            zz_assert( about->line[i].l );
            zz_assert( about->line[i].f < ARRAYSIZE(about->font) );

            ++i;
          }
          about->nblines = !i ? -1 : i;
          dmsg("lines: %d\n", about->nblines);
        }

        for (y=y2=i=0; i<about->nblines; ++i) {
          HGDIOBJ old =
            SelectObject(pDIS->hDC,
                         about->font[ about->line[i].f ]);

          SetTextColor(pDIS->hDC,
                       RGB(about->line[i].r,about->line[i].g,about->line[i].b));

          GetTextExtentPoint32(pDIS->hDC,about->line[i].t,about->line[i].l,&sz);

          y2 = sz.cy >> 1;

          x = ( pDIS->rcItem.right - pDIS->rcItem.left - sz.cx ) >> 1;

          dmsg("[%d] %d %d \"%s\"\n",
               i, x, y, about->line[i].t);

          TextOutA(pDIS->hDC,
                   pDIS->rcItem.left+x, pDIS->rcItem.top+y,
                   about->line[i].t, about->line[i].l);

          y += sz.cy+y2;
          SelectObject(pDIS->hDC, old);
        }

      }
    }
    return TRUE;
  } break; /* WM_DRAWITEM */


  case WM_DESTROY:
    if (about) {
      int i;

      if (about->wa_icon)
        DestroyIcon(about->wa_icon);
      about->wa_icon = 0;

      for (i=0; i<ARRAYSIZE(about->font); ++i)
        if (about->font[i])
          DeleteObject(about->font[i]);

      zz_free(&about);
    }
    break;
  }

  return FALSE;
}


/***********************************************************************
 * CONFIG
 */

static INT_PTR CALLBACK ConfigProc(HWND,UINT,WPARAM,LPARAM);

int ConfigDialog(HINSTANCE hinst, HWND parent, config_t * cfg)
{
  config_t tmp = *cfg;

  INT_PTR res =
    DialogBoxParamA(
      hinst,                            /* hInstance */
      MAKEINTRESOURCE(IDD_CONFIG),      /* lpTemplate */
      parent,                           /* hWndParent */
      ConfigProc,                       /* lpDialogFunc */
      (LPARAM) &tmp);                   /* lpDialogFunc */
  if (res == 0x1337)
    *cfg = tmp;

  dmsg("ConfigDialog => %lx (mid:%hu spr:%lu dms:%lu)\n",
       LU(res), HU(cfg->mid), LU(cfg->spr), LU(cfg->dms));
  return res;
}

static BOOL SetMixers(HWND hdlg, int cur)
{
  const char *name="", *desc="";
  zz_u8_t i, icur = 0;

  SendDlgItemMessage(hdlg, IDC_MIXER, CB_RESETCONTENT, 0, 0);
  SendDlgItemMessage(hdlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM)"(default)");
  for (i=0; zz_mixer_info(i, &name, &desc) != ZZ_MIXER_ERR; ++i) {
    if (i == cur) icur = i+1;
    SendDlgItemMessage(hdlg, IDC_MIXER, CB_ADDSTRING, 0, (LPARAM) desc);
  }
  SendDlgItemMessage(hdlg, IDC_MIXER, CB_SETCURSEL, icur, 0);
  return TRUE;
}

static int GetMid(HWND hdlg)
{
  int id = SendDlgItemMessage(hdlg, IDC_MIXER, CB_GETCURSEL, 0, 0);
  return id <= 0
    ? ZZ_MIXER_DEF
    : id-1
    ;
}

static int clip_spr(int spr)
{
  return spr < SPR_MIN ? SPR_MIN : spr > SPR_MAX ? SPR_MAX : spr;
}

static struct { int val;  const char * txt; } sprinfo[] =
{
  /* 0 */ {    -1, "User defined (hz)"          },
  /* 1 */ {     0, "Song defined (4 to 20 kHz)" },
  /* 2 */ { 44100, "CD quality (44.1 KHz)"      },
  /* 3 */ { 48000, "DVD quality (48 kHz)"       },
  /* 4 */ { 96000, "High quality (96 kHz)"      }
};

#define MAX_SPRINFO (sizeof(sprinfo)/sizeof(sprinfo[0]))

static BOOL SetSampling(HWND hdlg, int spr)
{
  int j, i;
  if (spr == 0)
    i = 1;
  else {
    spr = clip_spr(spr>0 ? spr : SPR_DEF);
    if (spr == 44100)
      i = 2;
    else if (spr == 48000)
      i = 3;
    else if (spr == 96000)
      i = 4;
    else
      i = 0;
  }

  SendDlgItemMessage(hdlg, IDC_SPR, CB_RESETCONTENT, 0, 0);
  for (j=0; j<MAX_SPRINFO; ++j)
    SendDlgItemMessage(hdlg, IDC_SPR,
                       CB_ADDSTRING, 0, (LPARAM) sprinfo[j].txt);
  SendDlgItemMessage(hdlg, IDC_SPR, CB_SETCURSEL, i, 0);
  SetDlgItemInt(hdlg, IDC_SPRVAL, spr ? spr : SPR_DEF, FALSE);
  EnableWindow(GetDlgItem(hdlg, IDC_SPRVAL), !i);

  return TRUE;
}

static BOOL SetTime(HWND hdlg, int dms)
{
  /* $$$ TODO */
  return TRUE;
}

static int GetDms(HWND hdlg)
{
  /* $$$ TODO */
  return 0;
}

static int GetUserSpr(HWND hdlg, int * ret)
{
  BOOL done = FALSE;
  UINT i =
    GetDlgItemInt(
      hdlg,
      IDC_SPRVAL, &done, FALSE);
  if (ret) *ret = i;
  return !done ? 0 : clip_spr(i);
}

static int GetSpr(HWND hdlg)
{
  int spr = SendDlgItemMessage(hdlg, IDC_SPR, CB_GETCURSEL, 0, 0);
  if (spr == 0)
    spr = GetUserSpr(hdlg, 0);
  else if (spr >= 1 && spr < MAX_SPRINFO)
    spr = sprinfo[spr].val;
  else
    spr = 0;
  return spr;
}

static INT_PTR CALLBACK
ConfigProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg) {

  case WM_INITDIALOG: {
    config_t * const cfg = (config_t *) lp;
    SetMixers(hdlg, cfg->mid);
    SetSampling(hdlg, cfg->spr);
    SetTime(hdlg, cfg->dms);
    SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) cfg);
  } return TRUE;

  case WM_COMMAND: {
    const int wlo = LOWORD(wp), whi = HIWORD(wp);

    if (wlo == IDCANCEL) {
      EndDialog(hdlg, 0xD207);
      return TRUE;
    } else if (wlo == IDOK) {
      INT_PTR ret = 0xDEAD;
      config_t * cfg = (config_t*)GetWindowLongPtr(hdlg,GWLP_USERDATA);
      if (cfg) {
        cfg->mid = GetMid(hdlg);
        cfg->spr = GetSpr(hdlg);
        cfg->dms = GetDms(hdlg);
        ret = 0x1337;
      }
      EndDialog(hdlg, ret);
      return TRUE;
    } else if (wlo == IDC_SPR) {
      if (whi == CBN_SELCHANGE) {
        int i = SendMessageA((HWND)lp,CB_GETCURSEL,0,0);
        EnableWindow(GetDlgItem(hdlg, IDC_SPRVAL), !i);
        dmsg ( "SELCHANGE TO %d (%d)\n", i, wlo);
      }
    }/*  else if (wlo == IDC_SPRVAL) { */
    /*   if (whi == EN_CHANGE) { */
    /*     int ret, spr = GetUserSpr(hdlg, &ret); */
    /*     if (spr > 0 && spr != ret) */
    /*       SetDlgItemInt(hdlg,IDC_SPRVAL,spr,FALSE); */
    /*   } */
    /* } */

  } break; /* WM_COMMAND */
  }

  return FALSE;
}

#undef ZZ_DBG_PREFIX
#define ZZ_DBG_PREFIX "(amp) cfg: "

static
int ConfigCheck(config_t * cfg, const char ** name)
{
  zz_u8_t modified = 0;
  const char * desc;

  if (cfg->spr) {
    if (cfg->spr < SPR_MIN) {
      cfg->spr = SPR_MIN;
      modified |= 1;
    }
    else if (cfg->spr > SPR_MAX) {
      cfg->spr = SPR_MAX;
      modified |= 2;
    }
  }

  if (ZZ_MIXER_DEF != cfg->mid &&
      ZZ_MIXER_ERR == zz_mixer_info(cfg->mid, name, &desc)) {
    cfg->mid = ZZ_MIXER_DEF;
    modified |= 4;
  }

  if (cfg->mid == ZZ_MIXER_DEF)
    *name = "default";

  return modified;
}

static const char keyname[] = "Software\\Winamp\\ZingZong";

int ConfigLoad(config_t * cfg)
{
  HKEY hk = 0;
  int res = 0;

  struct {
    DWORD len;
    union {
      DWORD num;
      char  str[32];
    };
  } val;

  /* Setup with valid values */
  cfg->mid = ZZ_MIXER_DEF;
  cfg->spr = SPR_DEF;
  cfg->dms = 0;

  if (ERROR_SUCCESS !=
      RegOpenKeyEx(HKEY_CURRENT_USER,keyname,0,KEY_READ,&hk))
    res = -1;
  else {
    const char *name,*desc;
    zz_u8_t i;

    val.len = sizeof(val.str);
    val.str[0] = 0;
    RegGetValue(hk,0,"mid",RRF_RT_REG_SZ,0,val.str,&val.len);
    val.str[sizeof(val.str)-1] = 0;
    dmsg("config-load: mid=\"%s\"\n", val.str);
    for (i=0; ZZ_MIXER_ERR != zz_mixer_info(i,&name,&desc); ++i)
      if (!strcasecmp(name, val.str)) {
        dmsg("config-load: mid=%hu\n", HU(i));
        cfg->mid = i;
        break;
      }

    val.len = sizeof(val.num);
    val.num = cfg->spr;
    RegGetValue(hk,0,"spr",RRF_RT_REG_DWORD,0,&val.num,&val.len);
    cfg->spr = val.num;
    dmsg("config-load: spr=%lu\n", LU(cfg->spr));

    val.len = sizeof(val.num);
    val.num = cfg->dms;
    RegGetValue(hk,0,"dms",RRF_RT_REG_DWORD,0,&val.num,&val.len);
    cfg->dms = val.num;
    dmsg("config-load: dms=%lu\n", LU(cfg->dms));

    RegCloseKey(hk);
  }

  return res;
}

int ConfigSave(config_t * cfg)
{
  HKEY hk=0;
  const char * name = "default";
  int modified = ConfigCheck(cfg,&name);
  DWORD dw;
  LONG res = RegCreateKeyExA(
    HKEY_CURRENT_USER,                  /* key */
    keyname,                            /* subkey */
    0,                                  /* reserved */
    0,                                  /* class */
    0,                                  /* options */
    KEY_WRITE,                          /* access */
    0,                                  /* secu */
    &hk,                                /* key handle */
    0);                                 /* disposition */

  dmsg("config-save: mid:%hu spr:%lu dms:%lu -- %ld\n",
       HU(cfg->mid), LU(cfg->spr), LU(cfg->dms), res);

  if (res == ERROR_SUCCESS) {
    const char * version = zz_version();
    RegSetValue(hk,0,REG_SZ,(char*)version, strlen(version)+1);
    RegSetKeyValueA(hk,0,"mid",REG_SZ,name,strlen(name)+1);
    dw = cfg->spr;
    RegSetKeyValueA(hk,0,"spr",REG_DWORD,(char*)&dw,sizeof(dw));
    dw = cfg->dms;
    RegSetKeyValueA(hk,0,"dms",REG_DWORD,(char*)&dw,sizeof(dw));
    RegCloseKey(hk);
  } else {
    modified |= 1 << (sizeof(int)*8-1);
  }

  return modified;
}
