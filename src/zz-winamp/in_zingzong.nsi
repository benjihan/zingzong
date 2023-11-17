# -*- mode: conf; indent-tabs-mode: nil -*-
#
# @file   in_zingzong.nsi
# @author Benjamin Gerard AKA Ben/OVR
# @brief  Installer for zingzong winamp plugin
#

!Define HKLM_SASHIPA  "SOFTWARE\sashipa"
!Define HKLM_ZINGZONG "${HKLM_SASHIPA}\zingzong"
!Define HKLM_UNINSTALL "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\zingzong"

Name          "ZingZong"
Caption       "ZingZong for Winamp"
Icon          "${SRCDIR}\rsc\zingzong.ico"
UninstallIcon "${SRCDIR}\rsc\zingzong.ico"
OutFile       "${OUTFILE}"
CRCCheck      On
SetCompress   force
SetCompressor /SOLID lzma
InstProgressFlags smooth colored
SetOverwrite  On

LicenseBkColor /windows
LicenseText "ZingZong ${VERSION} license"
LicenseData "${SRCDIR}\LICENSE"
LicenseForceSelection checkbox

Page license
Page instfiles

Section "ZingZong for winamp" s_wmp
    ClearErrors
    ReadRegStr $0 HKCU "Software\Winamp" ""
    IfErrors 0 Copy
    MessageBox MB_OK "Can not locate winamp directory"
    Quit
Copy:
    StrCpy $0 "$0\Plugins\in_zingzong.dll"
    file /oname=$0 ${WMPDLL}
SectionEnd
