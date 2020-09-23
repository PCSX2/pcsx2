
  ; --- UAC NIGHTMARES ---
  ; Ideally this would default to 'current' for user-level installs and 'all' for admin-level installs.
  ; There are problems to be aware of, however!
  ;
  ; * If the user is an admin, Windows will DEFAULT to an "all" shell context (installing shortcuts
  ;   for all users), even if we don't want it to (which causes the uninstaller to fail!)
  ; * If the user is not an admin, setting Shell Context to all will cause the installer to fail because the
  ;   user won't have permission enough to install it at all (sigh).

  ; (note!  the SetShellVarContext use in the uninstaller section must match this one!)

  ;SetShellVarContext all
  ;SetShellVarContext current

Function RedistInstallation
!include WinVer.nsh

; Check if the VC runtimes are installed
ReadRegDword $R5 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86" "Installed"

${If} $R5 == "1"
    Return
${EndIf}

; Download and install the VC redistributable from the internet
inetc::get /CONNECTTIMEOUT 30 /RECEIVETIMEOUT 30 "https://aka.ms/vs/16/release/VC_redist.x86.exe" "$TEMP\VC_redist.x86.exe" /END
    ExecShellWait open "$TEMP\VC_redist.x86.exe" "/INSTALL /Q /NORESTART"
    Delete "$TEMP\VC_redist.x86.exe"
FunctionEnd

Section "" SEC_REDIST
Call RedistInstallation
SectionEnd

; Copy unpacked files from TEMP to the user specified directory
Section "!${APP_NAME} (required)" SEC_CORE
SectionIn RO
CopyFiles /SILENT "$TEMP\PCSX2 ${APP_VERSION}\*" "$INSTDIR\" 24000
SectionEnd

!include "SharedShortcuts.nsh"

LangString DESC_CORE       ${LANG_ENGLISH} "Core components (binaries, plugins, documentation, etc)."
LangString DESC_STARTMENU  ${LANG_ENGLISH} "Adds shortcuts for PCSX2 to the start menu (all users)."
LangString DESC_DESKTOP    ${LANG_ENGLISH} "Adds a shortcut for PCSX2 to the desktop (all users)."

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}        $(DESC_CORE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU}   $(DESC_STARTMENU)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP}     $(DESC_DESKTOP)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END