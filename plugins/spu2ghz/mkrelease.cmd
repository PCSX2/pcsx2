@echo off

echo Configuring...
set RAREXE="C:\Program Files (x86)\WinRAR\rar.exe"

echo Checking files...

if not exist %RAREXE% goto error_1
if not exist .\bin\spu2ghz.dll goto error_2
if not exist .\changelog.txt goto error_3


echo Preparing files...
copy changelog.txt .\bin\
cd bin

echo Packing...
if exist spu2ghz.rar del spu2ghz.rar
%RAREXE% a -m5 spu2ghz.rar spu2ghz.dll changelog.txt >null

if errorlevel 1 (
  echo ERROR: %rarexe% returned an error code.
  exit 4
)

echo Finished.
exit

:error_1
echo ERROR: Cannot find the rar executable. Change %0 to point it to rar.exe.
exit /B 1

:error_2
echo ERROR: Cannot find spu2ghz.dll
exit /B 2

:error_3
echo ERROR: Cannot find changelog.txt
exit /B 3