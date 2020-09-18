$formattedDirs = @(
  "./pcsx2/CDVD"
)

$dirString = [string]$formattedDirs
Write-Output "Formatting the following directories - $($dirString)"

Invoke-Expression "python ./3rdparty/run-clang-format/run-clang-format.py -i -r $($dirString)"
