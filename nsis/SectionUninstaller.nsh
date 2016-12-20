; =======================================================================
;                         Shared Install Functions
; =======================================================================

Function .onInit

  ;prepare Advanced Uninstall log always within .onInit function
  !insertmacro UNINSTALL.LOG_PREPARE_INSTALL

  ; MORE UAC HELL ---------- >
  ;call IsUserAdmin

FunctionEnd

Function .onInstSuccess

  ;create/update log always within .onInstSuccess function
  !insertmacro UNINSTALL.LOG_UPDATE_INSTALL

FunctionEnd

; =======================================================================
;                         Shared Uninstall Functions
; =======================================================================

Function un.removeShorties

  ; Remove shortcuts, if any

  Delete "$DESKTOP\${APP_NAME}.lnk"

  Delete "$SMPROGRAMS\PCSX2\Uninstall ${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\PCSX2\${APP_NAME}.lnk"
  ;Delete "$SMPROGRAMS\PCSX2\pcsx2-dev.lnk"

  Delete "$SMPROGRAMS\PCSX2\Readme.lnk"
  Delete "$SMPROGRAMS\PCSX2\Frequently Asked Questions.lnk"

  RMDir "$SMPROGRAMS\PCSX2"

FunctionEnd

; begin uninstall, could be added on top of uninstall section instead
Function un.onInit
  !insertmacro UNINSTALL.LOG_BEGIN_UNINSTALL
  FunctionEnd

Function un.onUninstSuccess
  !insertmacro UNINSTALL.LOG_END_UNINSTALL
  ; And remove the various install dir(s) but only if they're clean of user content:

  RMDir "$DOCUMENTS\PCSX2"
  RMDir "$INSTDIR\langs"
  RMDir "$INSTDIR\plugins"
  RMDir "$INSTDIR\docs"
  RMDir "$INSTDIR"
  FunctionEnd

; =======================================================================
;                           Un.Installer Sections
; =======================================================================


; -----------------------------------------------------------------------
Section "Un.Program and Plugins ${APP_NAME}"

  SetShellVarContext all
  ; First thing, remove the registry entry in case uninstall doesn't complete successfully
  ;   otherwise, pcsx2 will be "confused" if it's re-installed later.
  DeleteRegKey HKLM Software\PCSX2

  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR"

  ; Remove uninstaller info reg key ( Wow6432Node on 64bit Windows! )
  DeleteRegKey HKLM "${INSTDIR_REG_KEY}"

  Call un.removeShorties

  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Langs"
  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Plugins"
  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Docs"
  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Cheats"
  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Shaders"
  ; Remove files and registry key that store PCSX2 paths configurations
  SetShellVarContext current
  Delete $DOCUMENTS\PCSX2\inis\PCSX2_ui.ini

SectionEnd

; /o for optional and unticked by default
Section /o "Un.Configuration files (Programs and Plugins)"

  SetShellVarContext current
  RMDir /r "$DOCUMENTS\PCSX2\inis\"

SectionEnd

Section /o "Un.Memory cards/savestates"

  SetShellVarContext current
  RMDir /r "$DOCUMENTS\PCSX2\memcards\"
  RMDir /r "$DOCUMENTS\PCSX2\sstates\"
SectionEnd

; /o for optional and unticked by default
Section /o "Un.User files (Cheats, Logs, Snapshots)"

  SetShellVarContext current
  RMDir /r "$DOCUMENTS\PCSX2\Cheats_ws\"
  RMDir /r "$DOCUMENTS\PCSX2\cheats\"
  RMDir /r "$DOCUMENTS\PCSX2\logs\"
  RMDir /r "$DOCUMENTS\PCSX2\snaps\"

SectionEnd

; /o for optional and unticked by default
Section /o "Un.BIOS files"

  SetShellVarContext current
  RMDir /r "$DOCUMENTS\PCSX2\bios\"

SectionEnd