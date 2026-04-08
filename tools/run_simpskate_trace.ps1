param(
    [string]$Pcsx2Exe = "",
    [string]$TraceOut = ""
)

$ErrorActionPreference = "Stop"

function Resolve-Pcsx2Exe {
    param([string]$Candidate)

    if ($Candidate -and (Test-Path -LiteralPath $Candidate)) {
        return (Resolve-Path -LiteralPath $Candidate).Path
    }

    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $rootDir = Resolve-Path (Join-Path $scriptDir "..")
    $defaultExe = Join-Path $rootDir "bin\pcsx2-qt.exe"
    if (Test-Path -LiteralPath $defaultExe) {
        return (Resolve-Path -LiteralPath $defaultExe).Path
    }

    throw "Could not find pcsx2-qt.exe. Pass -Pcsx2Exe <full path>."
}

function Resolve-TraceOut {
    param([string]$Candidate)

    if ($Candidate) {
        $dir = Split-Path -Parent $Candidate
        if ($dir -and -not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        return $Candidate
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    return (Join-Path $env:TEMP ("pcsx2_simpskate_trace_{0}.jsonl" -f $stamp))
}

$exePath = Resolve-Pcsx2Exe -Candidate $Pcsx2Exe
$tracePath = Resolve-TraceOut -Candidate $TraceOut

$env:PCSX2_SIMPTRACE = "1"
$env:PCSX2_SIMPTRACE_OUT = $tracePath

Write-Host "Launching trace-enabled PCSX2..."
Write-Host "  EXE  : $exePath"
Write-Host "  TRACE: $tracePath"
Write-Host ""
Write-Host "After you load Springfield Elementary -> Skatefest and exit PCSX2,"
Write-Host "send this JSONL trace file back for analysis."

Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath)

