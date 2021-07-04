; PCSX2 Pre-Installer Script
; Copyright (C) 2019 PCSX2 Team

!include "SharedDefs.nsh"

RequestExecutionLevel user

!define OUTFILE_POSTFIX "setup"
OutFile "pcsx2-${APP_VERSION}-${OUTFILE_POSTFIX}.exe"

; Dialogs and Controls
Var PreInstall_Dialog
Var PreInstall_DlgBack
Var PreInstall_DlgNext

Var InstallMode_Dialog
Var InstallMode_DlgBack
Var InstallMode_DlgNext
Var InstallMode_Label

# Normal installer mode (writes to Program Files)
Var InstallMode_Normal

# Portable installer mode
Var InstallMode_Portable

!include "nsDialogs.nsh"

Page Custom IsUserAdmin
Page Custom PreInstallDialog
Page Custom InstallMode InstallModeLeave

; Function located in SharedDefs
Section ""
Call IsUserAdmin
IfSilent 0 +5
Call TempFilesOut
${If} $option_portable == 0
  Call StartFullInstaller
${EndIf}
SectionEnd

Function PreInstallDialog

nsDialogs::Create /NOUNLOAD 1018
Pop $PreInstall_Dialog

    GetDlgItem $PreInstall_DlgBack $HWNDPARENT 3
    EnableWindow $PreInstall_DlgBack ${SW_HIDE}

    GetDlgItem $PreInstall_DlgNext $HWNDPARENT 1
    EnableWindow $PreInstall_DlgNext 0

  ${NSD_CreateTimer} NSD_Timer.Callback 1

nsDialogs::Show
FunctionEnd

Function NSD_Timer.Callback
${NSD_KillTimer} NSD_Timer.Callback

;-----------------------------------------
; Copy installer files to a temp directory instead of repacking twice (for each installer)
    ${NSD_CreateLabel} 0 45 80% 10u "Unpacking files. Maybe it's time to upgrade that computer!"
    Call TempFilesOut
    ${NSD_CreateLabel} 0 45 100% 10u "Moving on"
;-----------------------------------------

    Call PreInstall_UsrWait
SendMessage $HWNDPARENT ${WM_COMMAND} 1 0
FunctionEnd

Function TempFilesOut
  SetOutPath "$TEMP\PCSX2 ${APP_VERSION}"
    File ..\bin\pcsx2.exe
    File ..\bin\GameIndex.yaml
    File ..\bin\cheats_ws.zip
    File ..\bin\PCSX2_keys.ini.default
  SetOutPath "$TEMP\PCSX2 ${APP_VERSION}\Docs"
    File ..\bin\docs\*

  SetOutPath "$TEMP\PCSX2 ${APP_VERSION}\Shaders"
    File ..\bin\shaders\GS.fx
    File ..\bin\shaders\GS_FX_Settings.ini

  SetOutPath "$TEMP\PCSX2 ${APP_VERSION}\Langs"
    File /nonfatal /r ..\bin\Langs\*.mo
FunctionEnd

Function PreInstall_UsrWait
GetDlgItem $PreInstall_DlgNext $HWNDPARENT 1
EnableWindow $PreInstall_DlgNext 1
FunctionEnd

# Creates the first dialog "section" to display a choice of installer modes.
Function InstallMode
nsDialogs::Create /NOUNLOAD 1018
Pop $InstallMode_Dialog

GetDlgItem $InstallMode_DlgBack $HWNDPARENT 3
EnableWindow $InstallMode_DlgBack 0

GetDlgItem $InstallMode_DlgNext $HWNDPARENT 1
EnableWindow $InstallMode_DlgNext 0

${NSD_CreateLabel} 0 0 100% 10u "Select an installation mode for PCSX2."
Pop $InstallMode_Label

${NSD_CreateRadioButton} 0 35 100% 10u "Normal Installation"
Pop $InstallMode_Normal

# If the user doesn't have admin rights, disable the button for the normal (non-portable) installer
${If} $IsAdmin == 0
EnableWindow $InstallMode_Normal 0
${EndIf}

# Create labels/buttons for the normal installation
${NSD_OnClick} $InstallMode_Normal InstallMode_UsrWait
${NSD_CreateLabel} 10 55 100% 20u "PCSX2 will be installed in Program Files unless another directory is specified. User files are stored in the Documents/PCSX2 directory."

# Create labels/buttons for the portable installation
${NSD_CreateRadioButton} 0 95 100% 10u "Portable Installation"
Pop $InstallMode_Portable
${NSD_OnClick} $InstallMode_Portable InstallMode_UsrWait
${NSD_CreateLabel} 10 115 100% 20u "Install PCSX2 to any directory you want. Choose this option if you prefer to have all of your files in the same folder or frequently update PCSX2 through Orphis' Buildbot."

nsDialogs::Show

FunctionEnd

# Disables the "next" button until a selection has been made
Function InstallMode_UsrWait
GetDlgItem $InstallMode_DlgNext $HWNDPARENT 1
EnableWindow $InstallMode_DlgNext 1

# Displays a UAC shield on the button
${NSD_GetState} $InstallMode_Normal $0
${NSD_GetState} $InstallMode_Portable $1

${If} ${BST_CHECKED} == $0
SendMessage $InstallMode_DlgNext ${BCM_SETSHIELD} 0 1
${Else}
SendMessage $InstallMode_DlgNext ${BCM_SETSHIELD} 0 0
${EndIf}

FunctionEnd

# Runs the elevated installer and quits the current one
# If they chose portable mode, the current (unelevated installer)
# will still be used.
Function InstallModeLeave
${NSD_GetState} $InstallMode_Normal $0
${NSD_GetState} $InstallMode_Portable $1

${If} ${BST_CHECKED} == $0
Call StartFullInstaller
${EndIf}
FunctionEnd

Function StartFullInstaller
  ;Checks if install directory is changed from default with /D, and if not, changes to standard full install directory.
  ${If} $INSTDIR == "$DOCUMENTS\PCSX2 ${APP_VERSION}"
  StrCpy $INSTDIR "$PROGRAMFILES\PCSX2"
  ${EndIf}
  SetOutPath "$TEMP"
  File "pcsx2-${APP_VERSION}-include_standard.exe"
  ExecShell open "$TEMP\pcsx2-${APP_VERSION}-include_standard.exe" "$cmdLineParams /D=$INSTDIR"
  Quit
FunctionEnd

; ----------------------------------
;     Portable Install Section
; ----------------------------------
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\pcsx2.exe"
!define MUI_PAGE_CUSTOMFUNCTION_SHOW ModifyRunCheckbox
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
!include "ApplyExeProps.nsh"

; The default installation directory for the portable binary.
InstallDir "$DOCUMENTS\PCSX2 ${APP_VERSION}"

; Path references for the core files here
!include "SharedCore.nsh"

Section "" INST_PORTABLE
SetOutPath "$INSTDIR"
File portable.ini
RMDir /r "$TEMP\PCSX2 ${APP_VERSION}"
SectionEnd

Section "" SID_PCSX2
SectionEnd

# Gives the user a fancy checkbox to run PCSX2 right from the installer!
Function ModifyRunCheckbox
${IfNot} ${SectionIsSelected} ${SID_PCSX2}
    SendMessage $MUI.FINISHPAGE.RUN ${BM_SETCHECK} ${BST_UNCHECKED} 0
    EnableWindow $MUI.FINISHPAGE.RUN 0
${EndIf}
FunctionEnd
