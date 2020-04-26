# ----
# Runs all *.png files in the current directory
# through tools/bin/bin2cpp.exe
# which creates a header file with the image data to be used
# with wxWidgets (wxBitmap / wxImage specifically)
# ----

$files = Get-ChildItem -Path $PSScriptRoot -Filter *.png

foreach ($f in $files ) {
  ..\..\..\..\tools\bin\bin2cpp.exe $f.Name
}