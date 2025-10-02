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
set PATCH="C:\Program Files\Git\usr\bin\patch.exe"

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

set "PATH=%PATH%;%INSTALLDIR%\bin"

cd "%BUILDDIR%"

set FREETYPE=2.14.1
set HARFBUZZ=12.0.0
set LIBJPEGTURBO=3.1.2
set LIBPNG=1650
set SDL=SDL3-3.2.24
set LIBPNGLONG=1.6.50
set QT=6.9.2
set QTMINOR=6.9
set QTAPNG=1.3.0
set LZ4=1.10.0
set WEBP=1.6.0
set ZLIB=1.3.1
set ZLIBSHORT=131
set ZSTD=1.5.7
set KDDOCKWIDGETS=2.3.0
set PLUTOVG=1.3.1
set PLUTOSVG=0.0.7

set SHADERC=2025.3
set SHADERC_GLSLANG=efd24d75bcbc55620e759f6bf42c45a32abac5f8
set SHADERC_SPIRVHEADERS=2a611a970fdbc41ac2e3e328802aed9985352dca
set SHADERC_SPIRVTOOLS=33e02568181e3312f49a3cf33df470bf96ef293a

call :downloadfile "freetype-%FREETYPE%.tar.gz" https://sourceforge.net/projects/freetype/files/freetype2/%FREETYPE%/freetype-%FREETYPE%.tar.gz/download 174d9e53402e1bf9ec7277e22ec199ba3e55a6be2c0740cb18c0ee9850fc8c34 || goto error
call :downloadfile "harfbuzz-%HARFBUZZ%.zip" https://github.com/harfbuzz/harfbuzz/archive/refs/tags/%HARFBUZZ%.zip 0f3e036294974736982d8ec00f0bed763cd9f75ab9eacf8effe413af5d78ef06 || goto error
call :downloadfile "lpng%LIBPNG%.zip" https://download.sourceforge.net/libpng/lpng1650.zip 4be6938313b08d5921f9dede13f2789b653c96f4f8595d92ff3f09c9320e51c7 || goto error
call :downloadfile "lpng%LIBPNG%-apng.patch.gz" https://download.sourceforge.net/libpng-apng/libpng-%LIBPNGLONG%-apng.patch.gz 687ddc0c7cb128a3ea58e159b5129252537c27ede0c32a93f11f03127f0c0165 || goto error
call :downloadfile "libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/%LIBJPEGTURBO%/libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" 8f0012234b464ce50890c490f18194f913a7b1f4e6a03d6644179fa0f867d0cf || goto error
call :downloadfile "libwebp-%WEBP%.tar.gz" "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-%WEBP%.tar.gz" e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564 || goto error
call :downloadfile "%SDL%.zip" "https://libsdl.org/release/%SDL%.zip" ca7fe2ca54a97e047f5eff236e62ae87546e862f509f0a62fc6e564ded3c6a95 || goto error
call :downloadfile "qtbase-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtbase-everywhere-src-%QT%.zip" 97d59c78e40b4ddd018738d285a12afc320b57f8265a3f760353739a3619ccdb || goto error
call :downloadfile "qtimageformats-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtimageformats-everywhere-src-%QT%.zip" f2fc6ff382c6f3af79493d0709dbd64847d0356313518f094f9096315f2fdb30 || goto error
call :downloadfile "qtsvg-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtsvg-everywhere-src-%QT%.zip" af80bb671ea0f66c0036ce7041a56b0e550fc94fb88d2c77b5b6a3e33e42139b || goto error
call :downloadfile "qttools-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttools-everywhere-src-%QT%.zip" d2f4c7a4a12630e879702353f944f96a5d8e764771b5a5f04163334ad61b39db || goto error
call :downloadfile "qttranslations-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttranslations-everywhere-src-%QT%.zip" 3e168d1b081ee3a2175fe1bd97ad03bb40fe7ce38a37e99923a19f0e7ec4d81c || goto error
call :downloadfile "QtApng-%QTAPNG%.zip" "https://github.com/jurplel/QtApng/archive/refs/tags/%QTAPNG%.zip" 5176082cdd468047a7eb1ec1f106b032f57df207aa318d559b29606b00d159ac || goto error
call :downloadfile "lz4-%LZ4%.zip" "https://github.com/lz4/lz4/archive/refs/tags/v%LZ4%.zip" 3224b4c80f351f194984526ef396f6079bd6332dd9825c72ac0d7a37b3cdc565 || goto error
call :downloadfile "zlib%ZLIBSHORT%.zip" "https://zlib.net/zlib%ZLIBSHORT%.zip" 72af66d44fcc14c22013b46b814d5d2514673dda3d115e64b690c1ad636e7b17 || goto error
call :downloadfile "zstd-%ZSTD%.zip" "https://github.com/facebook/zstd/archive/refs/tags/v%ZSTD%.zip" 7897bc5d620580d9b7cd3539c44b59d78f3657d33663fe97a145e07b4ebd69a4 || goto error
call :downloadfile "KDDockWidgets-%KDDOCKWIDGETS%.zip" "https://github.com/KDAB/KDDockWidgets/archive/v%KDDOCKWIDGETS%.zip" d2b9592ebe5d053ac97a0213ea35139866d8d5e0a1d84b7d3fb581db7f0b01c6 || goto error
call :downloadfile "plutovg-%PLUTOVG%.zip" "https://github.com/sammycage/plutovg/archive/v%PLUTOVG%.zip" 615184f756d91ce416f2cf883bb67fd4262651417c2e40c4d681c8641a48263e || goto error
call :downloadfile "plutosvg-%PLUTOSVG%.zip" "https://github.com/sammycage/plutosvg/archive/v%PLUTOSVG%.zip" 82dee2c57ad712bdd6d6d81d3e76249d89caa4b5a4214353660fd5adff12201a || goto error

