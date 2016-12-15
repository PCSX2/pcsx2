
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
  RMDir "$INSTDIR\Cheats"
  RMDir "$INSTDIR"
  FunctionEnd