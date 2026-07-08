set /a errorno=1
for /F "delims=#" %%E in ('"prompt #$E# & for %%E in (1) do rem"') do set "esc=%%E"

rem https://docs.microsoft.com/visualstudio/msbuild/msbuild-command-line-reference

set "sln=lz4.sln"

@rem set "Configuration=Debug"
@rem set "Platform=Win32"

set "BIN=.\bin\!Platform!_!Configuration!"
rmdir /S /Q "!BIN!" 2>nul
echo msbuild "%sln%" /p:Configuration=!Configuration! /p:Platform=!Platform!
msbuild "%sln%"                          ^
        /nologo                          ^
        /v:minimal                       ^
        /m                               ^
        /p:Configuration=!Configuration! ^
        /p:Platform=!Platform!           ^
        /t:Clean,Build                   ^
        || goto :ERROR

if not exist "!BIN!\datagen.exe"       ( echo FAIL: "!BIN!\datagen.exe"       && goto :ERROR )
if not exist "!BIN!\frametest.exe"     ( echo FAIL: "!BIN!\frametest.exe"     && goto :ERROR )
if not exist "!BIN!\fullbench-dll.exe" ( echo FAIL: "!BIN!\fullbench-dll.exe" && goto :ERROR )
if not exist "!BIN!\fullbench.exe"     ( echo FAIL: "!BIN!\fullbench.exe"     && goto :ERROR )
if not exist "!BIN!\fuzzer.exe"        ( echo FAIL: "!BIN!\fuzzer.exe"        && goto :ERROR )
if not exist "!BIN!\liblz4.dll"        ( echo FAIL: "!BIN!\liblz4.dll"        && goto :ERROR )
if not exist "!BIN!\liblz4.lib"        ( echo FAIL: "!BIN!\liblz4.lib"        && goto :ERROR )
if not exist "!BIN!\liblz4_static.lib" ( echo FAIL: "!BIN!\liblz4_static.lib" && goto :ERROR )
if not exist "!BIN!\lz4.exe"           ( echo FAIL: "!BIN!\lz4.exe"           && goto :ERROR )

set /a errorno=0
goto :END

:ERROR

:END
exit /B %errorno%
