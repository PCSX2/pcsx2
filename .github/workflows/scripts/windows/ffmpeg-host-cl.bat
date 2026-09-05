@echo off
call "%VSINSTPATH%\VC\Auxiliary\Build\vcvars64.bat"
cl %*
