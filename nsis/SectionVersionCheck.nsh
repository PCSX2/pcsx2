; Copyright (C) 2019 PCSX2 Team

Function UninstallPrevious

; Here's how StrContains works:
; $result_var: This will store our result if the "needle" is found. Otherwise it will return null ("")
; $needle: String to search for
; $Haystack: String to look in

; Search for 1.0.0
ReadRegStr $R7 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2Directory"
${StrContains} "$4" "1.0.0" "$R7"

; This will become the primary version check for 1.6.0 and later
ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "DisplayVersion"

; Search for 1.4.0
ReadRegStr $R2 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2 1.4.0Directory"
${StrContains} "$2" "1.4.0" "$R2"

; Search for 1.2.1
ReadRegStr $R3 HKLM "${INSTDIR_REG_KEY}-r5875" "Uninst-pcsx2-r5875Directory"
${StrContains} "$3" "1.2.1" "$R3"

; Search for 0.9.8
ReadRegStr $R5 HKLM "${INSTDIR_REG_KEY}-r4600" "Uninst-pcsx2-r4600Directory"
${StrContains} "$5" "0.9.8" "$R5"

; If all cases return null, our work here is done.
${If} $R2 == ""
${AndIf} $R3 == ""
${AndIf} $R5 == ""
${AndIf} $R7 == ""
Return
${EndIf}

; Installing another version
MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "Another version of PCSX2 is already installed. The current configuration folder in Documents will be duplicated and renamed as PCSX2_backup. Click OK to uninstall PCSX2 or Cancel to abort the setup." IDOK SetUninstPath IDCANCEL false

false:
Quit

SetUninstPath:
SetOutPath "$DOCUMENTS"
CopyFiles /SILENT "$DOCUMENTS\PCSX2" "$DOCUMENTS\PCSX2_backup"
RMDir /r "$DOCUMENTS\PCSX2"

${If} $R7 != ""
    ${AndIf} $R1 != ""
    ${OrIf} $4 == "1.0.0"
    Goto ExecNormal
${EndIf}

${If} $R2 != ""
    ${AndIf} $2 == "1.4.0"
    Goto Exec1.4.0
${EndIf}

${If} $R3 != ""
    ${AndIf} $3 == "1.2.1"
    Goto Exec1.2.1
${EndIf}

${If} $R5 != ""
    ${AndIf} $5 == "0.9.8"
    Goto Exec0.9.8
${EndIf}

ExecNormal:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R7\Uninst-pcsx2.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R7'
        Delete "$TEMP\Uninst-pcsx2.exe"
        Return

Exec1.4.0:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R2\Uninst-pcsx2 1.4.0.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2 1.4.0.exe" /S _?=$R2'
        Delete "$TEMP\Uninst-pcsx2 1.4.0.exe"
        DeleteRegKey HKLM Software\PCSX2
        Return

Exec1.2.1:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R3\Uninst-pcsx2-r5875.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r5875.exe" /S _?=$R3'
        Delete "$TEMP\Uninst-pcsx2-r5875.exe"
        Return

Exec0.9.8:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R5\Uninst-pcsx2-r4600.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r4600.exe" /S _?=$R5'
        Delete "$TEMP\Uninst-pcsx2-r4600.exe"
        Return

FunctionEnd