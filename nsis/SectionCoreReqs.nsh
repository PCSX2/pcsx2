
  ; --- UAC NIGHTMARES ---
  ; Ideally this would default to 'current' for user-level installs and 'all' for admin-level installs.
  ; There are problems to be aware of, however!
  ;
  ; * If the user is an admin, Windows Vista/7 will DEFAULT to an "all" shell context (installing shortcuts
  ;   for all users), even if we don't want it to (which causes the uninstaller to fail!)
  ; * If the user is not an admin, setting Shell Context to all will cause the installer to fail because the
  ;   user won't have permission enough to install it at all (sigh).
  ;
  ; For now we just require Admin rights to install PCSX2.  An ideal solution would be to use our IsUserAdmin
  ; function to auto-detect and modify nsis installer behavior accordingly.
  ;
  ; (note!  the SetShellVarContext use in the uninstaller section must match this one!)

  SetShellVarContext all
  ;SetShellVarContext current

  SetOutPath "$INSTDIR"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL
    File ..\bin\pcsx2.exe
    ;File /nonfatal ..\bin\pcsx2-dev.exe

  ; ------------------------------------------
  ;       -- Shared Core Components --
  ; ------------------------------------------
  ; (Binaries, shared DLLs, null plugins, game database, languages, etc)

    File ..\bin\GameIndex.dbf
    File ..\bin\cheats_ws.zip
    File ..\bin\PCSX2_keys.ini.default

    !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

    SetOutPath "$INSTDIR\Docs"
    !insertmacro UNINSTALL.LOG_OPEN_INSTALL
    File ..\bin\docs\*
    !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

    SetOutPath "$INSTDIR\Shaders"
    !insertmacro UNINSTALL.LOG_OPEN_INSTALL
    File ..\bin\shaders\*
    !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

    SetOutPath "$INSTDIR\Plugins"
    !insertmacro UNINSTALL.LOG_OPEN_INSTALL
    ; NULL plugins are required, because the PCSX2 plugin selector needs a dummy plugin in every slot
    ; in order to run (including CDVD!)

    File ..\bin\Plugins\USBnull.dll
    File ..\bin\Plugins\DEV9null.dll
    File ..\bin\Plugins\FWnull.dll
  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL


  ; ------------------------------------------
  ;         -- Registry Section --
  ; ------------------------------------------

  ; Write the installation path into the registry
  WriteRegStr HKLM Software\PCSX2 "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "DisplayName"      "PCSX2 - Playstation 2 Emulator"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "UninstallString"  "${UNINST_EXE}"
  WriteRegStr   HKLM "${INSTDIR_REG_KEY}"  "InstalledVersion" "${APP_VERSION}"
  WriteRegDWORD HKLM "${INSTDIR_REG_KEY}"  "NoModify" 1
  WriteRegDWORD HKLM "${INSTDIR_REG_KEY}"  "NoRepair" 1
  WriteUninstaller "${UNINST_EXE}"