call :downloadfile "shaderc-%SHADERC%.zip" "https://github.com/google/shaderc/archive/refs/tags/v%SHADERC%.zip" 77d2425458bca62c16b1ed49ed02de4c4114a113781bd94c1961b273bdca00fb || goto error
call :downloadfile "shaderc-glslang-%SHADERC_GLSLANG%.zip" "https://github.com/KhronosGroup/glslang/archive/%SHADERC_GLSLANG%.zip" ebd389bf79c17d79d999b3e9756359945020bbef799537aa96d8900464c373c5 || goto error
call :downloadfile "shaderc-spirv-headers-%SHADERC_SPIRVHEADERS%.zip" "https://github.com/KhronosGroup/SPIRV-Headers/archive/%SHADERC_SPIRVHEADERS%.zip" 6b954cb358a43915a54b6ca7a27db11b15c4f6e9ec547ab4cad71857354692bc || goto error
call :downloadfile "shaderc-spirv-tools-%SHADERC_SPIRVTOOLS%.zip" "https://github.com/KhronosGroup/SPIRV-Tools/archive/%SHADERC_SPIRVTOOLS%.zip" 00c4fa1a26de21c7c8db6947e06094a338e7d4edf972bc70d30afea9315373f2 || goto error

if %DEBUG%==1 (
  echo Building debug and release libraries...
) else (
  echo Building release libraries...
)

set FORCEPDB=-DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/DEBUG" -DCMAKE_MODULE_LINKER_FLAGS_RELEASE="/DEBUG"

echo Building Zlib...
rmdir /S /Q "zlib-%ZLIB%"
%SEVENZIP% x "zlib%ZLIBSHORT%.zip" || goto error
cd "zlib-%ZLIB%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DZLIB_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building libpng...
rmdir /S /Q "lpng%LIBPNG%"
%SEVENZIP% x "lpng%LIBPNG%.zip" || goto error
rem apng not in released libpng yet
%SEVENZIP% x "lpng%LIBPNG%-apng.patch.gz" -aoa || goto error
cd "lpng%LIBPNG%" || goto error
%PATCH% -p1 < "../libpng-%LIBPNGLONG%-apng.patch" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building libjpegturbo...
rmdir /S /Q "libjpeg-turbo-%LIBJPEGTURBO%"
tar -xf "libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" || goto error
cd "libjpeg-turbo-%LIBJPEGTURBO%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building LZ4...
rmdir /S /Q "lz4"
%SEVENZIP% x "lz4-%LZ4%.zip" || goto error
rename "lz4-%LZ4%" "lz4" || goto error
cd "lz4" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -DCMAKE_C_FLAGS="/wd4711 /wd5045" -B build-dir -G Ninja build/cmake || goto error
cmake --build build-dir --parallel || goto error
ninja -C build-dir install || goto error
cd ..

