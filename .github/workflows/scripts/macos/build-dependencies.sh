#!/bin/bash

set -e

if [ "$#" -ne 1 ]; then
    echo "Syntax: $0 <output directory>"
    exit 1
fi

# The bundled ffmpeg has a lot of things disabled to reduce code size.
# Users may want to use system ffmpeg for additional features
: ${BUILD_FFMPEG:=1}

export MACOSX_DEPLOYMENT_TARGET=11.0

NPROCS="$(getconf _NPROCESSORS_ONLN)"
SCRIPTDIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))
INSTALLDIR="$1"
if [ "${INSTALLDIR:0:1}" != "/" ]; then
	INSTALLDIR="$PWD/$INSTALLDIR"
fi

FREETYPE=2.13.2
HARFBUZZ=8.3.1
SDL=SDL2-2.30.3
ZSTD=1.5.5
LZ4=b8fd2d15309dd4e605070bd4486e26b6ef814e29
LIBPNG=1.6.43
LIBJPEG=9f
LIBWEBP=1.3.2
FFMPEG=6.0
MOLTENVK=1.2.8
QT=6.7.0

SHADERC=2024.0
SHADERC_GLSLANG=d73712b8f6c9047b09e99614e20d456d5ada2390
SHADERC_SPIRVHEADERS=8b246ff75c6615ba4532fe4fde20f1be090c3764
SHADERC_SPIRVTOOLS=04896c462d9f3f504c99a4698605b6524af813c1

mkdir -p deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include $CXXFLAGS"
CMAKE_COMMON=(
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_SHARED_LINKER_FLAGS="-dead_strip -dead_strip_dylibs"
	-DCMAKE_PREFIX_PATH="$INSTALLDIR"
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR"
	-DCMAKE_OSX_ARCHITECTURES="x86_64"
	-DCMAKE_INSTALL_NAME_DIR='$<INSTALL_PREFIX>/lib'
)

cat > SHASUMS <<EOF
12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d  freetype-$FREETYPE.tar.xz
19a54fe9596f7a47c502549fce8e8a10978c697203774008cc173f8360b19a9a  harfbuzz-$HARFBUZZ.tar.gz
820440072f8f5b50188c1dae104f2ad25984de268785be40c41a099a510f0aec  $SDL.tar.gz
9c4396cc829cfae319a6e2615202e82aad41372073482fce286fac78646d3ee4  zstd-$ZSTD.tar.gz
0728800155f3ed0a0c87e03addbd30ecbe374f7b080678bbca1506051d50dec3  $LZ4.tar.gz
6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c  libpng-$LIBPNG.tar.xz
2a499607df669e40258e53d0ade8035ba4ec0175244869d1025d460562aa09b4  libwebp-$LIBWEBP.tar.gz
04705c110cb2469caa79fb71fba3d7bf834914706e9641a4589485c1f832565b  jpegsrc.v$LIBJPEG.tar.gz
57be87c22d9b49c112b6d24bc67d42508660e6b718b3db89c44e47e289137082  ffmpeg-$FFMPEG.tar.xz
85beaf8abfcc54d9da0ff0257ae311abd9e7aa96e53da37e1c37d6bc04ac83cd  v$MOLTENVK.tar.gz
11b2e29e2e52fb0e3b453ea13bbe51a10fdff36e1c192d8868c5a40233b8b254  qtbase-everywhere-src-$QT.tar.xz
516ce07ec8dd5a11c59816fe33ddb71d4f691d0ebbc1798ac338f23b86c029a7  qtimageformats-everywhere-src-$QT.tar.xz
1518f40e08ff5e6153a6e26e5b95b033413ac143b70795dc1317e7f73ebf922d  qtsvg-everywhere-src-$QT.tar.xz
c8da6b239e82fe1e23465cbf0936c0da5a334438d3fb433e19c503cbb1abee7b  qttools-everywhere-src-$QT.tar.xz
26fc8047062ca4bacd1bd953be86fd39c6e0a5f5e9920c72ba9d40876cea4b56  qttranslations-everywhere-src-$QT.tar.xz
c761044e4e204be8e0b9a2d7494f08671ca35b92c4c791c7049594ca7514197f  shaderc-$SHADERC.tar.gz
d27f7359156a92749f8fd4681d1d518c736864213c431cf8144ecc2fb6689a2d  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
cfeed5f9a97d12a9761a26e7f5bd10fedb1a8ce92033075151ae3bc7206fc229  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
c0d01e758a543b3a358cb97af02c6817ebd3f5ff13a2edf9fb220646a3d67999  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
EOF

