REM PCSX2 - PS2 Emulator for PCs
REM Copyright (C) 2002-2011  PCSX2 Dev Team
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
REM
REM Ensure use Visual Studio Developer Command Prompt
REM Example, into Solution Folder (pcsx2\)
REM	build.cmd "Debug" or
REM	build.cmd "Release" or
REM	build.cmd "Release AVX"

@echo off
SET Configuration=%1
call clean_msvc.cmd
MSBuild.exe pcsx2_suite_2013.sln /m /p:BuildInParallel=true /p:CreateHardLinksForCopyLocalIfPossible=true /t:Clean,Rebuild /p:Configuration=%Configuration%
