#!/usr/bin/env bash

set -e

if [ "$#" -ne 1 ]; then
    echo "Syntax: $0 <output directory>"
    exit 1
fi

INSTALLDIR="$1"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.30.1
QT=6.6.3
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075

if [ "${INSTALLDIR:0:1}" != "/" ]; then
	INSTALLDIR="$PWD/$INSTALLDIR"
fi

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
01215ffbc8cfc4ad165ba7573750f15ddda1f971d5a66e9dcaffd37c587f473a  $SDL.tar.gz
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
0493fd0b380c4edf8872f011a7f26d245aa4cdd75b349904ef340a22dedf7462  qtbase-everywhere-src-$QT.tar.xz
3ca5ea60176603ce6ffc1bff59a4dcea139375233ce8e5e86c38f4e84c44627c  qtimageformats-everywhere-src-$QT.tar.xz
4acb1e576eca55e955cf2b0d15c914a200df290e737accd7c1901fa1e33a25c7  qtsvg-everywhere-src-$QT.tar.xz
aa6d4c822d8cb74066ef30ab42283ac24e5cc702f33e6d78a9ebef5b0df91bc0  qttools-everywhere-src-$QT.tar.xz
12e35f2ac9a262e41827d95f168d4de6eb85c166bdaf7e5b3291f8f516cf73cf  qttranslations-everywhere-src-$QT.tar.xz
a96ecc0fecc05f9e18cfb7806fe5ebd7416be94e8f51ebeca75c80412f66553d  qtwayland-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/ianlancetaylor/libbacktrace/archive/$LIBBACKTRACE.zip" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtwayland-everywhere-src-$QT.tar.xz"

shasum -a 256 --check SHASUMS

echo "Building SDL..."
tar xf "$SDL.tar.gz"
cd "$SDL"
./configure --prefix "$INSTALLDIR" --enable-dbus --without-x --disable-video-opengl --disable-video-opengles --disable-video-vulkan --disable-wayland-shared --disable-ime --disable-oss --disable-alsa --disable-jack --disable-esd --disable-pipewire --disable-pulseaudio --disable-arts --disable-nas --disable-sndio --disable-fusionsound --disable-diskaudio
make "-j$NPROCS"
make install
cd ..

echo "Building libbacktrace..."
unzip "$LIBBACKTRACE.zip"
cd "libbacktrace-$LIBBACKTRACE"
./configure --prefix="$INSTALLDIR"
make
make install
cd ..

# Couple notes:
# -fontconfig is needed otherwise Qt Widgets render only boxes.
# -qt-doubleconversion avoids a dependency on libdouble-conversion.
# ICU avoids pulling in a bunch of large libraries, and hopefully we can get away without it.
# OpenGL is needed to render window decorations in Wayland, apparently.
echo "Building Qt Base..."
tar xf "qtbase-everywhere-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
mkdir build
cd build
../configure -prefix "$INSTALLDIR" -release -dbus-linked -gui -widgets -fontconfig -qt-doubleconversion -ssl -openssl-runtime -opengl desktop -qpa xcb,wayland -xkbcommon -- -DFEATURE_dbus=ON -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF
cmake --build . --parallel
cmake --install .
cd ../../

echo "Building Qt SVG..."
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
mkdir build
cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
cmake --install .
cd ../../

echo "Building Qt Image Formats..."
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
mkdir build
cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
cmake --install .
cd ../../

echo "Building Qt Wayland..."
tar xf "qtwayland-everywhere-src-$QT.tar.xz"
cd "qtwayland-everywhere-src-$QT"
mkdir build
cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
cmake --install .
cd ../../

echo "Installing Qt Tools..."
tar xf "qttools-everywhere-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
# From Mac build-dependencies.sh:
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

# Also force disable clang scanning, it gets very confused.
patch -u configure.cmake <<EOF
--- configure.cmake
+++ configure.cmake
@@ -14,12 +14,12 @@
 # Presumably because 6.0 ClangConfig.cmake files are not good enough?
 # In any case explicitly request a minimum version of 8.x for now, otherwise
 # building with CMake will fail at compilation time.
-qt_find_package(WrapLibClang 8 PROVIDED_TARGETS WrapLibClang::WrapLibClang)
+#qt_find_package(WrapLibClang 8 PROVIDED_TARGETS WrapLibClang::WrapLibClang)
 # special case end

-if(TARGET WrapLibClang::WrapLibClang)
-    set(TEST_libclang "ON" CACHE BOOL "Required libclang version found." FORCE)
-endif()
+#if(TARGET WrapLibClang::WrapLibClang)
+#    set(TEST_libclang "ON" CACHE BOOL "Required libclang version found." FORCE)
+#endif()



EOF

mkdir build
cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF ..
cmake --build . --parallel
cmake --install .
cd ../../

echo "Installing Qt Translations..."
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
mkdir build
cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
cmake --install .
cd ../../

echo "Cleaning up..."
cd ..
rm -r deps-build
