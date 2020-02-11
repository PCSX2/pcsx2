; Copyright (C) 2017 PCSX2 Team

; These definitions are shared between the 2 installers (pre-install/portable and full)
; This reduces duplicate code throughout both installers.

ManifestDPIAware true
ShowInstDetails nevershow
ShowUninstDetails nevershow

SetCompressor /SOLID lzma
SetCompressorDictSize 24

!ifndef APP_VERSION
  !define APP_VERSION      "1.6.0"
!endif

!define APP_NAME         "PCSX2 ${APP_VERSION}"

; The name of the installer
Name "${APP_NAME}"

!define APP_FILENAME     "pcsx2"
!define APP_EXE          "$INSTDIR\${APP_FILENAME}.exe"

;===============================
; MUI STUFF
!include "MUI2.nsh"

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "banner.bmp"
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_ICON "AppIcon.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\nsis3-uninstall.ico"