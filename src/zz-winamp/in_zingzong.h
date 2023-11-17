/**
 * @file    in_zingzong.h
 * @date    2017-07-29
 * @brief   zingzong plugin for winamp 5.5
 * @author  https://github.com/benjihan
 *
 * ----------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2017-2023 Benjamin Gerard AKA Ben^OVR.
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

#ifndef IN_ZINGZONG_H
#define IN_ZINGZONG_H

/* Reduce windows header overhead */
#define WIN32_LEAN_AND_MEAN

/* Remove unused API */
#define NOCOMM
/* #define NOGDI */
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
/* #define NOMSG */
#define NOOPENFILE
#define NORASTEROPS
/* #define NOSCROLL */
#define NOSOUND
/* #define NOSYSMETRICS */
/* #define NOTEXTMETRIC */
#define NOWH
/* #define NOCOMM */
#define NOKANJI
#define NOCRYPT
#define NOMCX

#if 0
/* XP */
#define NTDDI_VERSION 0x05010000	/* NTDDI_WINXP XP */
#define _WIN32_WINNT  0x0501		/* _WIN32_WINNT_WINXP */
#define _WIN32_IE     0x0400		/* IE4 */
#else
/* VISTA */
#define NTDDI_VERSION 0x06000000	/* NTDDI_VISTA */
#define _WIN32_WINNT  0x0600		/* _WIN32_WINNT_VISTA */
#define _WIN32_IE     0x0500		/* _WIN32_IE_IE50 */
#endif

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#ifndef NOVTABLE
# define NOVTABLE
#endif

#ifndef EXPORT
# define EXPORT EXTERN_C __declspec(dllexport)
#endif

#define DLGHWND	 g_mod.hMainWindow
#define DLGHINST g_mod.hDllInstance

typedef struct	{
  int mid, spr, dms, map;
} config_t;

/* defined in dialogs.c */
EXTERN_C void AboutDialog(HINSTANCE hinst, HWND parent);
EXTERN_C int ConfigDialog(HINSTANCE hinst, HWND parent, config_t * cfg);
EXTERN_C int ConfigLoad(config_t * cfg);
EXTERN_C int ConfigSave(config_t * cfg);

#endif
