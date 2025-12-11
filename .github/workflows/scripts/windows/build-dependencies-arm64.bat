@echo off
setlocal enabledelayedexpansion

echo Setting environment...
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
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
mkdir deps-arm64
cd deps-arm64 || goto error
set "INSTALLDIR=%CD%"
cd ..
cd deps || goto error
set "X64INSTALLDIR=%CD%"
cd ..
popd

echo SCRIPTDIR=%SCRIPTDIR%
echo BUILDDIR=%BUILDDIR%
echo INSTALLDIR=%INSTALLDIR%

cd "%BUILDDIR%"

set FREETYPE=2.14.1
set HARFBUZZ=12.2.0
set LIBJPEGTURBO=3.1.2
set LIBPNG=1653
set LIBPNGLONG=1.6.53
set SDL=SDL3-3.2.26
set QT=6.10.1
set QTMINOR=6.10
set QTAPNG=1.3.0
set LZ4=1.10.0
set WEBP=1.6.0
set ZLIB=1.3.1
set ZLIBSHORT=131
set ZSTD=1.5.7
set KDDOCKWIDGETS=2.4.0
set PLUTOVG=1.3.2
set PLUTOSVG=0.0.7

set SHADERC=2025.4
set SHADERC_GLSLANG=7a47e2531cb334982b2a2dd8513dca0a3de4373d
set SHADERC_SPIRVHEADERS=b824a462d4256d720bebb40e78b9eb8f78bbb305
set SHADERC_SPIRVTOOLS=971a7b6e8d7740035bbff089bbbf9f42951ecfd5

call :downloadfile "freetype-%FREETYPE%.tar.gz" https://sourceforge.net/projects/freetype/files/freetype2/%FREETYPE%/freetype-%FREETYPE%.tar.gz/download 174d9e53402e1bf9ec7277e22ec199ba3e55a6be2c0740cb18c0ee9850fc8c34 || goto error
call :downloadfile "harfbuzz-%HARFBUZZ%.zip" https://github.com/harfbuzz/harfbuzz/archive/refs/tags/%HARFBUZZ%.zip 31490c781bacd2ce56862555b11c51c964977c39f14f51b817dfaecf0be089fe || goto error
call :downloadfile "lpng%LIBPNG%.zip" https://download.sourceforge.net/libpng/lpng1653.zip 140566abc64bb2320cb35f1d154d1cb3eb7174a12234d33bfdffb446bdc0a1d2 || goto error
call :downloadfile "lpng%LIBPNG%-apng.patch.gz" https://download.sourceforge.net/libpng-apng/libpng-%LIBPNGLONG%-apng.patch.gz 452a1a290bd0cf18737fad0057dc17b7fdf10a73eda2d6d4f31ba04fda25ef2c || goto error
call :downloadfile "libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/%LIBJPEGTURBO%/libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" 8f0012234b464ce50890c490f18194f913a7b1f4e6a03d6644179fa0f867d0cf || goto error
call :downloadfile "libwebp-%WEBP%.tar.gz" "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-%WEBP%.tar.gz" e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564 || goto error
call :downloadfile "%SDL%.zip" "https://libsdl.org/release/%SDL%.zip" 739356eef1192fff9d641c320a8f5ef4a10506b8927def4b9ceb764c7e947369 || goto error
call :downloadfile "qtbase-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtbase-everywhere-src-%QT%.zip" c43f471a808b07fc541528410e94ce89c6745bdc1d744492e19911d35fbf7d33 || goto error
call :downloadfile "qtimageformats-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtimageformats-everywhere-src-%QT%.zip" 2d828d8c999fdd18167937c071781c22321c643b04a106c714411c2356cdb26d || goto error
call :downloadfile "qtsvg-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qtsvg-everywhere-src-%QT%.zip" ddd74a417d2397eb085d047a9b6ba52b76e748055817f728fe691f8456035d23 || goto error
call :downloadfile "qttools-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttools-everywhere-src-%QT%.zip" db8e49ed50912c3c064a4f9ada7791c09eccec5a8d53463a19608eaab17679f0 || goto error
call :downloadfile "qttranslations-everywhere-src-%QT%.zip" "https://download.qt.io/official_releases/qt/%QTMINOR%/%QT%/submodules/qttranslations-everywhere-src-%QT%.zip" 868eb651e395d48ade5932ef2c386e606e054eb5888ebe5284fbd8cb63ed935a || goto error
call :downloadfile "QtApng-%QTAPNG%.zip" "https://github.com/jurplel/QtApng/archive/refs/tags/%QTAPNG%.zip" 5176082cdd468047a7eb1ec1f106b032f57df207aa318d559b29606b00d159ac || goto error
call :downloadfile "lz4-%LZ4%.zip" "https://github.com/lz4/lz4/archive/refs/tags/v%LZ4%.zip" 3224b4c80f351f194984526ef396f6079bd6332dd9825c72ac0d7a37b3cdc565 || goto error
call :downloadfile "zlib%ZLIBSHORT%.zip" "https://github.com/madler/zlib/releases/download/v%ZLIB%/zlib%ZLIBSHORT%.zip" 72af66d44fcc14c22013b46b814d5d2514673dda3d115e64b690c1ad636e7b17 || goto error
call :downloadfile "zstd-%ZSTD%.zip" "https://github.com/facebook/zstd/archive/refs/tags/v%ZSTD%.zip" 7897bc5d620580d9b7cd3539c44b59d78f3657d33663fe97a145e07b4ebd69a4 || goto error
call :downloadfile "KDDockWidgets-%KDDOCKWIDGETS%.zip" "https://github.com/KDAB/KDDockWidgets/archive/v%KDDOCKWIDGETS%.zip" 47ddb48197872055f0adf8e90a7235f8a3b795ca1ee3a28ac2c504c673ae3806 || goto error
call :downloadfile "plutovg-%PLUTOVG%.zip" "https://github.com/sammycage/plutovg/archive/v%PLUTOVG%.zip" 4fe4e48f28aa80171b2166d45c0976ab0f21eecedb52cd4c3ef73b5afb48fac9 || goto error
call :downloadfile "plutosvg-%PLUTOSVG%.zip" "https://github.com/sammycage/plutosvg/archive/v%PLUTOSVG%.zip" 82dee2c57ad712bdd6d6d81d3e76249d89caa4b5a4214353660fd5adff12201a || goto error

