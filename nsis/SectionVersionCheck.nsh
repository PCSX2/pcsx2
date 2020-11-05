; Copyright (C) 2019 PCSX2 Team

Function UninstallPrevious

; This will become the primary version check
ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "DisplayVersion"
ReadRegStr $R2 HKLM Software\PCSX2 "Install_Dir"
${If} $R1 != ""
${AndIf} $R2 != ""
    Goto UserPrompt
${EndIf}

; If all cases return null, our work here is done.
${If} $R2 == ""
    Return
${EndIf}

UserPrompt:
; Installing another version
MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "An existing version of PCSX2 has been detected and will be REMOVED. The config folder in Documents will be duplicated (if it exists) and renamed as PCSX2_backup. Backup any important files and click OK to uninstall PCSX2 or Cancel to abort the setup." /SD IDOK IDOK SetUninstPath IDCANCEL false

false:
Quit

SetUninstPath:
SetOutPath "$DOCUMENTS"
CopyFiles /SILENT "$DOCUMENTS\PCSX2" "$DOCUMENTS\PCSX2_backup"
RMDir /r "$DOCUMENTS\PCSX2"

${If} $R1 != ""
    Goto ExecNormal
${EndIf}

ExecNormal:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R2\Uninst-pcsx2.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R2'
        Delete "$TEMP\Uninst-pcsx2.exe"
        Return
FunctionEnd
