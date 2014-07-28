@echo off
SET Configuration=%1
call clean_msvc.cmd
MSBuild.exe pcsx2_suite_2013.sln /m /p:BuildInParallel=true /p:CreateHardLinksForCopyLocalIfPossible=true /t:Clean,Rebuild /p:Configuration=%Configuration%