call :downloadfile "shaderc-%SHADERC%.zip" "https://github.com/google/shaderc/archive/refs/tags/v%SHADERC%.zip" fab72d1a38eacea52710d18edb95dfd75db894ad869675d07a1eb26827da9b15 || goto error
call :downloadfile "shaderc-glslang-%SHADERC_GLSLANG%.zip" "https://github.com/KhronosGroup/glslang/archive/%SHADERC_GLSLANG%.zip" 4a118247386ffba9160113f146f2189ba5abe3995db357114d7112ede6bd3cd1 || goto error
call :downloadfile "shaderc-spirv-headers-%SHADERC_SPIRVHEADERS%.zip" "https://github.com/KhronosGroup/SPIRV-Headers/archive/%SHADERC_SPIRVHEADERS%.zip" 9a38cb3b14484f5038d78cd5df89404f2f5b389a6ad91f9f1df4ae71bb9490dc || goto error
call :downloadfile "shaderc-spirv-tools-%SHADERC_SPIRVTOOLS%.zip" "https://github.com/KhronosGroup/SPIRV-Tools/archive/%SHADERC_SPIRVTOOLS%.zip" a26383c836a84fab5b03aed5d98e8e27d6c0a9cdbc3b0f462ccfe0a11a3d91ea || goto error

if %DEBUG%==1 (
  echo Building debug and release libraries...
) else (
  echo Building release libraries...
)

set FORCEPDB=-DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/DEBUG" -DCMAKE_SHARED_LINKER_FLAGS_MINSIZEREL="/DEBUG"
set ARM64TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE="%SCRIPTDIR%\cmake-toolchain-windows-arm64.cmake"

echo Building Zlib...
rmdir /S /Q "zlib-%ZLIB%"
%SEVENZIP% x "zlib%ZLIBSHORT%.zip" || goto error
cd "zlib-%ZLIB%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DZLIB_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
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
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building libjpegturbo...
rmdir /S /Q "libjpeg-turbo-%LIBJPEGTURBO%"
tar -xf "libjpeg-turbo-%LIBJPEGTURBO%.tar.gz" || goto error
cd "libjpeg-turbo-%LIBJPEGTURBO%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building LZ4...
rmdir /S /Q "lz4"
%SEVENZIP% x "lz4-%LZ4%.zip" || goto error
rename "lz4-%LZ4%" "lz4" || goto error
cd "lz4" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -DCMAKE_C_FLAGS="/wd4711 /wd5045" -B build-dir -G Ninja build/cmake || goto error
cmake --build build-dir --parallel || goto error
ninja -C build-dir install || goto error
cd ..

