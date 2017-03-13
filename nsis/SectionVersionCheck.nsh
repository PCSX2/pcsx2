; Copyright (C) 2017 Christian Kenny

Function UninstallPrevious

; Here's how StrContains works:
; $result_var: This will store our result if the "needle" is found. Otherwise it will return null ("")
; $needle: String to search for
; $Haystack: String to look in

; Search for 1.0.0
ReadRegStr $R7 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2Directory"
${StrContains} "$4" "1.0.0" "$R7"
StrCmp $4 "" +1
${If} $R7 == ""
${EndIf}

; This will become the primary version check once more stable builds
ReadRegStr $R1 HKLM "${INSTDIR_REG_KEY}" "DisplayVersion"
${If} $R1 == ""
${EndIf}

; Search for 1.4.0
ReadRegStr $R2 HKLM "${INSTDIR_REG_KEY}" "Uninst-pcsx2 1.4.0Directory"
${StrContains} "$2" "1.4.0" "$R2"
    StrCmp $2 "" +1
${If} $R2 == ""
${EndIf}

; Search for 1.2.1
ReadRegStr $R3 HKLM "${INSTDIR_REG_KEY}-r5875" "Uninst-pcsx2-r5875Directory"
${StrContains} "$3" "1.2.1" "$R3"
    StrCmp $3 "" +1
${If} $R3 == ""
${EndIf}

; Search for 0.9.8
ReadRegStr $R5 HKLM "${INSTDIR_REG_KEY}-r4600" "Uninst-pcsx2-r4600Directory"
${StrContains} "$5" "0.9.8" "$R5"
    StrCmp $5 "" +1
${If} $R5 == ""
${EndIf}

; If all cases return null, our work here is done.
;${If} $R1 == ""
${If} $R2 == ""
${AndIf} $R3 == ""
${AndIf} $R5 == ""
${AndIf} $R7 == ""
Goto Done
${EndIf}

; Installing another version
    MessageBox MB_ICONEXCLAMATION|MB_OKCANCEL "Another version of PCSX2 is already installed. Please back up your files and click OK to uninstall PCSX2; or Cancel to abort the setup." IDOK true IDCANCEL false
true:
Goto SetUninstPath
    false:
        Quit

SetUninstPath:
${If} $R7 != ""
    ${AndIf} $R1 != ""
    ${OrIf} $4 == "1.0.0"
    Goto ExecNormal
${Else}
        Goto +1
${EndIf}

${If} $R2 != ""
    ${AndIf} $2 == "1.4.0"
    Goto Exec1.4.0
${Else}
        Goto +1
${EndIf}

${If} $R3 != ""
    ${AndIf} $3 == "1.2.1"
    Goto Exec1.2.1
${Else}
        Goto +1
${EndIf}

${If} $R5 != ""
    ${AndIf} $5 == "0.9.8"
    Goto Exec0.9.8
${Else}
        Goto +1
${EndIf}

ExecNormal:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R7\Uninst-pcsx2.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2.exe" /S _?=$R7'
        Delete "$TEMP\Uninst-pcsx2.exe"
        Goto Done

Exec1.4.0: 
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R2\Uninst-pcsx2 1.4.0.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2 1.4.0.exe" /S _?=$R2'
        RMDir /r "$DOCUMENTS\PCSX2\inis_1.4.0"
        Delete "$TEMP\Uninst-pcsx2 1.4.0.exe"
        DeleteRegKey HKLM Software\PCSX2
        Goto Done

Exec1.2.1:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R3\Uninst-pcsx2-r5875.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r5875.exe" /S _?=$R3'
        Delete "$TEMP\Uninst-pcsx2-r5875.exe"
        Goto Done

Exec0.9.8:
    SetOutPath "$TEMP"
    CopyFiles /SILENT /FILESONLY "$R5\Uninst-pcsx2-r4600.exe" "$TEMP"
        ExecWait '"$TEMP\Uninst-pcsx2-r4600.exe" /S _?=$R5'
        Delete "$TEMP\Uninst-pcsx2-r4600.exe"
        Goto Done

Done:

FunctionEnd