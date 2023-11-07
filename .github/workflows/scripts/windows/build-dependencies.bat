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

set SEVENZIP="C:\Program Files\7-Zip\7z.exe"

if defined DEBUG (
  echo DEBUG=%DEBUG%
) else (
  set DEBUG=1
)

pushd %~dp0
set "SCRIPTDIR=%CD%"
cd ..\..\..\..
mkdir deps-build
cd deps-build || goto error
set "BUILDDIR=%CD%"
cd ..
mkdir deps
cd deps || goto error
set "INSTALLDIR=%CD%"
popd

echo SCRIPTDIR=%SCRIPTDIR%
echo BUILDDIR=%BUILDDIR%
echo INSTALLDIR=%INSTALLDIR%

cd "%BUILDDIR%"

set QT=6.6.0
set QTMINOR=6.6
set SDL=SDL2-2.28.4

call :downloadfile "%SDL%.zip" "https://libsdl.org/release/%SDL%.zip" b53b9b42e731a33552d0a533316a88009b423c16a8a3a418df9ffe498c37da3d || goto error
call :downloadfile "qtbase-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtbase-everywhere-src-%QT%.zip" 0b9dafd9a42deeb55fd4b74a6caaa6871a03d0051ddb54ea4ea448f28cb90ecc || goto error
call :downloadfile "qtimageformats-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtimageformats-everywhere-src-%QT%.zip" ae31eadd81da19e1bba0d72e16d29fdce0b98e991c4dcbf6351acf09967fd175 || goto error
call :downloadfile "qtsvg-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtsvg-everywhere-src-%QT%.zip" 88b4b1a8224465cb04feed2a37823e95e959164cf824e9969648335614a4c855 || goto error
call :downloadfile "qttools-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttools-everywhere-src-%QT%.zip" 051787f12e2457425f35f31e642163f0d481cdd687b4b164ca41ddb84d5ea3a6 || goto error
call :downloadfile "qttranslations-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttranslations-everywhere-src-%QT%.zip" 5d04b33336d3d49512eedfa85b378f0805dc4d8770365db318234b125deef9dc || goto error

if %DEBUG%==1 (
  echo Building debug and release libraries...
) else (
  echo Building release libraries...
)

echo Building SDL...
rmdir /S /Q "%SDL%"
%SEVENZIP% x "%SDL%.zip" || goto error
cd "%SDL%" || goto error
if %DEBUG%==1 (
  cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -G Ninja || goto error
  cmake --build build-debug --parallel || goto error
  ninja -C build-debug install || goto error
)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

if %DEBUG%==1 (
  set QTBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES="Release;Debug" -G "Ninja Multi-Config"
) else (
  set QTBUILDSPEC=-DCMAKE_BUILD_TYPE=Release -G Ninja
)

echo Building Qt base...
rmdir /S /Q "qtbase-everywhere-src-%QT%"
%SEVENZIP% x "qtbase-everywhere-src-%QT%.zip" || goto error
cd "qtbase-everywhere-src-%QT%" || goto error
cmake -B build -DFEATURE_sql=OFF -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DINPUT_gui=yes -DINPUT_widgets=yes -DINPUT_ssl=yes -DINPUT_openssl=no -DINPUT_schannel=yes %QTBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building Qt SVG...
rmdir /S /Q "qtsvg-everywhere-src-%QT%"
%SEVENZIP% x "qtsvg-everywhere-src-%QT%.zip" || goto error
cd "qtsvg-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Image Formats...
rmdir /S /Q "qtimageformats-everywhere-src-%QT%"
%SEVENZIP% x "qtimageformats-everywhere-src-%QT%.zip" || goto error
cd "qtimageformats-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Tools...
rmdir /S /Q "qtimageformats-everywhere-src-%QT%"
%SEVENZIP% x "qttools-everywhere-src-%QT%.zip" || goto error
cd "qttools-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Translations...
rmdir /S /Q "qttranslations-everywhere-src-%QT%"
%SEVENZIP% x "qttranslations-everywhere-src-%QT%.zip" || goto error
cd "qttranslations-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Cleaning up...
cd ..
rd /S /Q deps-build

echo Exiting with success.
exit 0

:error
echo Failed with error #%errorlevel%.
pause
exit %errorlevel%

:downloadfile
if not exist "%~1" (
  echo Downloading %~1 from %~2...
  curl -L -o "%~1" "%~2" || goto error
)

rem based on https://gist.github.com/gsscoder/e22daefaff9b5d8ac16afb070f1a7971
set idx=0
for /f %%F in ('certutil -hashfile "%~1" SHA256') do (
    set "out!idx!=%%F"
    set /a idx += 1
)
set filechecksum=%out1%

if /i %~3==%filechecksum% (
    echo Validated %~1.
    exit /B 0
) else (
    echo Expected %~3 got %filechecksum%.
    exit /B 1
)
