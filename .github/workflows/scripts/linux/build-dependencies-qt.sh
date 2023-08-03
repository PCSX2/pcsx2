#!/usr/bin/env bash

set -e

INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.28.1
QT=6.5.2
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
4977ceba5c0054dbe6c2f114641aced43ce3bf2b41ea64b6a372d6ba129cb15d  $SDL.tar.gz
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
3db4c729b4d80a9d8fda8dd77128406353baff4755ca619177eda4cddae71269  qtbase-everywhere-src-$QT.tar.xz
aae0c08924c6a5e47f9d57e031673d611ffff7aab2bee2e1cc460471ecac6743  qtimageformats-everywhere-src-$QT.tar.xz
48b4cc1093af2e0ab3bea30f60651bddd877a2335d16e7207879a2e9e81963a3  qtsvg-everywhere-src-$QT.tar.xz
551ffb22751d8fd4d88e9ebd55b9131f4ca55341ee497fdbbba4da8d10d94341  qttools-everywhere-src-$QT.tar.xz
337c45637e757e754c2f0ea65c20de3e6e53a841dda1253db15baa622515beeb  qttranslations-everywhere-src-$QT.tar.xz
3020be86fb7fd0abb8509906ca6583cadcaee168159abceaeb5b3e9d42563c9a  qtwayland-everywhere-src-$QT.tar.xz
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
./configure --prefix "$INSTALLDIR" --disable-dbus --without-x --disable-video-opengl --disable-video-opengles --disable-video-vulkan --disable-wayland-shared --disable-ime --disable-oss --disable-alsa --disable-jack --disable-esd --disable-pipewire --disable-pulseaudio --disable-arts --disable-nas --disable-sndio --disable-fusionsound --disable-diskaudio
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
../configure -prefix "$INSTALLDIR" -release -no-dbus -gui -widgets -fontconfig -qt-doubleconversion -ssl -openssl-runtime -opengl desktop -qpa xcb,wayland -xkbcommon -- -DFEATURE_dbus=OFF -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF
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
