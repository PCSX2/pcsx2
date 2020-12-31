@echo off
msbuild "buildbot.xml" /m /v:n /t:ReleaseAll /p:Platform=Win32

