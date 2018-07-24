$source = $args[0]
$destination = $args[1]
#if (Test-Path $destination) {Remove-Item $destination}
Compress-Archive -CompressionLevel Optimal -Force -DestinationPath "$destination" -Path "$source\*.pnach"
