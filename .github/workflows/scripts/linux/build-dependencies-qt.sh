#!/usr/bin/env bash

set -e

INSTALLDIR="$HOME/deps"
# In-tree:
LIBBACKTRACE_PATH="$PWD/3rdparty/libbacktrace/libbacktrace"
# To be downloaded:
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.28.1
QT=6.5.0

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
4977ceba5c0054dbe6c2f114641aced43ce3bf2b41ea64b6a372d6ba129cb15d  $SDL.tar.gz
fde1aa7b4fbe64ec1b4fc576a57f4688ad1453d2fab59cbadd948a10a6eaf5ef  qtbase-everywhere-src-$QT.tar.xz
64ca7e61f44d51e28bcbb4e0509299b53a9a7e38879e00a7fe91643196067a4f  qtsvg-everywhere-src-$QT.tar.xz
49c33d96b0a44988be954269b8ce3d1a495b439726e03a6be7c0d50a686369c4  qttools-everywhere-src-$QT.tar.xz
fc85d0fd8393f518653ccada1014177a56df6e73f30f3b64eea0c2e4a0067a3d  qttranslations-everywhere-src-$QT.tar.xz
ccc57fa277fc5f1c1c2c4733eae80a60996b67a067233c47809e542aa31759a3  qtwayland-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
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
cd "$LIBBACKTRACE_PATH"
./configure --prefix="$HOME/deps"
make
make install
cd -

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
