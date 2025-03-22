@echo off
setlocal enabledelayedexpansion

echo Setting environment...
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
  echo Visual Studio 2022 not found.
  goto error
)

pushd %~dp0

cd ..\..\..\..
cd deps || goto error
set "DEPSDIR=%CD%"
cd ..
mkdir gammaray
cd gammaray || goto error
set "INSTALLDIR=%CD%"
cd ..
mkdir gammaray-build
cd gammaray-build || goto error
set "BUILDDIR=%CD%"

echo DEPSDIR=%DEPSDIR%
echo BUILDDIR=%BUILDDIR%
echo INSTALLDIR=%INSTALLDIR%

set GAMMARAY="master"

echo Downloading...
curl -L -o "GammaRay-%GAMMARAY%.tar.gz" "https://github.com/KDAB/GammaRay/archive/%GAMMARAY%.tar.gz" || goto error

rmdir /s /q "GammaRay-%GAMMARAY%"

echo Extracting...
tar -xf "GammaRay-%GAMMARAY%.tar.gz" || goto error

echo Configuring...
cmake "GammaRay-%GAMMARAY%" -B build -DCMAKE_PREFIX_PATH="%DEPSDIR%" -G Ninja -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DGAMMARAY_BUILD_DOCS=false || goto error

echo Building...
cmake --build build --parallel || goto error

echo Installing...
cmake --build build --target install || goto errorlevel

echo Copying DLLs...
xcopy /y "%DEPSDIR%\bin\*.dll" "%INSTALLDIR%\bin\"
xcopy /y /e /s "%DEPSDIR%\plugins" "%INSTALLDIR%\bin\"

echo Cleaning up...
cd ..
rd /s /q gammaray-build

echo Exiting with success.
popd
pause
exit 0

:error
echo Failed with error #%errorlevel%.
popd
pause
exit %errorlevel%