curl -L \
	-O "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE.tar.xz" \
	-o "harfbuzz-$HARFBUZZ.tar.gz" "https://github.com/harfbuzz/harfbuzz/archive/refs/tags/$HARFBUZZ.tar.gz" \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
	-O "https://github.com/lz4/lz4/archive/$LZ4.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
	-O "https://ijg.org/files/jpegsrc.v$LIBJPEG.tar.gz" \
	-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
	-O "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" \
	-O "https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v$MOLTENVK.tar.gz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \
	-o "shaderc-$SHADERC.tar.gz" "https://github.com/google/shaderc/archive/refs/tags/v$SHADERC.tar.gz" \
	-o "shaderc-glslang-$SHADERC_GLSLANG.tar.gz" "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG.tar.gz" \
	-o "shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS.tar.gz" \
	-o "shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS.tar.gz"

shasum -a 256 --check SHASUMS

echo "Installing SDL..."
rm -fr "$SDL"
tar xf "$SDL.tar.gz"
cd "$SDL"
cmake -B build "${CMAKE_COMMON[@]}" -DSDL_X11=OFF -DBUILD_SHARED_LIBS=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

if [ "$BUILD_FFMPEG" -ne 0 ]; then
	echo "Installing FFmpeg..."
	rm -fr "ffmpeg-$FFMPEG"
	tar xf "ffmpeg-$FFMPEG.tar.xz"
	cd "ffmpeg-$FFMPEG"
	LDFLAGS="-dead_strip $LDFLAGS" CFLAGS="-Os $CFLAGS" CXXFLAGS="-Os $CXXFLAGS" \
		./configure --prefix="$INSTALLDIR" \
		--enable-cross-compile --arch=x86_64 --cc='clang -arch x86_64' --cxx='clang++ -arch x86_64' \
		--disable-all --disable-autodetect --disable-static --enable-shared \
		--enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
		--enable-audiotoolbox --enable-videotoolbox \
		--enable-encoder=ffv1,qtrle,pcm_s16be,pcm_s16le,*_at,*_videotoolbox \
		--enable-muxer=avi,matroska,mov,mp3,mp4,wav \
		--enable-protocol=file
	make "-j$NPROCS"
	make install
	cd ..
fi

echo "Installing Zstd..."
rm -fr "zstd-$ZSTD"
tar xf "zstd-$ZSTD.tar.gz"
cd "zstd-$ZSTD"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DZSTD_BUILD_PROGRAMS=OFF -B build-dir build/cmake
make -C build-dir "-j$NPROCS"
make -C build-dir install
cd ..

echo "Installing LZ4..."
rm -fr "lz4-$LZ4"
tar xf "$LZ4.tar.gz"
cd "lz4-$LZ4"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -B build-dir build/cmake
make -C build-dir "-j$NPROCS"
make -C build-dir install
cd ..

echo "Installing libpng..."
rm -fr "libpng-$LIBPNG"
tar xf "libpng-$LIBPNG.tar.xz"
cd "libpng-$LIBPNG"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_FRAMEWORK=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing libjpeg..."
rm -fr "jpeg-$LIBJPEG"
tar xf "jpegsrc.v$LIBJPEG.tar.gz"
cd "jpeg-$LIBJPEG"
mkdir build
cd build
../configure --prefix="$INSTALLDIR" --disable-static --enable-shared --host="x86_64-apple-darwin" CFLAGS="-arch x86_64"
make "-j$NPROCS"
make install
cd ../..

echo "Installing WebP..."
rm -fr "libwebp-$LIBWEBP"
tar xf "libwebp-$LIBWEBP.tar.gz"
cd "libwebp-$LIBWEBP"
cmake "${CMAKE_COMMON[@]}" -B build \
	-DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
	-DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DBUILD_SHARED_LIBS=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building FreeType without HarfBuzz..."
rm -fr "freetype-$FREETYPE"
tar xf "freetype-$FREETYPE.tar.xz"
cd "freetype-$FREETYPE"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_DISABLE_HARFBUZZ=TRUE -B build
cmake --build build --parallel
cmake --install build
cd ..

echo "Building HarfBuzz..."
rm -fr "harfbuzz-$HARFBUZZ"
tar xf "harfbuzz-$HARFBUZZ.tar.gz"
cd "harfbuzz-$HARFBUZZ"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DHB_BUILD_UTILS=OFF -B build
cmake --build build --parallel
cmake --install build
cd ..

