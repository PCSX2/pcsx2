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

FREETYPE=2.14.1
HARFBUZZ=12.0.0
SDL=SDL3-3.2.26
ZSTD=1.5.7
LZ4=1.10.0
LIBPNG=1.6.50
LIBJPEGTURBO=3.1.2
LIBWEBP=1.6.0
FFMPEG=8.0
MOLTENVK=1.2.9
QT=6.10.0
QTAPNG=1.3.0
KDDOCKWIDGETS=2.4.0
PLUTOVG=1.3.1
PLUTOSVG=0.0.7

SHADERC=2025.3
SHADERC_GLSLANG=efd24d75bcbc55620e759f6bf42c45a32abac5f8
SHADERC_SPIRVHEADERS=2a611a970fdbc41ac2e3e328802aed9985352dca
SHADERC_SPIRVTOOLS=33e02568181e3312f49a3cf33df470bf96ef293a

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
	-DCMAKE_APPLE_SILICON_PROCESSOR="x86_64"
	-DCMAKE_INSTALL_NAME_DIR='$<INSTALL_PREFIX>/lib'
)

cat > SHASUMS <<EOF
32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc  freetype-$FREETYPE.tar.xz
c4a398539c3e0fdc9a82dfe7824d0438cae78c1e2124e7c6ada3dfa600cdb6c8  harfbuzz-$HARFBUZZ.tar.gz
dad488474a51a0b01d547cd2834893d6299328d2e30f479a3564088b5476bae2  $SDL.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b  lz4-$LZ4.tar.gz
4df396518620a7aa3651443e87d1b2862e4e88cad135a8b93423e01706232307  libpng-$LIBPNG.tar.xz
e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564  libwebp-$LIBWEBP.tar.gz
687ddc0c7cb128a3ea58e159b5129252537c27ede0c32a93f11f03127f0c0165  libpng-$LIBPNG-apng.patch.gz
8f0012234b464ce50890c490f18194f913a7b1f4e6a03d6644179fa0f867d0cf  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
b2751fccb6cc4c77708113cd78b561059b6fa904b24162fa0be2d60273d27b8e  ffmpeg-$FFMPEG.tar.xz
f415a09385030c6510a936155ce211f617c31506db5fbc563e804345f1ecf56e  v$MOLTENVK.tar.gz
ead4623bcb54a32257c5b3e3a5aec6d16ec96f4cda58d2e003f5a0c16f72046d  qtbase-everywhere-src-$QT.tar.xz
64450a52507c540de53616ed5e516df0e0905a99d3035ddfaa690f2b3f7c0cea  qtimageformats-everywhere-src-$QT.tar.xz
5ed2c0e04d5e73ff75c2a2ed92db5dc1788ba70f704fc2b71bc21644beda2533  qtsvg-everywhere-src-$QT.tar.xz
d86d5098cf3e3e599f37e18df477e65908fc8f036e10ea731b3469ec4fdbd02a  qttools-everywhere-src-$QT.tar.xz
326e8253cfd0cb5745238117f297da80e30ce8f4c1db81990497bd388b026cde  qttranslations-everywhere-src-$QT.tar.xz
f1d3be3489f758efe1a8f12118a212febbe611aa670af32e0159fa3c1feab2a6  QtApng-$QTAPNG.tar.gz
a8e4a25e5c2686fd36981e527ed05e451fcfc226bddf350f4e76181371190937  shaderc-$SHADERC.tar.gz
9427deccbdf4bde6a269938df38c6bd75247493786a310d8d733a2c82065ef47  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
c2225a49c3d7efa5c4f4ce4a6b42081e6ea3daca376f3353d9d7c2722d77a28a  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
44d1005880c583fc00a0fb41c839214c68214b000ea8dcb54d352732fee600ff  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
51dbf24fe72e43dd7cb9a289d3cab47112010f1a2ed69b6fc8ac0dff31991ed2  KDDockWidgets-$KDDOCKWIDGETS.tar.gz
bea672eb96ee36c2cbeb911b9bac66dfe989b3ad9a9943101e00aeb2df2aefdb  plutovg-$PLUTOVG.tar.gz
78561b571ac224030cdc450ca2986b4de915c2ba7616004a6d71a379bffd15f3  plutosvg-$PLUTOSVG.tar.gz
EOF

