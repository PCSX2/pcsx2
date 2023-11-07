#!/bin/bash

set -e

export MACOSX_DEPLOYMENT_TARGET=11.0

INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.28.4
PNG=1.6.37
JPG=9e
FFMPEG=6.0
QT=6.6.0

mkdir deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib -dead_strip $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"

cat > SHASUMS <<EOF
888b8c39f36ae2035d023d1b14ab0191eb1d26403c3cf4d4d5ede30e66a4942c  $SDL.tar.gz
505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca  libpng-$PNG.tar.xz
57be87c22d9b49c112b6d24bc67d42508660e6b718b3db89c44e47e289137082  ffmpeg-$FFMPEG.tar.xz
039d53312acb5897a9054bd38c9ccbdab72500b71fdccdb3f4f0844b0dd39e0e  qtbase-everywhere-src-$QT.tar.xz
e1542cb50176e237809895c6549598c08587c63703d100be54ac2d806834e384  qtimageformats-everywhere-src-$QT.tar.xz
33da25fef51102f564624a7ea3e57cb4a0a31b7b44783d1af5749ac36d3c72de  qtsvg-everywhere-src-$QT.tar.xz
4e9feebc142bbb6e453e1dc3277e09ec45c8ef081b5ee2a029e6684b5905ba99  qttools-everywhere-src-$QT.tar.xz
a0d89a236f64b810eb0fe4ae1e90db22b0e86263521b35f89e69f1392815078c  qttranslations-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$PNG/libpng-$PNG.tar.xz" \
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

# MFI causes multiple joystick connection events, I'm guessing because both the HIDAPI and MFI interfaces
# race each other, and sometimes both end up getting through. So, just force MFI off.
patch -u CMakeLists.txt <<EOF
--- CMakeLists.txt	2023-08-03 01:33:11
+++ CMakeLists.txt	2023-08-26 12:58:53
@@ -2105,7 +2105,7 @@
           #import <Foundation/Foundation.h>
           #import <CoreHaptics/CoreHaptics.h>
           int main() { return 0; }" HAVE_FRAMEWORK_COREHAPTICS)
-      if(HAVE_FRAMEWORK_GAMECONTROLLER AND HAVE_FRAMEWORK_COREHAPTICS)
+      if(HAVE_FRAMEWORK_GAMECONTROLLER AND HAVE_FRAMEWORK_COREHAPTICS AND FALSE)
         # Only enable MFI if we also have CoreHaptics to ensure rumble works
         set(SDL_JOYSTICK_MFI 1)
         set(SDL_FRAMEWORK_GAMECONTROLLER 1)

EOF

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DSDL_X11=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing libpng..."
tar xf "libpng-$PNG.tar.xz"
cd "libpng-$PNG"
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking
make "-j$NPROCS"
make install
cd ..

echo "Installing FFmpeg..."
tar xf "ffmpeg-$FFMPEG.tar.xz"
cd "ffmpeg-$FFMPEG"
./configure --prefix="$INSTALLDIR" --disable-all --disable-autodetect --disable-static --enable-shared \
	--enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
	--enable-audiotoolbox --enable-videotoolbox \
	--enable-encoder=ffv1,qtrle,pcm_s16be,pcm_s16le,*_at,*_videotoolbox \
	--enable-muxer=avi,matroska,mov,mp3,mp4,wav \
	--enable-protocol=file
make "-j$NPROCS"
make install
cd ..

echo "Installing Qt Base..."
tar xf "qtbase-everywhere-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_optimize_size=ON -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt SVG..."
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt Image Formats..."
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
make -C build "-j$NPROCS"
make -C build install
cd ..
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
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt Translations..."
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