echo "Building FreeType with HarfBuzz..."
rm -fr "freetype-$FREETYPE"
tar xf "freetype-$FREETYPE.tar.xz"
cd "freetype-$FREETYPE"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_REQUIRE_HARFBUZZ=TRUE -B build
cmake --build build --parallel
cmake --install build
cd ..

# MoltenVK already builds universal binaries, nothing special to do here.
echo "Installing MoltenVK..."
rm -fr "MoltenVK-${MOLTENVK}"
tar xf "v$MOLTENVK.tar.gz"
cd "MoltenVK-${MOLTENVK}"
sed -i '' 's/xcodebuild "$@"/xcodebuild $XCODEBUILD_EXTRA_ARGS "$@"/g' fetchDependencies
sed -i '' 's/XCODEBUILD :=/XCODEBUILD ?=/g' Makefile
XCODEBUILD_EXTRA_ARGS="VALID_ARCHS=x86_64" ./fetchDependencies --macos
XCODEBUILD="set -o pipefail && xcodebuild VALID_ARCHS=x86_64" make macos
cp Package/Latest/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib "$INSTALLDIR/lib/"
cd ..

echo "Installing Qt Base..."
rm -fr "qtbase-everywhere-src-$QT"
tar xf "qtbase-everywhere-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
# since we don't have a direct reference to QtSvg, it doesn't deployed directly from the main binary
# (only indirectly from iconengines), and the libqsvg.dylib imageformat plugin does not get deployed.
# We could run macdeployqt twice, but that's even more janky than patching it.
patch -u src/tools/macdeployqt/shared/shared.cpp <<EOF
--- shared.cpp
+++ shared.cpp
@@ -1119,14 +1119,8 @@
         addPlugins(QStringLiteral("networkinformation"));
     }
 
-    // All image formats (svg if QtSvg is used)
-    const bool usesSvg = deploymentInfo.containsModule("Svg", libInfix);
-    addPlugins(QStringLiteral("imageformats"), [usesSvg](const QString &lib) {
-        if (lib.contains(QStringLiteral("qsvg")) && !usesSvg)
-            return false;
-        return true;
-    });
-
+    // All image formats
+    addPlugins(QStringLiteral("imageformats"));
     addPlugins(QStringLiteral("iconengines"));
 
     // Platforminputcontext plugins if QtGui is in use
EOF
cmake -B build "${CMAKE_COMMON[@]}" -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt SVG..."
rm -fr "qtsvg-everywhere-src-$QT"
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}"
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Image Formats..."
rm -fr "qtimageformats-everywhere-src-$QT"
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DFEATURE_system_webp=ON
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Tools..."
rm -fr "qttools-everywhere-src-$QT"
tar xf "qttools-everywhere-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
# Linguist relies on a library in the Designer target, which takes 5-7 minutes to build on the CI
# Avoid it by not building Linguist, since we only need the tools that come with it
patch -u src/linguist/CMakeLists.txt <<EOF
--- src/linguist/CMakeLists.txt
+++ src/linguist/CMakeLists.txt
@@ -14,7 +14,7 @@
 add_subdirectory(lrelease-pro)
 add_subdirectory(lupdate)
 add_subdirectory(lupdate-pro)
-if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND NOT no-png)
+if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND TARGET Qt::PrintSupport AND NOT no-png)
     add_subdirectory(linguist)
 endif()
EOF
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
make "-j$NPROCS"
make install
cd ../..

echo "Building shaderc..."
rm -fr "shaderc-$SHADERC"
tar xf "shaderc-$SHADERC.tar.gz"
cd "shaderc-$SHADERC"
cd third_party
tar xf "../../shaderc-glslang-$SHADERC_GLSLANG.tar.gz"
mv "glslang-$SHADERC_GLSLANG" "glslang"
tar xf "../../shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz"
mv "SPIRV-Headers-$SHADERC_SPIRVHEADERS" "spirv-headers"
tar xf "../../shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz"
mv "SPIRV-Tools-$SHADERC_SPIRVTOOLS" "spirv-tools"
cd ..
patch -p1 < "$SCRIPTDIR/../common/shaderc-changes.patch"
cmake "${CMAKE_COMMON[@]}" "$CMAKE_ARCH_UNIVERSAL" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt Translations..."
rm -fr "qttranslations-everywhere-src-$QT"
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}"
make "-j$NPROCS"
make install
cd ../..

echo "Cleaning up..."
cd ..
rm -rf deps-build
