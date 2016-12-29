
; PCSX2 Full/Complete Install Package!
; (a NSIS installer script)
;
; Copyright 2009-2016  PCSX2 Dev Team
;


!ifndef INC_CRT_2015
  ; Set to 0 to disable inclusion of Visual Studio 2013 SP1 CRT Redists
  !define INC_CRT_2015  1
!endif

ManifestDPIAware true
ShowInstDetails nevershow
ShowUninstDetails nevershow

!define OUTFILE_POSTFIX "setup"
!include "SharedBase.nsh"
!include "x64.nsh"

; Reserve features for improved performance with solid archiving.
;  (uncomment if we add our own install options ini files)
;!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
;!insertmacro MUI_RESERVEFILE_LANGDLL

!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; This hack is required to launch pcsx2.exe from the FINISHPAGE of the installer.
; RequestExecutionLevel defaults to admin as of NSIS 3.0; and UAC behavior is defined
; as such that any elevation rights are transferred to child processes

!define MUI_FINISHPAGE_RUN "$WINDIR\explorer.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS "$INSTDIR\pcsx2.exe"
!define MUI_PAGE_CUSTOMFUNCTION_SHOW ModifyRunCheckbox
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!include "ApplyExeProps.nsh"

; =======================================================================
;                            Installer Sections
; =======================================================================

; -----------------------------------------------------------------------

; The "" makes the section hidden.
Section "" SecUninstallPrevious

    Call UninstallPrevious

SectionEnd

Function UninstallPrevious
 ;Check for uninstaller.
    ReadRegStr $R0 HKLM "${INSTDIR_REG_KEY}" "UninstallString"
${If} $R0 == ""
        Goto Done
${EndIf}

 ;Check if any other version is installed
    ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "InstalledVersion"

 ;This check for older versions (pre 1.6.0) without InstalledVersion string will bypass this section
${If} $R1 == ""
    DetailPrint "InstalledVersion string not found, skipping version check"
    Goto Done
${EndIf}

 ;Installing same version
${If} $R1 S== ${APP_VERSION}
    MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "This version of PCSX2 is already installed. Do you want to continue?" IDCANCEL false
        DetailPrint "Overwriting current install"
        Goto Done
    false:
        Quit
${Else}
        DetailPrint "Not the same version"
${EndIf}

 ;Installing newer version (and old version is detected)
${If} $R1 S< ${APP_VERSION}
        MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "An older version of PCSX2 is installed. Do you want to uninstall it?" IDOK true2 IDCANCEL false2
        true2: 
        DetailPrint "Uninstalling old PCSX2 version"
        Goto Next
            false2:
            Quit
${Else}
            DetailPrint "Not installing a new version"
${EndIf}

 ;Run the uninstaller silently.
    Next:
        DetailPrint "Running silent uninstall"

 ;Copy files to a temp dir to prevent conflicts when removing $INSTDIR/uninstaller
    ReadRegStr $R2 HKLM Software\PCSX2 "Install_Dir"
    CreateDirectory "$TEMP\pcsx2_uninst_temp"
    CopyFiles /SILENT /FILESONLY "$R2\uninst-pcsx2.exe" "$TEMP\pcsx2_uninst_temp"

    ExecWait '"$TEMP\pcsx2_uninst_temp\uninst-pcsx2.exe" /S _?=$R2'
    RMDir /r $TEMP\pcsx2_uninst_temp
    Done:

FunctionEnd

; Basic section (emulation proper)
Section "!${APP_NAME} (required)" SEC_CORE

  SectionIn RO

  !include "SectionCoreReqs.nsh"

  ; ------------------------------------------
  ;          -- Plugins Section --
  ; ------------------------------------------

!if ${INC_PLUGINS} > 0

  SetOutPath "$INSTDIR\Plugins"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL

    File /nonfatal ..\bin\Plugins\gsdx32-sse2.dll
    File /nonfatal ..\bin\Plugins\gsdx32-ssse3.dll 
    File /nonfatal ..\bin\Plugins\gsdx32-sse4.dll
    File /nonfatal ..\bin\Plugins\gsdx32-avx.dll
    File /nonfatal ..\bin\Plugins\gsdx32-avx2.dll
    File /nonfatal ..\bin\Plugins\spu2-x.dll
    File /nonfatal ..\bin\Plugins\cdvdGigaherz.dll
    File /nonfatal ..\bin\Plugins\lilypad.dll
    File /nonfatal ..\bin\Plugins\padPokopom.dll

  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

