@setlocal enabledelayedexpansion
@echo off
set /a errorno=1
for /F "delims=#" %%E in ('"prompt #$E# & for %%E in (1) do rem"') do set "esc=%%E"

call _setup.bat || goto :ERROR

set "Configuration=Release"
set "Platform=x64"

call _build.bat || goto :ERROR
call _test.bat  || goto :ERROR


echo Build Status -%esc%[92m SUCCEEDED %esc%[0m
set /a errorno=0
goto :END


:ERROR
echo Abort by error.
echo Build Status -%esc%[91m ERROR %esc%[0m


:END
exit /B %errorno%
