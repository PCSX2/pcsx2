; PCSX2 Full/Complete Install Package!
; (a NSIS installer script)
;
; Copyright 2009-2017 PCSX2 Dev Team

!include "SharedDefs.nsh"

RequestExecutionLevel admin

; This is the uninstaller name.
!define INSTDIR_REG_ROOT "HKLM"
!define OUTFILE_POSTFIX "include_standard"

; The installer name will read as "pcsx2-x.x.x-include_standard"
OutFile "pcsx2-${APP_VERSION}-${OUTFILE_POSTFIX}.exe"

; The default installation directory for the full installer
InstallDir "$PROGRAMFILES\PCSX2"

!define INSTDIR_REG_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_FILENAME}"

!include "SectionVersionCheck.nsh"

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; RequestExecutionLevel is admin for the full install, so we need to avoid transferring the elevated rights to PCSX2
; if the user chooses to run from the installer upon completion.
!define MUI_FINISHPAGE_RUN "$WINDIR\explorer.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "$INSTDIR\pcsx2.exe"
!define MUI_PAGE_CUSTOMFUNCTION_SHOW ModifyRunCheckbox
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"


!include "nsDialogs.nsh"
!include "ApplyExeProps.nsh"

Section "" 
Call UninstallPrevious
SectionEnd

!include "SharedCore.nsh"
!include "SectionUninstaller.nsh"

Section ""

; Write the installation path into the registry
  WriteRegStr HKLM Software\PCSX2 "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "DisplayName"      "PCSX2 - Playstation 2 Emulator"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "Publisher"        "PCSX2 Team"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "DisplayIcon"      "$INSTDIR\pcsx2.exe"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "DisplayVersion"   "${APP_VERSION}"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "HelpLink"         "https://forums.pcsx2.net"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "UninstallString"  "$INSTDIR\Uninst-pcsx2.exe"
  WriteRegDWORD HKLM "${INSTDIR_REG_KEY}"  "NoModify" 1
  WriteRegDWORD HKLM "${INSTDIR_REG_KEY}"  "NoRepair" 1
  WriteUninstaller "$INSTDIR\Uninst-pcsx2.exe"
SectionEnd

Section "" SID_PCSX2
SectionEnd

; Gives the user a fancy checkbox to run PCSX2 right from the installer!
Function ModifyRunCheckbox
${IfNot} ${SectionIsSelected} ${SID_PCSX2}
    SendMessage $MUI.FINISHPAGE.RUN ${BM_SETCHECK} ${BST_UNCHECKED} 0
    EnableWindow $MUI.FINISHPAGE.RUN 0
${EndIf}
FunctionEnd