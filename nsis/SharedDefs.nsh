; Copyright (C) 2017 PCSX2 Team

; These definitions are shared between the 2 installers (pre-install/portable and full)
; This reduces duplicate code throughout both installers.

ManifestDPIAware true
Unicode true
ShowInstDetails nevershow
ShowUninstDetails nevershow

SetCompressor /SOLID lzma
SetCompressorDictSize 24

Var UserPrivileges
Var IsAdmin

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

Function IsUserAdmin
!include WinVer.nsh
# No user should ever have to experience this pain ;)
  ${IfNot} ${AtLeastWinVista}
    MessageBox MB_OK "Your operating system is unsupported by PCSX2. Please upgrade your operating system or install PCSX2 1.4.0."
    Quit
  ${EndIf}

UserInfo::GetOriginalAccountType
Pop $UserPrivileges

  # GetOriginalAccountType will check the tokens of the original user of the
  # current thread/process. If the user tokens were elevated or limited for
  # this process, GetOriginalAccountType will return the non-restricted
  # account type.
  # On Vista with UAC, for example, this is not the same value when running
  # with `RequestExecutionLevel user`. GetOriginalAccountType will return
  # "admin" while GetAccountType will return "user".
  ;UserInfo::GetOriginalAccountType
  ;Pop $R2

${If} $UserPrivileges == "Admin"
    StrCpy $IsAdmin 1
    ${ElseIf} $UserPrivileges == "User"
    StrCpy $IsAdmin 0
${EndIf}
FunctionEnd