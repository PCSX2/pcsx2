; Copyright (C) 2019 PCSX2 Team

Function UninstallPrevious

; Here's how StrContains works:
; $result_var: This will store our result if the "needle" is found. Otherwise it will return null ("")
; $needle: String to search for
; $Haystack: String to look in

; This will become the primary version check for 1.6.0 and later
ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "DisplayVersion"
ReadRegStr $R2 HKLM Software\PCSX2 "Install_Dir"
${If} $R1 != ""
${AndIf} $R2 != ""
    Goto UserPrompt
${EndIf}

; Search for 1.4.0
ReadRegStr $R3 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2 1.4.0Directory"
${StrContains} "$2" "1.4.0" "$R3"

; If all cases return null, our work here is done.
${If} $R2 == ""
${AndIf} $R3 == ""
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

${If} $R3 != ""
    ${AndIf} $2 == "1.4.0"
    Goto Exec1.4.0
${EndIf}

ExecNormal:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R2\Uninst-pcsx2.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R2'
        Delete "$TEMP\Uninst-pcsx2.exe"
        Return

Exec1.4.0:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R3\Uninst-pcsx2 1.4.0.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2 1.4.0.exe" /S _?=$R3'
        Delete "$TEMP\Uninst-pcsx2 1.4.0.exe"
        DeleteRegKey HKLM Software\PCSX2
        Return
FunctionEnd
