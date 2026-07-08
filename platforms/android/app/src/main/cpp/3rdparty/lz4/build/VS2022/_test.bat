set /a errorno=1
for /F "delims=#" %%E in ('"prompt #$E# & for %%E in (1) do rem"') do set "esc=%%E"

@rem set "Configuration=Debug"
@rem set "Platform=Win32"

set "BIN=.\bin\!Platform!_!Configuration!"
set "TEST_FILES=..\..\tests\COPYING"

echo !BIN!\lz4 -h
     !BIN!\lz4 -h	|| goto :ERROR

echo !BIN!\lz4 -i1b
     !BIN!\lz4 -i1b	|| goto :ERROR

echo !BIN!\lz4 -i1b5
     !BIN!\lz4 -i1b5	|| goto :ERROR

echo !BIN!\lz4 -i1b10
     !BIN!\lz4 -i1b10	|| goto :ERROR

echo !BIN!\lz4 -i1b15
     !BIN!\lz4 -i1b15	|| goto :ERROR
     
echo fullbench
!BIN!\fullbench.exe --no-prompt -i1 %TEST_FILES%	|| goto :ERROR

echo fuzzer
!BIN!\fuzzer.exe -v -T30s				|| goto :ERROR


set /a errorno=0
goto :END

:ERROR

:END
exit /B %errorno%
