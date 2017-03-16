; PCSX2 Pre-Installer Script
; Copyright (C) 2017 Christian Kenny
; Copyright (C) 2017 PCSX2 Team

!include "SharedDefs.nsh"

RequestExecutionLevel user

!define OUTFILE_POSTFIX "setup"
OutFile "pcsx2-${APP_VERSION}-${OUTFILE_POSTFIX}.exe"

Var UserPrivileges
Var IsAdmin
Var DirectXSetupError

; Dialog Vars
Var hwnd
Var PreInstall_Dialog
Var PreInstall_DlgBack
Var PreInstall_DlgNext

Var InstallMode_Dialog
Var InstallMode_DlgBack
Var InstallMode_DlgNext
Var InstallMode_Label
Var InstallMode_Full
Var InstallMode_Portable
!include "nsDialogs.nsh"

Page Custom IsUserAdmin
Page Custom PreInstallDialog
Page Custom InstallMode InstallModeLeave

Function IsUserAdmin
!include WinVer.nsh
  ${IfNot} ${AtLeastWinVista}
    MessageBox MB_OK "Your operating system is unsupported by PCSX2. Please upgrade your operating system or install PCSX2 1.4.0."
    Quit
  ${EndIf}

ClearErrors
UserInfo::GetName
  Pop $R8

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

Function PreInstallDialog

nsDialogs::Create /NOUNLOAD 1018
Pop $PreInstall_Dialog

    GetDlgItem $PreInstall_DlgBack $HWNDPARENT 3
    EnableWindow $PreInstall_DlgBack ${SW_HIDE}

    GetDlgItem $PreInstall_DlgNext $HWNDPARENT 1
    EnableWindow $PreInstall_DlgNext 0

${NSD_CreateProgressBar} 0 75 100% 10% "Test"
    Pop $hwnd

  ${NSD_CreateTimer} NSD_Timer.Callback 1

nsDialogs::Show
FunctionEnd

Function NSD_Timer.Callback
${NSD_KillTimer} NSD_Timer.Callback
    SendMessage $hwnd ${PBM_SETRANGE32} 0 100

!include WinVer.nsh
!include "X64.nsh" 
${If} ${AtLeastWin8.1}
${OrIf} $IsAdmin == 0
Call PreInstall_UsrWait
SendMessage $HWNDPARENT ${WM_COMMAND} 1 0

${EndIf}

${If} ${RunningX64}
ReadRegDword $R0 HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
${Else}
   ReadRegDword $R0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
${EndIf}
    Pop $R0

${If} $R0 == "1"
Goto ExecDxSetup
${EndIf}

${NSD_CreateLabel} 0 45 100% 10u "Downloading Visual C++ 2015 package"
Pop $hwnd
inetc::get "https://download.microsoft.com/download/9/3/F/93FCF1E7-E6A4-478B-96E7-D4B285925B00/vc_redist.x86.exe" "$TEMP\vcredist_2015_Update_1_x86.exe" /SILENT /CONNECTTIMEOUT 30 /RECEIVETIMEOUT 30 /END
    ${NSD_CreateLabel} 0 45 100% 10u "Installing Visual C++ 2015 package"
    Pop $hwnd
    ExecWait '"$TEMP\vcredist_2015_Update_1_x86.exe /S"'
    SendMessage $hwnd ${PBM_SETPOS} 40 0
    Delete "$TEMP\vcredist_2015_Update_1_x86.exe"

ExecDxSetup:

${NSD_CreateLabel} 0 45 100% 10u "Installing DXWebSetup package"
Pop $hwnd
SendMessage $hwnd ${PBM_SETPOS} 80 0

SetOutPath "$TEMP"
File "dxwebsetup.exe"
ExecWait '"$TEMP\dxwebsetup.exe" /Q' $DirectXSetupError

SendMessage $hwnd ${PBM_SETPOS} 100 0
Delete "$TEMP\dxwebsetup.exe"
Sleep 20
    Call PreInstall_UsrWait
SendMessage $HWNDPARENT ${WM_COMMAND} 1 0

FunctionEnd

Function PreInstall_UsrWait
GetDlgItem $PreInstall_DlgNext $HWNDPARENT 1
EnableWindow $PreInstall_DlgNext 1
FunctionEnd

Function InstallMode

nsDialogs::Create /NOUNLOAD 1018
Pop $InstallMode_Dialog

    GetDlgItem $InstallMode_DlgBack $HWNDPARENT 3
    EnableWindow $InstallMode_DlgBack 0

    GetDlgItem $InstallMode_DlgNext $HWNDPARENT 1
    EnableWindow $InstallMode_DlgNext 0

${NSD_CreateLabel} 0 0 100% 10u "Select an installation mode for PCSX2."
Pop $InstallMode_Label

${NSD_CreateRadioButton} 0 35 100% 10u "Full Installation"
Pop $InstallMode_Full

${If} $IsAdmin == 0
EnableWindow $InstallMode_Full 0
${EndIf}

${NSD_OnClick} $InstallMode_Full InstallMode_UsrWait
${NSD_CreateLabel} 10 55 100% 20u "PCSX2 will be installed in Program Files unless another directory is specified. User files are stored in the Documents/PCSX2 directory."

${NSD_CreateRadioButton} 0 95 100% 10u "Portable Installation"
Pop $InstallMode_Portable
    ${NSD_OnClick} $InstallMode_Portable InstallMode_UsrWait
    ${NSD_CreateLabel} 10 115 100% 20u "Install PCSX2 to any directory you want. Choose this option if you prefer to have all of your files in the same folder or frequently update PCSX2 through Orphis' Buildbot."

nsDialogs::Show

FunctionEnd

Function InstallMode_UsrWait
GetDlgItem $InstallMode_DlgNext $HWNDPARENT 1
EnableWindow $InstallMode_DlgNext 1
FunctionEnd

Function InstallModeLeave
${NSD_GetState} $InstallMode_Full $0
${NSD_GetState} $InstallMode_Portable $1

${If} ${BST_CHECKED} == $0
SetOutPath "$TEMP"
File "pcsx2-${APP_VERSION}-include_standard.exe"
ExecShell open "$TEMP\pcsx2-${APP_VERSION}-include_standard.exe"
Quit
${EndIf}
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
InstallDir "$DOCUMENTS\$R8\PCSX2 ${APP_VERSION}"

; Installed files are housed here
!include "SharedCore.nsh"

Section "" INST_PORTABLE

SetOutPath "$INSTDIR"
File portable.ini
SectionEnd

Section "" SID_PCSX2
SectionEnd

Function ModifyRunCheckbox
${IfNot} ${SectionIsSelected} ${SID_PCSX2}
    SendMessage $MUI.FINISHPAGE.RUN ${BM_SETCHECK} ${BST_UNCHECKED} 0
    EnableWindow $MUI.FINISHPAGE.RUN 0
${EndIf}
FunctionEnd