echo Building FreeType without HarfBuzz...
rmdir /S /Q "freetype-%FREETYPE%"
tar -xf "freetype-%FREETYPE%.tar.gz" || goto error
cd "freetype-%FREETYPE%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=TRUE -DFT_REQUIRE_PNG=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_DISABLE_HARFBUZZ=TRUE -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building HarfBuzz...
rmdir /S /Q "harfbuzz-%HARFBUZZ%"
%SEVENZIP% x "-x^!harfbuzz-%HARFBUZZ%\README" "harfbuzz-%HARFBUZZ%.zip" || goto error
cd "harfbuzz-%HARFBUZZ%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DHB_BUILD_UTILS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building FreeType with HarfBuzz...
rmdir /S /Q "freetype-%FREETYPE%"
tar -xf "freetype-%FREETYPE%.tar.gz" || goto error
cd "freetype-%FREETYPE%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=TRUE -DFT_REQUIRE_PNG=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_REQUIRE_HARFBUZZ=TRUE -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building Zstandard...
rmdir /S /Q "zstd-%ZSTD%"
%SEVENZIP% x "-x^!zstd-%ZSTD%\tests\cli-tests\bin" "zstd-%ZSTD%.zip" || goto error
cd "zstd-%ZSTD%"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DZSTD_BUILD_PROGRAMS=OFF -B build -G Ninja build/cmake
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building WebP...
rmdir /S /Q "libwebp-%WEBP%"
tar -xf "libwebp-%WEBP%.tar.gz" || goto error
cd "libwebp-%WEBP%" || goto error
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DBUILD_SHARED_LIBS=ON -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building SDL...
rmdir /S /Q "%SDL%"
%SEVENZIP% x "%SDL%.zip" || goto error
cd "%SDL%" || goto error
cmake -B build -DCMAKE_BUILD_TYPE=Release %FORCEPDB% -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
copy build\SDL3.pdb "%INSTALLDIR%\bin" || goto error
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

rem Disable the PCRE2 JIT, it doesn't properly verify AVX2 support.
%PATCH% -p1 < "%SCRIPTDIR%\qtbase-disable-pcre2-jit.patch" || goto error

cmake -B build -DFEATURE_sql=OFF -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" %FORCEPDB% -DINPUT_gui=yes -DINPUT_widgets=yes -DINPUT_ssl=yes -DINPUT_openssl=no -DINPUT_schannel=yes -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON %QTBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building Qt SVG...
rmdir /S /Q "qtsvg-everywhere-src-%QT%"
%SEVENZIP% x "qtsvg-everywhere-src-%QT%.zip" || goto error
cd "qtsvg-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- %FORCEPDB% -DCMAKE_PREFIX_PATH="%INSTALLDIR%" || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Image Formats...
rmdir /S /Q "qtimageformats-everywhere-src-%QT%"
%SEVENZIP% x "qtimageformats-everywhere-src-%QT%.zip" || goto error
cd "qtimageformats-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- %FORCEPDB% -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DFEATURE_system_webp=ON || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Tools...
rmdir /S /Q "qtimageformats-everywhere-src-%QT%"
%SEVENZIP% x "qttools-everywhere-src-%QT%.zip" || goto error
cd "qttools-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- %FORCEPDB% -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=ON -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

echo Building Qt Translations...
rmdir /S /Q "qttranslations-everywhere-src-%QT%"
%SEVENZIP% x "qttranslations-everywhere-src-%QT%.zip" || goto error
cd "qttranslations-everywhere-src-%QT%" || goto error
mkdir build || goto error
cd build || goto error
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- %FORCEPDB% || goto error
cmake --build . --parallel || goto error
ninja install || goto error
cd ..\.. || goto error

