
; PCSX2 Full/Complete Install Package!
; (a NSIS installer script)
;
; Copyright 2009-2017  PCSX2 Dev Team
;

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
!include "StrContains.nsh"
; =======================================================================
;                            Installer Sections
; =======================================================================

; The "" makes the section hidden.
Section "" SecUninstallPrevious

    Call UninstallPrevious

SectionEnd

Function UninstallPrevious

; Here's how StrContains works:
; $result_var: This will store our result if the "needle" is found. Otherwise it will return null ("")
; $needle: String to search for
; $Haystack: String to look in

; Used for the InstalledVersion string to prevent conflicts with 1.0.0 RegString
ReadRegStr $R7 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2Directory"
${If} $R7 == ""
${EndIf}

; This will become the primary version check once more stable builds
ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "InstalledVersion"
${If} $R1 == ""
${EndIf}

; Search for 1.0.0
ReadRegStr $R4 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2Directory"
${StrContains} "$4" "1.0.0" "$R4"
    StrCmp $4 "" +1
${If} $R4 == ""
${EndIf}

; Search for 1.4.0
ReadRegStr $R2 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2 1.4.0Directory"
${StrContains} "$2" "1.4.0" "$R2"
    StrCmp $2 "" +1
${If} $R2 == ""
${EndIf}

; Search for 1.2.1
ReadRegStr $R3 HKLM "${INSTDIR_REG_KEY}-r5875" "Uninst-pcsx2-r5875Directory"
${StrContains} "$3" "1.2.1" "$R3"
    StrCmp $3 "" +1
${If} $R3 == ""
${EndIf}

; Search for 0.9.8
ReadRegStr $R5 HKLM "${INSTDIR_REG_KEY}-r4600" "Uninst-pcsx2-r4600Directory"
${StrContains} "$5" "0.9.8" "$R5"
    StrCmp $5 "" +1
${If} $R5 == ""
${EndIf}

; If all cases return null, our work here is done.
${If} $R1 == ""
${AndIf} $R2 == ""
${AndIf} $R3 == ""
${AndIf} $R4 == ""
${AndIf} $R5 == ""
${AndIf} $R7 == ""
Goto Done
${EndIf}

; Installing another version
    MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "Another version of PCSX2 is already installed. Please back up your files and click OK to uninstall PCSX2; or Cancel to abort the setup." IDOK true IDCANCEL false
true:
Goto SetUninstPath
    false:
        Quit

SetUninstPath:
${If} $R7 != ""
    ${AndIf} $R1 != ""
    Goto ExecNormal
${Else}
        Goto +1
${EndIf}

${If} $R4 != ""
    ${AndIf} $4 == "1.0.0"
    Goto Exec1.0.0
${Else}
        Goto +1
${EndIf}

${If} $R2 != ""
    ${AndIf} $2 == "1.4.0"
    Goto Exec1.4.0
${Else}
        Goto +1
${EndIf}

${If} $R3 != ""
    ${AndIf} $3 == "1.2.1"
    Goto Exec1.2.1
${Else}
        Goto +1
${EndIf}

${If} $R5 != ""
    ${AndIf} $5 == "0.9.8"
    Goto Exec0.9.8
${Else}
        Goto +1
${EndIf}

 ;Run the uninstaller silently.
Exec1.4.0: 
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R2\Uninst-pcsx2 1.4.0.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2 1.4.0.exe" /S _?=$R2'
        Delete "$TEMP\Uninst-pcsx2 1.4.0.exe"
        Goto Done

Exec1.2.1: ;GOOD
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R3\Uninst-pcsx2-r5875.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r5875.exe" /S _?=$R3'
        Delete "$TEMP\Uninst-pcsx2-r5875.exe"
        Goto Done

Exec1.0.0:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R4\Uninst-pcsx2.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R4'
        Delete "$TEMP\Uninst-pcsx2.exe"
        Goto Done

Exec0.9.8:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R5\Uninst-pcsx2-r4600.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r4600.exe" /S _?=$R5'
        Delete "$TEMP\Uninst-pcsx2-r4600.exe"
        Goto Done

ExecNormal:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R7\Uninst-pcsx2.exe" "$TEMP"
            ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R7'
            Delete "$TEMP\Uninst-pcsx2.exe"

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
    File /nonfatal ..\bin\Plugins\gsdx32-sse4.dll
    File /nonfatal ..\bin\Plugins\gsdx32-avx2.dll
    File /nonfatal ..\bin\Plugins\spu2-x.dll
    File /nonfatal ..\bin\Plugins\cdvdGigaherz.dll
    File /nonfatal ..\bin\Plugins\lilypad.dll
    File /nonfatal ..\bin\Plugins\padPokopom.dll

    File /nonfatal ..\bin\Plugins\USBnull.dll
    File /nonfatal ..\bin\Plugins\DEV9null.dll
    File /nonfatal ..\bin\Plugins\FWnull.dll
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

Section "" SEC_DXRedists
!include WinVer.nsh

${IfNot} ${AtLeastWin8.1}
Goto InstallRedist
${ElseIf} ${AtLeastWin8.1} 
    Goto SkipDx
${EndIf}

InstallRedist:
ReadRegDword $R0 HKLM "SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"
    ReadRegDword $R0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"

${If} $R0 == "1"
Goto done
${Else} 
    Goto +2
${EndIf}

  SetOutPath "$TEMP"
  File "vcredist_2015_Update_1_x86.exe"
  ExecWait '"$TEMP\vcredist_2015_Update_1_x86.exe" /qb'
  DetailPrint "Finished Visual C++ 2015 Redistributable Setup"
  Delete "$TEMP\vcredist_2015_Update_1_x86.exe"

done:

;DirectX Web Setup
 
 SetOutPath "$TEMP"
 File "dxwebsetup.exe"
 DetailPrint "Running DirectX Web Setup..."
 ExecWait '"$TEMP\dxwebsetup.exe" /Q' $DirectXSetupError
 DetailPrint "Finished DirectX Web Setup"

 Delete "$TEMP\dxwebsetup.exe"

 SkipDX:
SectionEnd

!include "SectionUninstaller.nsh"

LangString DESC_CORE       ${LANG_ENGLISH} "Core components (binaries, plugins, documentation, etc)."
LangString DESC_STARTMENU  ${LANG_ENGLISH} "Adds shortcuts for PCSX2 to the start menu (all users)."
LangString DESC_DESKTOP    ${LANG_ENGLISH} "Adds a shortcut for PCSX2 to the desktop (all users)."
LangString DESC_LANGS      ${LANG_ENGLISH} "Adds additional languages other than the system default to PCSX2."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}        $(DESC_CORE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU}   $(DESC_STARTMENU)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP}     $(DESC_DESKTOP)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_LANGS}       $(DESC_LANGS)

!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "" SID_PCSX2

SectionEnd

Function ModifyRunCheckbox
${IfNot} ${SectionIsSelected} ${SID_PCSX2}
    SendMessage $MUI.FINISHPAGE.RUN ${BM_SETCHECK} ${BST_UNCHECKED} 0
    EnableWindow $MUI.FINISHPAGE.RUN 0
${EndIf}
FunctionEnd