!endif

SectionEnd

!include "SectionShortcuts.nsh"

Section "Additional Languages" SEC_LANGS
    SetOutPath $INSTDIR\Langs
    !insertmacro UNINSTALL.LOG_OPEN_INSTALL
    File /nonfatal /r ..\bin\Langs\*.mo
    !insertmacro UNINSTALL.LOG_CLOSE_INSTALL
SectionEnd

SectionGroup "DirectX Packages (required for PCSX2)" SEC_DXPACKS
!if ${INC_CRT_2015} > 0
Section "Microsoft Visual C++ 2015 Redist" SEC_CRT2015

  ;SectionIn RO

  ; Detection made easy: Unlike previous redists, VC2015 now generates a platform
  ; independent key for checking availability.
  ; HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86  for x64 Windows
  ; HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86  for x86 Windows

  ; Downloaded from:
  ;   https://www.microsoft.com/en-us/download/details.aspx?id=49984

  ClearErrors

  ${If} ${RunningX64}
  	ReadRegDword $R0 HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
	${Else}
		ReadRegDword $R0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
	${EndIf}

  IfErrors 0 +2
  DetailPrint "Visual C++ 2015 Redistributable registry key was not found; assumed to be uninstalled."
  StrCmp $R0 "1" 0 +3
    DetailPrint "Visual C++ 2015 Redistributable is already installed; skipping!"
    Goto done

  SetOutPath "$TEMP"
  File "vcredist_2015_Update_1_x86.exe"
  DetailPrint "Running Visual C++ 2015 Redistributable Setup..."
  ExecWait '"$TEMP\vcredist_2015_Update_1_x86.exe" /qb'
  DetailPrint "Finished Visual C++ 2015 Redistributable Setup"

  Delete "$TEMP\vcredist_2015_Update_1_x86.exe"

done:
SectionEnd
!endif

; -----------------------------------------------------------------------
; This section needs to be last, so that in case it fails, the rest of the program will
; be installed cleanly.
; 
; This section could be optional, but why not?  It's pretty painless to double-check that
; all the libraries are up-to-date.
;
Section "DirectX Web Setup" SEC_DIRECTX

 ;SectionIn RO

 SetOutPath "$TEMP"
 File "dxwebsetup.exe"
 DetailPrint "Running DirectX Web Setup..."
 ExecWait '"$TEMP\dxwebsetup.exe" /Q' $DirectXSetupError
 DetailPrint "Finished DirectX Web Setup"

 Delete "$TEMP\dxwebsetup.exe"

SectionEnd
SectionGroupEnd

!include "SectionUninstaller.nsh"

LangString DESC_CORE       ${LANG_ENGLISH} "Core components (binaries, plugins, documentation, etc)."

LangString DESC_STARTMENU  ${LANG_ENGLISH} "Adds shortcuts for PCSX2 to the start menu (all users)."
LangString DESC_DESKTOP    ${LANG_ENGLISH} "Adds a shortcut for PCSX2 to the desktop (all users)."
LangString DESC_LANGS      ${LANG_ENGLISH} "Adds additional languages other than the system default to PCSX2."

LangString DESC_DXPACKS    ${LANG_ENGLISH} "Installs the Visual C++ Redistributable and DirectX SDK"
LangString DESC_CRT2015    ${LANG_ENGLISH} "Required by the PCSX2 binaries packaged in this installer."
LangString DESC_DIRECTX    ${LANG_ENGLISH} "Only uncheck this if you are quite certain your Direct3D runtimes are up to date."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}        $(DESC_CORE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU}   $(DESC_STARTMENU)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP}     $(DESC_DESKTOP)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_LANGS}       $(DESC_LANGS)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DXPACKS}     $(DESC_DXPACKS)
  
!if ${INC_CRT_2015} > 0
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CRT2015}     $(DESC_CRT2015)
!endif

  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIRECTX}     $(DESC_DIRECTX)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "" SID_PCSX2

SectionEnd

Function ModifyRunCheckbox
${IfNot} ${SectionIsSelected} ${SID_PCSX2}
    SendMessage $MUI.FINISHPAGE.RUN ${BM_SETCHECK} ${BST_UNCHECKED} 0
    EnableWindow $MUI.FINISHPAGE.RUN 0
${EndIf}
FunctionEnd
