#!/bin/bash

set -e

if [ "$#" -ne 1 ]; then
    echo "Syntax: $0 <output directory>"
    exit 1
fi

export MACOSX_DEPLOYMENT_TARGET=11.0

INSTALLDIR="$1"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.30.0
XZ=5.4.5
ZSTD=1.5.5
LZ4=b8fd2d15309dd4e605070bd4486e26b6ef814e29
PNG=1.6.37
WEBP=1.3.2
FFMPEG=6.0
QT=6.6.0

mkdir -p deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include $CXXFLAGS"

cat > SHASUMS <<EOF
36e2e41557e0fa4a1519315c0f5958a87ccb27e25c51776beb6f1239526447b0  $SDL.tar.gz
135c90b934aee8fbc0d467de87a05cb70d627da36abe518c357a873709e5b7d6  xz-$XZ.tar.gz
9c4396cc829cfae319a6e2615202e82aad41372073482fce286fac78646d3ee4  zstd-$ZSTD.tar.gz
0728800155f3ed0a0c87e03addbd30ecbe374f7b080678bbca1506051d50dec3  $LZ4.tar.gz
505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca  libpng-$PNG.tar.xz
2a499607df669e40258e53d0ade8035ba4ec0175244869d1025d460562aa09b4  libwebp-$WEBP.tar.gz
57be87c22d9b49c112b6d24bc67d42508660e6b718b3db89c44e47e289137082  ffmpeg-$FFMPEG.tar.xz
039d53312acb5897a9054bd38c9ccbdab72500b71fdccdb3f4f0844b0dd39e0e  qtbase-everywhere-src-$QT.tar.xz
e1542cb50176e237809895c6549598c08587c63703d100be54ac2d806834e384  qtimageformats-everywhere-src-$QT.tar.xz
33da25fef51102f564624a7ea3e57cb4a0a31b7b44783d1af5749ac36d3c72de  qtsvg-everywhere-src-$QT.tar.xz
4e9feebc142bbb6e453e1dc3277e09ec45c8ef081b5ee2a029e6684b5905ba99  qttools-everywhere-src-$QT.tar.xz
a0d89a236f64b810eb0fe4ae1e90db22b0e86263521b35f89e69f1392815078c  qttranslations-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/tukaani-project/xz/releases/download/v$XZ/xz-$XZ.tar.gz" \
	-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
	-O "https://github.com/lz4/lz4/archive/$LZ4.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$PNG/libpng-$PNG.tar.xz" \
	-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$WEBP.tar.gz" \
	-O "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \

shasum -a 256 --check SHASUMS

echo "Installing SDL..."
tar xf "$SDL.tar.gz"
cd "$SDL"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DSDL_X11=OFF -DBUILD_SHARED_LIBS=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing FFmpeg..."
tar xf "ffmpeg-$FFMPEG.tar.xz"
cd "ffmpeg-$FFMPEG"
LDFLAGS="-dead_strip $LDFLAGS" CFLAGS="-Os $CFLAGS" CXXFLAGS="-Os $CXXFLAGS" \
	./configure --prefix="$INSTALLDIR" \
	--enable-cross-compile --arch=x86_64 --cc='clang -arch x86_64' --cxx='clang++ -arch x86_64' --disable-x86asm \
	--disable-all --disable-autodetect --disable-static --enable-shared \
	--enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
	--enable-audiotoolbox --enable-videotoolbox \
	--enable-encoder=ffv1,qtrle,pcm_s16be,pcm_s16le,*_at,*_videotoolbox \
	--enable-muxer=avi,matroska,mov,mp3,mp4,wav \
	--enable-protocol=file
make "-j$NPROCS"
make install
cd ..

echo "Installing XZ..."
tar xf "xz-$XZ.tar.gz"
cd "xz-$XZ"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -B build
make -C build "-j$NPROCS"
make -C build install
make install
cd ..

echo "Installing Zstd..."
tar xf "zstd-$ZSTD.tar.gz"
cd "zstd-$ZSTD"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DBUILD_SHARED_LIBS=ON -DZSTD_BUILD_PROGRAMS=OFF -B build-dir build/cmake
make -C build-dir "-j$NPROCS"
make -C build-dir install
cd ..

echo "Installing LZ4..."
tar xf "$LZ4.tar.gz"
cd "lz4-$LZ4"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -B build-dir build/cmake
make -C build-dir "-j$NPROCS"
make -C build-dir install
cd ..

echo "Installing libpng..."
tar xf "libpng-$PNG.tar.xz"
cd "libpng-$PNG"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -B build
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing WebP..."
tar xf "libwebp-$WEBP.tar.gz"
cd "libwebp-$WEBP"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_OSX_ARCHITECTURES="x86_64" -B build \
	-DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
	-DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DBUILD_SHARED_LIBS=ON
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt Base..."
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
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64" -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt SVG..."
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" ..
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Image Formats..."
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" ..
make "-j$NPROCS"
make install
cd ../..

echo "Installing Qt Tools..."
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
"$INSTALLDIR/bin/qt-configure-module" .. -- -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
make "-j$NPROCS"
make install 
cd ../..

echo "Installing Qt Translations..."
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" ..
make "-j$NPROCS"
make install
cd ../..

echo "Cleaning up..."
cd ..
rm -rf deps-build
