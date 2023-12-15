@echo off

set QTBIN=..\..\deps\bin
set SRCDIRS=../ ../../pcsx2/

set OPTS=-tr-function-alias QT_TRANSLATE_NOOP+=TRANSLATE,QT_TRANSLATE_NOOP+=TRANSLATE_SV,QT_TRANSLATE_NOOP+=TRANSLATE_STR,QT_TRANSLATE_NOOP+=TRANSLATE_FS,QT_TRANSLATE_N_NOOP3+=TRANSLATE_FMT,QT_TRANSLATE_NOOP+=TRANSLATE_NOOP

"%QTBIN%\lupdate.exe" %SRCDIRS% %OPTS% -no-obsolete -source-language en -ts pcsx2-qt_en.ts

pause