curl -L \
	-o "freetype-$FREETYPE.tar.xz" "https://sourceforge.net/projects/freetype/files/freetype2/$FREETYPE/freetype-$FREETYPE.tar.xz/download" \
	-o "harfbuzz-$HARFBUZZ.tar.gz" "https://github.com/harfbuzz/harfbuzz/archive/refs/tags/$HARFBUZZ.tar.gz" \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
	-O "https://github.com/lz4/lz4/releases/download/v$LZ4/lz4-$LZ4.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
	-O "https://download.sourceforge.net/libpng-apng/libpng-$LIBPNG-apng.patch.gz" \
	-O "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO/libjpeg-turbo-$LIBJPEGTURBO.tar.gz" \
	-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
	-O "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" \
	-O "https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v$MOLTENVK.tar.gz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \
	-o "QtApng-$QTAPNG.tar.gz" "https://github.com/jurplel/QtApng/archive/refs/tags/$QTAPNG.tar.gz" \
	-o "shaderc-$SHADERC.tar.gz" "https://github.com/google/shaderc/archive/refs/tags/v$SHADERC.tar.gz" \
	-o "shaderc-glslang-$SHADERC_GLSLANG.tar.gz" "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG.tar.gz" \
	-o "shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS.tar.gz" \
	-o "shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS.tar.gz" \
	-o "KDDockWidgets-$KDDOCKWIDGETS.tar.gz" "https://github.com/KDAB/KDDockWidgets/archive/v$KDDOCKWIDGETS.tar.gz" \
	-o "plutovg-$PLUTOVG.tar.gz" "https://github.com/sammycage/plutovg/archive/v$PLUTOVG.tar.gz" \
	-o "plutosvg-$PLUTOSVG.tar.gz" "https://github.com/sammycage/plutosvg/archive/v$PLUTOSVG.tar.gz"

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
tar xf "lz4-$LZ4.tar.gz"
cd "lz4-$LZ4"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -B build-dir build/cmake
make -C build-dir "-j$NPROCS"
make -C build-dir install
cd ..

echo "Installing libpng..."
rm -fr "libpng-$LIBPNG"
tar xf "libpng-$LIBPNG.tar.xz"
gunzip -d -f "libpng-$LIBPNG-apng.patch.gz"
cd "libpng-$LIBPNG"
patch -p1 < "../libpng-$LIBPNG-apng.patch"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_FRAMEWORK=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing libjpegturbo..."
rm -fr "libjpeg-turbo-$LIBJPEGTURBO"
tar xf "libjpeg-turbo-$LIBJPEGTURBO.tar.gz"
cd "libjpeg-turbo-$LIBJPEGTURBO"
cmake "${CMAKE_COMMON[@]}" "$CMAKE_ARCH_X64" -DENABLE_STATIC=OFF -DENABLE_SHARED=ON -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

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
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building HarfBuzz..."
rm -fr "harfbuzz-$HARFBUZZ"
tar xf "harfbuzz-$HARFBUZZ.tar.gz"
cd "harfbuzz-$HARFBUZZ"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DHB_BUILD_UTILS=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building FreeType with HarfBuzz..."
rm -fr "freetype-$FREETYPE"
tar xf "freetype-$FREETYPE.tar.xz"
cd "freetype-$FREETYPE"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_REQUIRE_HARFBUZZ=TRUE -B build
make -C build "-j$NPROCS"
make -C build install
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

# Patch Qt to support macOS 11
patch -p1 < "$SCRIPTDIR/qt-macos11compat.patch"

# since we don't have a direct reference to QtSvg, it doesn't deployed directly from the main binary
# (only indirectly from iconengines), and the libqsvg.dylib imageformat plugin does not get deployed.
# We could run macdeployqt twice, but that's even more janky than patching it.
patch -u src/tools/macdeployqt/shared/shared.cpp <<EOF
--- shared.cpp
+++ shared.cpp
@@ -1122,14 +1122,8 @@
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
cmake -B build "${CMAKE_COMMON[@]}" -DCMAKE_BUILD_TYPE=MinSizeRel -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt SVG..."
rm -fr "qtsvg-everywhere-src-$QT"
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DQT_GENERATE_SBOM=OFF
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Image Formats..."
rm -fr "qtimageformats-everywhere-src-$QT"
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DQT_GENERATE_SBOM=OFF -DFEATURE_system_webp=ON
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Tools..."
rm -fr "qttools-everywhere-src-$QT"
tar xf "qttools-everywhere-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DQT_GENERATE_SBOM=OFF -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=ON -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
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
"$INSTALLDIR/bin/qt-configure-module" .. -- "${CMAKE_COMMON[@]}" -DQT_GENERATE_SBOM=OFF
make "-j$NPROCS"
make install
cd ../..

echo "Building Qt APNG..."
rm -fr "QtApng-$QTAPNG"
tar xf "QtApng-$QTAPNG.tar.gz"
cd "QtApng-$QTAPNG"
patch -p1 < "$SCRIPTDIR/../common/qtapng-cmake.patch"
cmake "${CMAKE_COMMON[@]}" -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building KDDockWidgets..."
rm -fr "KDDockWidgets-$KDDOCKWIDGETS"
tar xf "KDDockWidgets-$KDDOCKWIDGETS.tar.gz"
cd "KDDockWidgets-$KDDOCKWIDGETS"
cmake "${CMAKE_COMMON[@]}" -DKDDockWidgets_QT6=true -DKDDockWidgets_EXAMPLES=false -DKDDockWidgets_FRONTENDS=qtwidgets -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building PlutoVG..."
rm -fr "plutovg-$PLUTOVG"
tar xf "plutovg-$PLUTOVG.tar.gz"
cd "plutovg-$PLUTOVG"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DPLUTOVG_BUILD_EXAMPLES=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Building PlutoSVG..."
rm -fr "plutosvg-$PLUTOSVG"
tar xf "plutosvg-$PLUTOSVG.tar.gz"
cd "plutosvg-$PLUTOSVG"
cmake "${CMAKE_COMMON[@]}" -DBUILD_SHARED_LIBS=ON -DPLUTOSVG_ENABLE_FREETYPE=ON -DPLUTOSVG_BUILD_EXAMPLES=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -rf deps-build
