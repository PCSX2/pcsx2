Section "Start Menu Shortcuts" SEC_STARTMENU

  ; CreateShortCut gets the working directory from OutPath
  SetOutPath "$INSTDIR"
  CreateShortCut "$SMPROGRAMS\${APP_NAME}.lnk"                "${APP_EXE}"         ""    "${APP_EXE}"       0
SectionEnd

Section "Desktop Shortcut" SEC_DESKTOP

  ; CreateShortCut gets the working directory from OutPath
  SetOutPath "$INSTDIR"
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk"            "${APP_EXE}"      "" "${APP_EXE}"     0 "" "" "A Playstation 2 Emulator"
SectionEnd