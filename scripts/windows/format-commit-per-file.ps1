# Run Clang Formatting on Provided Directory
$dir = $args[0]
Write-Output "Formatting - $($dir)"
Invoke-Expression "python .\3rdparty\run-clang-format\run-clang-format.py -i -r $($dir)"

# Create a commit for each file with the message format - "format: clang-formatted'd 'file-name-here'"
$unstagedFiles = Invoke-Expression "git diff --name-only"

foreach ($file in $unstagedFiles) {
  Invoke-Expression "git add $($file)"
  Invoke-Expression "git commit -m `"format: clang-format'ed '$($file)'`""
}
