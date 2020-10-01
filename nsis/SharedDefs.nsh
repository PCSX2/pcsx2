; Copyright (C) 2017 PCSX2 Team

; These definitions are shared between the 2 installers (pre-install/portable and full)
; This reduces duplicate code throughout both installers.

!include "FileFunc.nsh"

ManifestDPIAware true
Unicode true
ShowInstDetails nevershow
ShowUninstDetails nevershow

SetCompressor /SOLID lzma
SetCompressorDictSize 24

Var UserPrivileges
Var IsAdmin

!ifndef APP_VERSION
  !define APP_VERSION      "1.8.0"
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
  ${ElseIfNot} ${AtLeastWin8.1}
    MessageBox MB_OK "Your operating system is unsupported by PCSX2. Please upgrade your operating system or install PCSX2 1.6.0."
    Quit
  ${EndIf}

UserInfo::GetOriginalAccountType
Pop $UserPrivileges

  # GetOriginalAccountType will check the tokens of the original user of the
  # current thread/process. If the user tokens were elevated or limited for
  # this process, GetOriginalAccountType will return the non-restricted
  # account type.
  # On Windows with UAC, for example, this is not the same value when running
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

Function ShowHelpMessage
  !define line1 "Command line options:$\r$\n$\r$\n"
  !define line2 "/S - silent install (must be uppercase)$\r$\n"
  !define line3 "/D=path\to\install\folder - Change install directory (Must be uppercase, the last option given and no quotes)$\r$\n"
  !define line4 "/NoStart - Do not create start menu shortcut$\r$\n"
  !define line5 "/NoDesktop - Do not create desktop shortcut$\r$\n"
  !define line6 "/Portable- Install in portable mode instead of full install, no effect unless /S is passed as well"
  MessageBox MB_OK "${line1}${line2}${line3}${line4}${line5}${line6}"
  Abort
FunctionEnd

Function .onInit
    Var /GLOBAL cmdLineParams
    Push $R0
    ${GetParameters} $cmdLineParams
    ClearErrors

    ${GetOptions} $cmdLineParams '/?' $R0
    IfErrors +2 0
    Call ShowHelpMessage

    ${GetOptions} $cmdLineParams '/H' $R0
    IfErrors +2 0
    Call ShowHelpMessage

    Pop $R0


    Var /GLOBAL option_startMenu
    Var /GLOBAL option_desktop
    Var /GLOBAL option_portable
    StrCpy $option_startMenu     1
    StrCpy $option_desktop       1
    StrCpy $option_portable      0

    Push $R0

    ${GetOptions} $cmdLineParams '/NoStart' $R0
    IfErrors +2 0
    StrCpy $option_startMenu 0

    ${GetOptions} $cmdLineParams '/NoDesktop' $R0
    IfErrors +2 0
    StrCpy $option_desktop 0

    ${GetOptions} $cmdLineParams '/Portable' $R0
    IfErrors +2 0
    StrCpy $option_portable 1

    Pop $R0
    
FunctionEnd