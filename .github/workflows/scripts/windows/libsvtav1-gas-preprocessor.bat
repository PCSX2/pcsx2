@echo off
"%UNIX_TOOLS%\perl.exe" "%INSTALLDIR%\bin\gas-preprocessor.pl" -arch aarch64 -as-type armasm -- armasm64 -nologo %*
