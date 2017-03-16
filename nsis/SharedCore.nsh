
  ; --- UAC NIGHTMARES ---
  ; Ideally this would default to 'current' for user-level installs and 'all' for admin-level installs.
  ; There are problems to be aware of, however!
  ;
  ; * If the user is an admin, Windows Vista/7 will DEFAULT to an "all" shell context (installing shortcuts
  ;   for all users), even if we don't want it to (which causes the uninstaller to fail!)
  ; * If the user is not an admin, setting Shell Context to all will cause the installer to fail because the
  ;   user won't have permission enough to install it at all (sigh).

  ; (note!  the SetShellVarContext use in the uninstaller section must match this one!)

  ;SetShellVarContext all
  ;SetShellVarContext current

Section "!${APP_NAME} (required)" SEC_CORE

    SectionIn RO

  SetOutPath "$INSTDIR"
    File ..\bin\pcsx2.exe
    File ..\bin\GameIndex.dbf
    File ..\bin\cheats_ws.zip
    File ..\bin\PCSX2_keys.ini.default

  SetOutPath "$INSTDIR\Docs"
    File ..\bin\docs\*

  SetOutPath "$INSTDIR\Shaders"
    File ..\bin\shaders\GSdx.fx
    File ..\bin\shaders\GSdx_FX_Settings.ini

  SetOutPath "$INSTDIR\Plugins"
    File /nonfatal ..\bin\Plugins\gsdx32-sse2.dll
    File /nonfatal ..\bin\Plugins\gsdx32-sse4.dll
    File /nonfatal ..\bin\Plugins\gsdx32-avx2.dll
    File /nonfatal ..\bin\Plugins\spu2-x.dll
    File /nonfatal ..\bin\Plugins\cdvdGigaherz.dll
    File /nonfatal ..\bin\Plugins\lilypad.dll
    File /nonfatal ..\bin\Plugins\USBnull.dll
    File /nonfatal ..\bin\Plugins\DEV9null.dll
    File /nonfatal ..\bin\Plugins\FWnull.dll
SectionEnd

Section "Additional Languages" SEC_LANGS
    SetOutPath $INSTDIR\Langs
    File /nonfatal /r ..\bin\Langs\*.mo
SectionEnd

!include "SharedShortcuts.nsh"

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