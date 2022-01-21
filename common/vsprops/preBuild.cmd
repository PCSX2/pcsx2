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

SETLOCAL ENABLEEXTENSIONS

IF EXIST "%ProgramFiles(x86)%\Git\bin\git.exe" SET "GITPATH=%ProgramFiles(x86)%\Git\bin"
IF EXIST "%ProgramFiles%\Git\bin\git.exe" SET "GITPATH=%ProgramFiles%\Git\bin"
IF EXIST "%ProgramW6432%\Git\bin\git.exe" SET "GITPATH=%ProgramW6432%\Git\bin"
IF DEFINED GITPATH SET "PATH=%PATH%;%GITPATH%"

FOR /F "tokens=1-2" %%i IN ('"git show -s --format=%%%ci HEAD 2> NUL"') do (
  set REV3=%%i%%j
)

FOR /F %%i IN ('"git describe 2> NUL"') do (
  set GIT_REV=%%i
)

FOR /F "tokens=* USEBACKQ" %%i IN (`git tag --points-at HEAD`) DO (
  set GIT_TAG=%%i
)

set REV2=%REV3: =%
set REV1=%REV2:-=%
set REV=%REV1::=%

git show -s > NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo Automatic version detection unavailable.
  echo If you want to have the version string print correctly,
  echo make sure your Git.exe is in the default installation directory,
  echo or in your PATH.
  echo You can safely ignore this message - a dummy string will be printed.

  echo #define SVN_REV_UNKNOWN > "%CD%\svnrev.h"
  echo #define SVN_REV 0ll >> "%CD%\svnrev.h"
  echo #define GIT_REV "" >> "%CD%\svnrev.h"
  echo #define GIT_TAG "" >> "%CD%\svnrev.h"
  echo #define GIT_TAGGED_COMMIT 0 >> "%CD%\svnrev.h"
) else (
  :: Support New Tagged Release Model
  if [%GIT_TAG%] NEQ [] (
    echo Detected that the current commit is tagged, using that!
    echo #define SVN_REV_UNKNOWN > "%CD%\svnrev.h"
    echo #define SVN_REV 0ll >> "%CD%\svnrev.h"
    echo #define GIT_REV "" >> "%CD%\svnrev.h"
    echo #define GIT_TAG "%GIT_TAG%" >> "%CD%\svnrev.h"
    echo #define GIT_TAGGED_COMMIT 1 >> "%CD%\svnrev.h"
  ) else (
    echo #define SVN_REV %REV%ll > "%CD%\svnrev.h"
    echo #define GIT_REV "%GIT_REV%" >> "%CD%\svnrev.h"
    echo #define GIT_TAG "" >> "%CD%\svnrev.h"
    echo #define GIT_TAGGED_COMMIT 0 >> "%CD%\svnrev.h"
  )
)

ENDLOCAL
:: Always return an errorlevel of 0 -- this allows compilation to continue if SubWCRev failed.
exit /B 0
