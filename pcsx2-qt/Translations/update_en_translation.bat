@echo off

set QTBIN=..\..\deps\bin
set SRCDIRS=../ ../../pcsx2/

set OPTS=-tr-function-alias QT_TRANSLATE_NOOP+=TRANSLATE,QT_TRANSLATE_NOOP+=TRANSLATE_SV,QT_TRANSLATE_NOOP+=TRANSLATE_STR,QT_TRANSLATE_NOOP+=TRANSLATE_FS,QT_TRANSLATE_N_NOOP3+=TRANSLATE_FMT,QT_TRANSLATE_NOOP+=TRANSLATE_NOOP,translate+=TRANSLATE_PLURAL_STR,translate+=TRANSLATE_PLURAL_FS  -pluralonly -no-obsolete

"%QTBIN%\lupdate.exe" %SRCDIRS% %OPTS% -no-obsolete -source-language en_US -ts pcsx2-qt_en-US.ts
start %QTBIN%\linguist.exe %~dp0\pcsx2-qt_en-US.ts