echo Building FreeType without HarfBuzz...
rmdir /S /Q "freetype-%FREETYPE%"
tar -xf "freetype-%FREETYPE%.tar.gz" || goto error
cd "freetype-%FREETYPE%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=TRUE -DFT_REQUIRE_PNG=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_DISABLE_HARFBUZZ=TRUE -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building HarfBuzz...
rmdir /S /Q "harfbuzz-%HARFBUZZ%"
%SEVENZIP% x "-x^!harfbuzz-%HARFBUZZ%\README" "harfbuzz-%HARFBUZZ%.zip" || goto error
cd "harfbuzz-%HARFBUZZ%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DHB_BUILD_UTILS=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building FreeType with HarfBuzz...
rmdir /S /Q "freetype-%FREETYPE%"
tar -xf "freetype-%FREETYPE%.tar.gz" || goto error
cd "freetype-%FREETYPE%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=TRUE -DFT_REQUIRE_PNG=TRUE -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_REQUIRE_HARFBUZZ=TRUE -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building Zstandard...
rmdir /S /Q "zstd-%ZSTD%"
%SEVENZIP% x "-x^!zstd-%ZSTD%\tests\cli-tests\bin" "zstd-%ZSTD%.zip" || goto error
cd "zstd-%ZSTD%"
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DZSTD_BUILD_PROGRAMS=OFF -B build -G Ninja build/cmake
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building WebP...
rmdir /S /Q "libwebp-%WEBP%"
tar -xf "libwebp-%WEBP%.tar.gz" || goto error
cd "libwebp-%WEBP%" || goto error
cmake -B build %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DBUILD_SHARED_LIBS=ON -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo Building SDL...
rmdir /S /Q "%SDL%"
%SEVENZIP% x "%SDL%.zip" || goto error
cd "%SDL%" || goto error
cmake -B build %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release %FORCEPDB% -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
copy build\SDL3.pdb "%INSTALLDIR%\bin" || goto error
cd .. || goto error

if %DEBUG%==1 (
  set QTBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES="Release;Debug" -G "Ninja Multi-Config"
) else (
  set QTBUILDSPEC=-DCMAKE_BUILD_TYPE=MinSizeRel -G Ninja
)

echo Building Qt base...
rmdir /S /Q "qtbase-everywhere-src-%QT%"
%SEVENZIP% x "qtbase-everywhere-src-%QT%.zip" || goto error
cd "qtbase-everywhere-src-%QT%" || goto error
cmake -B build %ARM64TOOLCHAIN% -DFEATURE_sql=OFF -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DQT_HOST_PATH="%X64INSTALLDIR%" %FORCEPDB% -DINPUT_gui=yes -DINPUT_widgets=yes -DINPUT_ssl=yes -DINPUT_openssl=no -DINPUT_schannel=yes -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON %QTBUILDSPEC% || goto error
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
call "%INSTALLDIR%\bin\qt-configure-module.bat" .. -- %FORCEPDB% -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF || goto error
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
cmake -B build %ARM64TOOLCHAIN% -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" %FORCEPDB% %QTAPNGBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

if %DEBUG%==1 (
  set KDDOCKWIDGETSBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES="Release;Debug" -DCMAKE_CROSS_CONFIGS=all -DCMAKE_DEFAULT_BUILD_TYPE=Release -DCMAKE_DEFAULT_CONFIGS=all -G "Ninja Multi-Config"
) else (
  rem kddockwidgets slightly changes the name of the dll depending on if CMAKE_BUILD_TYPE or CMAKE_CONFIGURATION_TYPES is used
  rem The dll name being kddockwidgets-qt62.dll or kddockwidgets-qt62.dll respectively
  rem Always use CMAKE_CONFIGURATION_TYPES to give consistant naming
  set KDDOCKWIDGETSBUILDSPEC=-DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_CROSS_CONFIGS=all -DCMAKE_DEFAULT_BUILD_TYPE=Release -DCMAKE_DEFAULT_CONFIGS=Release -G "Ninja Multi-Config"
)

echo "Building KDDockWidgets..."
rmdir /S /Q "KDDockWidgets-%KDDOCKWIDGETS%"
%SEVENZIP% x "KDDockWidgets-%KDDOCKWIDGETS%.zip" || goto error
cd "KDDockWidgets-%KDDOCKWIDGETS%" || goto error
cmake -B build %ARM64TOOLCHAIN% -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DKDDockWidgets_QT6=true -DKDDockWidgets_EXAMPLES=false -DKDDockWidgets_FRONTENDS=qtwidgets %KDDOCKWIDGETSBUILDSPEC% || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo "Building PlutoVG..."
rmdir /S /Q "plutovg-%PLUTOVG%"
%SEVENZIP% x "plutovg-%PLUTOVG%.zip" || goto error
cd "plutovg-%PLUTOVG%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DPLUTOVG_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
cmake --build build --parallel || goto error
ninja -C build install || goto error
cd .. || goto error

echo "Building PlutoSVG..."
rmdir /S /Q "plutosvg-%PLUTOSVG%"
%SEVENZIP% x "plutosvg-%PLUTOSVG%.zip" || goto error
cd "plutosvg-%PLUTOSVG%" || goto error
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DBUILD_SHARED_LIBS=ON -DPLUTOSVG_ENABLE_FREETYPE=ON -DPLUTOSVG_BUILD_EXAMPLES=OFF -B build -G Ninja || goto error
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
cmake %ARM64TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%INSTALLDIR%" -DCMAKE_INSTALL_PREFIX="%INSTALLDIR%" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSHADERC_ENABLE_SHARED_CRT=ON -B build -G Ninja || goto error
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
