@ECHO OFF
REM PCSX2 - PS2 Emulator for PCs
REM Copyright (C) 2002-2015  PCSX2 Dev Team
REM
REM PCSX2 is free software: you can redistribute it and/or modify it under the terms
REM of the GNU Lesser General Public License as published by the Free Software Found-
REM ation, either version 3 of the License, or (at your option) any later version.
REM
REM PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
REM without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
REM PURPOSE.  See the GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License along with PCSX2.
REM If not, see <http://www.gnu.org/licenses/>.

CLS
ECHO Select your Visual Studio version:
ECHO 1. Microsoft Visual Studio 2019
ECHO Q. Exit the script.
CHOICE /C 1Q /T 10 /D 1 /M "Visual Studio version: "
IF ERRORLEVEL 2 GOTO END
IF ERRORLEVEL 1 SET "VCVARPATH=%VS160COMNTOOLS%..\..\VC\Auxiliary\Build\vcvarsall.bat"

ECHO.
ECHO Select the desired configuration:
ECHO 1. Release 32bit (default)
ECHO 2. Devel   32bit
ECHO 3. Debug   32bit
ECHO 4. Release 64bit (WIP)
ECHO 5. Devel   64bit (WIP)
ECHO 6. Debug   64bit (WIP)
ECHO Q. Exit the script.
CHOICE /C 123456Q /T 10 /D 1 /M "Configuration: "
IF ERRORLEVEL 7 GOTO END
IF ERRORLEVEL 6 SET "SELARCH=x64" && SET "SELCONF=DebugAll"
IF ERRORLEVEL 5 SET "SELARCH=x64" && SET "SELCONF=DevelAll"
IF ERRORLEVEL 4 SET "SELARCH=x64" && SET "SELCONF=ReleaseAll"
IF ERRORLEVEL 3 SET "SELARCH=x86" && SET "SELCONF=DebugAll"
IF ERRORLEVEL 2 SET "SELARCH=x86" && SET "SELCONF=DevelAll"
IF ERRORLEVEL 1 SET "SELARCH=x86" && SET "SELCONF=ReleaseAll"

IF EXIST "%VCVARPATH%" (call "%VCVARPATH%" %SELARCH%) ELSE GOTO ERRORVS
cl > NUL 2>&1
if %ERRORLEVEL% NEQ 0 GOTO ERRORVS

ECHO.
ECHO Using:
cl 2>&1 | findstr "Version"
ECHO.

SET Platform=
SET "LOGOPTIONS=/v:m /fl1 /fl2 /flp1:logfile="%~dpn0-%SELARCH%-%SELCONF%-errors.log";errorsonly /flp2:logfile="%~dpn0-%SELARCH%-%SELCONF%-warnings.log";warningsonly"
msbuild "%~dp0\buildbot.xml" /m %LOGOPTIONS% /t:%SELCONF%
GOTO END

:ERRORVS
ECHO.
ECHO The selected Visual Studio version was not found.

:END
ECHO.
ECHO Bye!
ECHO.
timeout /t 10
