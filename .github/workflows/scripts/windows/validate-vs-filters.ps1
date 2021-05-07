$filterFiles = Get-ChildItem $PWD -name -recurse *.vcxproj.filters

$failed = $FALSE
foreach ($file in $filterFiles) {
  # Skip 3rdparty files
  if ($file -NotMatch "^3rdparty") {
    $expression  = "python -c `"import sys, xml.dom.minidom as d; d.parse(sys.argv[1])`" $($file)"
    $expression += ';$LastExitCode'
    $exitCode = Invoke-Expression $expression
    if($exitCode -ne 0){
      Write-Host -foregroundColor red "$($file) - Invalid VS filters file.  Likely missing tags"
      $failed = $TRUE
    }
  }
}

if ($failed) {
  exit 1
}
