Section "Start Menu Shortcuts" SEC_STARTMENU
  ${If} $option_startMenu == 1
    ; CreateShortCut gets the working directory from OutPath
    SetOutPath "$INSTDIR"
    CreateShortCut "$SMPROGRAMS\${APP_NAME}.lnk"                "${APP_EXE}"         ""    "${APP_EXE}"       0
  ${EndIf}
SectionEnd

Section "Desktop Shortcut" SEC_DESKTOP
  ${If} $option_desktop == 1
    ; CreateShortCut gets the working directory from OutPath
    SetOutPath "$INSTDIR"
    CreateShortCut "$DESKTOP\${APP_NAME}.lnk"            "${APP_EXE}"      "" "${APP_EXE}"     0 "" "" "A PlayStation 2 Emulator"
  ${EndIf}
SectionEnd