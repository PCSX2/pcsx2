@echo off

set QTBIN=..\..\3rdparty\qt\6.5.0\msvc2022_64\bin
set SRCDIRS=../ ../../pcsx2/

set OPTS=-tr-function-alias QT_TRANSLATE_NOOP+=TRANSLATE,QT_TRANSLATE_NOOP+=TRANSLATE_SV,QT_TRANSLATE_NOOP+=TRANSLATE_STR,QT_TRANSLATE_N_NOOP3+=TRANSLATE_FMT,QT_TRANSLATE_NOOP+=TRANSLATE_NOOP

"%QTBIN%\lupdate.exe" %SRCDIRS% %OPTS% -no-obsolete -source-language en -ts pcsx2-qt_en.ts

echo.
echo ******************************************************************************************************************
echo *                                                  PLEASE READ                                                   *
echo ******************************************************************************************************************
echo.
echo Make sure you have deleted the build (x64) directory from pcsx2-qt, otherwise you will have polluted the .ts file.
echo If you did not, then you should reset/checkout the ts file, delete the build directory, and run this script again.
echo.
echo.
pause
