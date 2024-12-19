::@echo off
:: This file GENERATES the automatic GIT revision/version tag.
:: It uses the git.exe program to create an "svnrev.h" file for whichever
:: project is being compiled, during the project's pre-build step.
::
:: The git.exe program is part of the msysgit installation.
::
:: MsysGit can be downloaded from http://msysgit.github.io/
::
:: Usage: preBuild.cmd ProjectSrcDir VspropsDir
::
::    ProjectSrcDir - $(ProjectDir)\.. - Top-level Directory of project source code.

SETLOCAL ENABLEDELAYEDEXPANSION ENABLEEXTENSIONS

IF EXIST "%ProgramFiles(x86)%\Git\bin\git.exe" SET "GITPATH=%ProgramFiles(x86)%\Git\bin"
IF EXIST "%ProgramFiles%\Git\bin\git.exe" SET "GITPATH=%ProgramFiles%\Git\bin"
IF EXIST "%ProgramW6432%\Git\bin\git.exe" SET "GITPATH=%ProgramW6432%\Git\bin"
IF DEFINED GITPATH SET "PATH=%PATH%;%GITPATH%"

git describe --tags > NUL 2>NUL
if !ERRORLEVEL! EQU 0 (
  FOR /F %%i IN ('"git describe --tags 2> NUL"') do (
    set GIT_REV=%%i
  )
) else (
  FOR /F %%i IN ('"git rev-parse --short HEAD 2> NUL"') do (
    set GIT_REV=%%i
  )
)

FOR /F "tokens=* USEBACKQ" %%i IN (`git tag --points-at HEAD`) DO (
  set GIT_TAG=%%i
)

FOR /F "tokens=* USEBACKQ" %%i IN (`git rev-parse HEAD`) DO (
  set GIT_HASH=%%i
)

FOR /F "tokens=* USEBACKQ" %%i IN (`git log -1 "--format=%%cd" "--date=local"`) DO (
  set GIT_DATE=%%i
)

SET SIGNATURELINE=// R[%GIT_REV%] H[%GIT_HASH%] T[%GIT_TAG%]
SET /P EXISTINGLINE=<"%CD%\svnrev.h"

IF "%EXISTINGLINE%"=="%SIGNATURELINE%" (
  goto cleanup
)

ECHO Updating "%CD%\svnrev.h"...
echo %SIGNATURELINE%> "%CD%\svnrev.h"

echo #define GIT_HASH "%GIT_HASH%" >> "%CD%\svnrev.h"
echo #define GIT_TAG "%GIT_TAG%" >> "%CD%\svnrev.h"
echo #define GIT_DATE "%GIT_DATE%" >> "%CD%\svnrev.h"

echo %GIT_TAG%|FINDSTR /R "^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" > NUL
if !ERRORLEVEL! EQU 0 (
  echo #define GIT_REV "%GIT_TAG%" >> "%CD%\svnrev.h"
  echo #define GIT_TAGGED_COMMIT 1 >> "%CD%\svnrev.h"
  FOR /F "tokens=1,2,3 delims=v." %%a in ("%GIT_TAG%") DO (
    echo #define GIT_TAG_HI %%a >> "%CD%\svnrev.h"
    echo #define GIT_TAG_MID %%b >> "%CD%\svnrev.h"
    echo #define GIT_TAG_LO %%c >> "%CD%\svnrev.h"
  )
) else (
  :: Local branches
  echo #define GIT_REV "%GIT_REV%" >> "%CD%\svnrev.h"
  echo #define GIT_TAGGED_COMMIT 0 >> "%CD%\svnrev.h"
  echo %GIT_REV%|FINDSTR /R "^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]-*" > NUL
  if !ERRORLEVEL! EQU 0 (
    FOR /F "tokens=1,2,3 delims=v." %%a in ("%GIT_REV%") DO (
      echo #define GIT_TAG_HI %%a >> "%CD%\svnrev.h"
      echo #define GIT_TAG_MID %%b >> "%CD%\svnrev.h"
      FOR /F "tokens=1 delims=-" %%d in ("%%c%") DO (
        echo #define GIT_TAG_LO %%d >> "%CD%\svnrev.h"
      )
    )
  ) else (
    echo #define GIT_TAG_HI 0 >> "%CD%\svnrev.h"
    echo #define GIT_TAG_MID 0 >> "%CD%\svnrev.h"
    echo #define GIT_TAG_LO 0 >> "%CD%\svnrev.h"
  )
)

:cleanup
ENDLOCAL
:: Always return an errorlevel of 0 -- this allows compilation to continue if SubWCRev failed.
exit /B 0