if %DEBUG%==1 (
  set QTAPNGBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES="Release;Debug" -DCMAKE_CROSS_CONFIGS=all -DCMAKE_DEFAULT_BUILD_TYPE=Release -DCMAKE_DEFAULT_CONFIGS=all -G "Ninja Multi-Config"
) else (
  set QTAPNGBUILDSPEC=-DCMAKE_BUILD_TYPE=Release -G Ninja
)

echo Building Qt APNG...
rmdir /S /Q "QtApng-%QTAPNG%"
%SEVENZIP% x "QtApng-%QTAPNG%.zip" || goto error
cd "QtApng-%QTAPNG%" || goto error
%PATCH% -p1 < "%SCRIPTDIR%\..\common\qtapng-cmake.patch" || goto error
cmake -B build -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" %FORCEPDB% %QTAPNGBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

if %DEBUG%==1 (
  set KDDOCKWIDGETSBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES="Release;Debug" -DCMAKE_CROSS_CONFIGS=all -DCMAKE_DEFAULT_BUILD_TYPE=Release -DCMAKE_DEFAULT_CONFIGS=all -G "Ninja Multi-Config"
) else (
  rem kddockwidgets slightly changes the name of the dll depending on if CMAKE_BUILD_TYPE or CMAKE_CONFIGURATION_TYPES is used
  rem The dll name being kddockwidgets-qt62.dll or kddockwidgets-qt6.dll respectively
  rem Always use CMAKE_CONFIGURATION_TYPES to give consistent naming
  set KDDOCKWIDGETSBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_CROSS_CONFIGS=all -DCMAKE_DEFAULT_BUILD_TYPE=Release -DCMAKE_DEFAULT_CONFIGS=Release -G "Ninja Multi-Config"
)

echo "Building KDDockWidgets..."
rmdir /S /Q "KDDockWidgets-%KDDOCKWIDGETS%"
%SEVENZIP% x "KDDockWidgets-%KDDOCKWIDGETS%.zip" || goto error
cd "KDDockWidgets-%KDDOCKWIDGETS%" || goto error
cmake -B build -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DKDDockWidgets_QT6=true -DKDDockWidgets_EXAMPLES=false -DKDDockWidgets_FRONTENDS=qtwidgets %KDDOCKWIDGETSBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo "Building PlutoVG..."
rmdir /S /Q "plutovg-%PLUTOVG%"
%SEVENZIP% x "plutovg-%PLUTOVG%.zip" || goto error
cd "plutovg-%PLUTOVG%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DPLUTOVG_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo "Building PlutoSVG..."
rmdir /S /Q "plutosvg-%PLUTOSVG%"
%SEVENZIP% x "plutosvg-%PLUTOSVG%.zip" || goto error
cd "plutosvg-%PLUTOSVG%" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DPLUTOSVG_ENABLE_FREETYPE=ON -DPLUTOSVG_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building shaderc...
rmdir /S /Q "shaderc-%SHADERC%"
%SEVENZIP% x "shaderc-%SHADERC%.zip" || goto error
cd "shaderc-%SHADERC%" || goto error
cd third_party || goto error
%SEVENZIP% x "..\..\shaderc-glslang-%SHADERC_GLSLANG%.zip" || goto error
rename "glslang-%SHADERC_GLSLANG%" "glslang" || goto error
%SEVENZIP% x "..\..\shaderc-spirv-headers-%SHADERC_SPIRVHEADERS%.zip" || goto error
rename "SPIRV-Headers-%SHADERC_SPIRVHEADERS%" "spirv-headers" || goto error
%SEVENZIP% x "..\..\shaderc-spirv-tools-%SHADERC_SPIRVTOOLS%.zip" || goto error
rename "SPIRV-Tools-%SHADERC_SPIRVTOOLS%" "spirv-tools" || goto error
cd .. || goto error
%PATCH% -p1 < "%SCRIPTDIR%\..\common\shaderc-changes.patch" || goto error
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSHADERC_ENABLE_SHARED_CRT=ON -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

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
