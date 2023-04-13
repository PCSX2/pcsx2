#!/usr/bin/env bash

set -e

INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.26.5
QT=6.4.3
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
ad8fea3da1be64c83c45b1d363a6b4ba8fd60f5bde3b23ec73855709ec5eabf7  $SDL.tar.gz
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
5087c9e5b0165e7bc3c1a4ab176b35d0cd8f52636aea903fa377bdba00891a60  qtbase-everywhere-src-$QT.tar.xz
88315f886cf81898705e487cedba6e6160724359d23c518c92c333c098879a4a  qtsvg-everywhere-src-$QT.tar.xz
867df829cd5cd3ae8efe62e825503123542764b13c96953511e567df70c5a091  qttools-everywhere-src-$QT.tar.xz
79e56b7800d49649a8a8010818538c367a829e0b7a09d5f60bd3aecf5abe972c  qttranslations-everywhere-src-$QT.tar.xz
c6b161da8f4c01e48c10b7b558a0a01ac07dba9b907b13a98ff5d89f46bc4789  qtwayland-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/ianlancetaylor/libbacktrace/archive/$LIBBACKTRACE.zip" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
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
