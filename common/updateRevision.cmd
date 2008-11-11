@echo off
%1 %2 %3\svnrev_template.h %2\svnrev.h
if not ERRORLEVEL 0 (
    echo Automatic revision update unavailable, using generic template instead.
    echo You can safely ignore this message - see svnrev.h for details.
    copy /Y %3\svnrev_unknown.h %2\svnrev.h
)
set ERRORLEVEL=0
