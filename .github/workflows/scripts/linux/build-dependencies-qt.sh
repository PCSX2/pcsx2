#!/usr/bin/env bash

set -e

INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.28.4
QT=6.6.0
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
888b8c39f36ae2035d023d1b14ab0191eb1d26403c3cf4d4d5ede30e66a4942c  $SDL.tar.gz
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
039d53312acb5897a9054bd38c9ccbdab72500b71fdccdb3f4f0844b0dd39e0e  qtbase-everywhere-src-$QT.tar.xz
e1542cb50176e237809895c6549598c08587c63703d100be54ac2d806834e384  qtimageformats-everywhere-src-$QT.tar.xz
33da25fef51102f564624a7ea3e57cb4a0a31b7b44783d1af5749ac36d3c72de  qtsvg-everywhere-src-$QT.tar.xz
4e9feebc142bbb6e453e1dc3277e09ec45c8ef081b5ee2a029e6684b5905ba99  qttools-everywhere-src-$QT.tar.xz
a0d89a236f64b810eb0fe4ae1e90db22b0e86263521b35f89e69f1392815078c  qttranslations-everywhere-src-$QT.tar.xz
30c3d82d85d050c83370710a26d12c802199c51eba5c2726e9d6243edd03b7e8  qtwayland-everywhere-src-$QT.tar.xz
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
./configure --prefix="$HOME/deps"
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
