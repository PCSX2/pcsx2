#!/usr/bin/env bash

set -e
export CMAKE_POLICY_DEFAULT_CMP0174=NEW
export CMAKE_POLICY_DEFAULT_CMP0177=NEW

if [ "$#" -ne 1 ]; then
    echo "Syntax: $0 <output directory>"
    exit 1
fi

SCRIPTDIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))
NPROCS="$(getconf _NPROCESSORS_ONLN)"
INSTALLDIR="$1"
if [ "${INSTALLDIR:0:1}" != "/" ]; then
	INSTALLDIR="$PWD/$INSTALLDIR"
fi

LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075
LIBJPEGTURBO=3.1.0
LIBPNG=1.6.45
LIBWEBP=1.5.0
LZ4=b8fd2d15309dd4e605070bd4486e26b6ef814e29
SDL=SDL3-3.2.8
QT=6.8.2
ZSTD=1.5.7
KDDOCKWIDGETS=2.2.1

SHADERC=2024.1
SHADERC_GLSLANG=142052fa30f9eca191aa9dcf65359fcaed09eeec
SHADERC_SPIRVHEADERS=5e3ad389ee56fca27c9705d093ae5387ce404df4
SHADERC_SPIRVTOOLS=dd4b663e13c07fea4fbb3f70c1c91c86731099f7

# Set Android NDK and target architecture
ANDROID_SDK_PATH="/home/swami/Android/Sdk/"
ANDROID_NDK_PATH="/home/swami/Android/Sdk/ndk/29.0.13113456/"
ANDROID_API_LEVEL=21  # Adjust this to your target API level
ANDROID_ARCH="arm64-v8a"  # Change this to your desired architecture (e.g., armv7a, x86, etc.)

export ANDROID_SDK=$ANDROID_SDK_PATH
export ANDROID_NDK=$ANDROID_NDK_PATH
export ANDROID_API=$ANDROID_API_LEVEL
export ANDROID_ARCH=$ANDROID_ARCH

# Ensure you're using the Android cross-compilation toolchain
export PATH=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
926485350139ffb51ef69760db35f78846c805fef3d59bfdcb2fba704663f370  libpng-$LIBPNG.tar.xz
7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c  libwebp-$LIBWEBP.tar.gz
0728800155f3ed0a0c87e03addbd30ecbe374f7b080678bbca1506051d50dec3  $LZ4.tar.gz
13388fabb361de768ecdf2b65e52bb27d1054cae6ccb6942ba926e378e00db03  $SDL.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
012043ce6d411e6e8a91fdc4e05e6bedcfa10fcb1347d3c33908f7fdd10dfe05  qtbase-everywhere-src-$QT.tar.xz
d2a1bbb84707b8a0aec29227b170be00f04383fbf2361943596d09e7e443c8e1  qtimageformats-everywhere-src-$QT.tar.xz
aa2579f21ca66d19cbcf31d87e9067e07932635d36869c8239d4decd0a9dc1fa  qtsvg-everywhere-src-$QT.tar.xz
326381b7d43f07913612f291abc298ae79bd95382e2233abce982cff2b53d2c0  qttools-everywhere-src-$QT.tar.xz
d2106e8a580bfd77702c4c1840299288d344902b0e2c758ca813ea04c6d6a3d1  qttranslations-everywhere-src-$QT.tar.xz
5e46157908295f2bf924462d8c0855b0508ba338ced9e810891fefa295dc9647  qtwayland-everywhere-src-$QT.tar.xz
eb3b5f0c16313d34f208d90c2fa1e588a23283eed63b101edd5422be6165d528  shaderc-$SHADERC.tar.gz
aa27e4454ce631c5a17924ce0624eac736da19fc6f5a2ab15a6c58da7b36950f  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
5d866ce34a4b6908e262e5ebfffc0a5e11dd411640b5f24c85a80ad44c0d4697  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
03ee1a2c06f3b61008478f4abe9423454e53e580b9488b47c8071547c6a9db47  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
8693e06abee0c88517d8480b22647702a51a0708f3c876ed5385d9a4e356e1a5  KDDockWidgets-$KDDOCKWIDGETS.tar.gz
EOF

curl -L \
	-O "https://github.com/ianlancetaylor/libbacktrace/archive/$LIBBACKTRACE.zip" \
	-O "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO/libjpeg-turbo-$LIBJPEGTURBO.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
	-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
	-O "https://github.com/lz4/lz4/archive/$LZ4.tar.gz" \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtwayland-everywhere-src-$QT.tar.xz" \
	-o "shaderc-$SHADERC.tar.gz" "https://github.com/google/shaderc/archive/refs/tags/v$SHADERC.tar.gz" \
	-o "shaderc-glslang-$SHADERC_GLSLANG.tar.gz" "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG.tar.gz" \
	-o "shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS.tar.gz" \
	-o "shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS.tar.gz" \
	-o "KDDockWidgets-$KDDOCKWIDGETS.tar.gz" "https://github.com/KDAB/KDDockWidgets/archive/v$KDDOCKWIDGETS.tar.gz"

shasum -a 256 --check SHASUMS


# Build libbacktrace for Android
echo "Building libbacktrace for Android..."
unzip "$LIBBACKTRACE.zip"
cd "libbacktrace-$LIBBACKTRACE"
./configure --prefix="$INSTALLDIR" --host=arm-linux-androideabi --build=x86_64-linux-gnu --with-android-ndk=$ANDROID_NDK
make
make install
cd ..

# Build libpng for Android 
echo "Building libpng for Android..."
rm -fr "libpng-$LIBPNG"
tar xf "libpng-$LIBPNG.tar.xz"
cd "libpng-$LIBPNG"
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DBUILD_SHARED_LIBS=ON \
	-DPNG_TESTS=OFF \
	-DPNG_STATIC=OFF \
	-DPNG_SHARED=ON \
	-DPNG_TOOLS=OFF \
	-B build \
	-G Ninja 
cmake --build build --parallel
ninja -C build install
cd ..

# build libjpeg for Android
echo "Building libjpegturbo for Android..."
rm -fr "libjpeg-turbo-$LIBJPEGTURBO"
tar xf "libjpeg-turbo-$LIBJPEGTURBO.tar.gz"
cd "libjpeg-turbo-$LIBJPEGTURBO"

# Run cmake with the Android-specific toolchain
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
    -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
    -DENABLE_STATIC=OFF \
    -DENABLE_SHARED=ON \
    -B build \
	-G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building LZ4..."
rm -fr "lz4-$LZ4"
tar xf "$LZ4.tar.gz"
cd "lz4-$LZ4"
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DBUILD_SHARED_LIBS=ON \
	-DLZ4_BUILD_CLI=OFF \
	-DLZ4_BUILD_LEGACY_LZ4C=OFF \
	-B build \
	-G Ninja build/cmake
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building Zstandard..."
rm -fr "zstd-$ZSTD"
tar xf "zstd-$ZSTD.tar.gz"
cd "zstd-$ZSTD"
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DBUILD_SHARED_LIBS=ON \
	-DZSTD_BUILD_SHARED=ON \
	-DZSTD_BUILD_STATIC=OFF \
	-DZSTD_BUILD_PROGRAMS=OFF \
	-B build \
	-G Ninja build/cmake
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building WebP..."
rm -fr "libwebp-$LIBWEBP"
tar xf "libwebp-$LIBWEBP.tar.gz"
cd "libwebp-$LIBWEBP"
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DWEBP_BUILD_ANIM_UTILS=OFF \
	-DWEBP_BUILD_CWEBP=OFF \
	-DWEBP_BUILD_DWEBP=OFF \
	-DWEBP_BUILD_GIF2WEBP=OFF \
	-DWEBP_BUILD_IMG2WEBP=OFF \
	-DWEBP_BUILD_VWEBP=OFF \
	-DWEBP_BUILD_WEBPINFO=OFF \
	-DWEBP_BUILD_WEBPMUX=OFF \
	-DWEBP_BUILD_EXTRAS=OFF \
	-DBUILD_SHARED_LIBS=ON \
	-B build \
	-G Ninja 
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building Freetype..."
git clone https://gitlab.freedesktop.org/freetype/freetype.git
cd freetype
./autogen.sh
./configure --prefix="$INSTALLDIR" \
	 --host=arm-linux-androideabi \
	 --build=x86_64-linux-gnu \
	 --with-android-ndk=$ANDROID_NDK\
	 --with-zlib=yes \
	 --with-png=yes \
	 --with-harfbuzz=no

make && make install
cd ..

echo "Building Harfbuzz..."
git clone https://github.com/harfbuzz/harfbuzz.git
cd harfbuzz
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DBUILD_SHARED_LIBS=ON \
    -Dcoretext=disabled \
    -Dfreetype=enabled \
    -Dglib=disabled \
	-B build \
	-G Ninja 
cmake --build build --parallel
ninja -C build install
cd ..

git clone https://github.com/openssl/openssl.git
cd openssl
git checkout openssl-3.2.2
./Configure android-arm64 no-shared \
    -D__ANDROID_API__=$ANDROID_API \
    --prefix="$INSTALLDIR" \
    --with-zlib-include="$INSTALLDIR/include" \
    --with-zlib-lib="$INSTALLDIR/lib"
make -j$(nproc) SHLIB_VERSION_NUMBER= SHLIB_EXT=_1_1.so build_libs
make install
cd ..

echo "Building SDL..."
rm -fr "$SDL"
tar xf "$SDL.tar.gz"
cd "$SDL"
cmake -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DBUILD_SHARED_LIBS=ON \
	-DSDL_SHARED=ON \
	-DSDL_STATIC=OFF \
	-B build \
	-G Ninja 
cmake --build build --parallel
ninja -C build install
cd ..

# Couple notes:
# -fontconfig is needed otherwise Qt Widgets render only boxes.
# -qt-doubleconversion avoids a dependency on libdouble-conversion.
# ICU avoids pulling in a bunch of large libraries, and hopefully we can get away without it.
# OpenGL is needed to render window decorations in Wayland, apparently.
echo "Building Qt Base..."
rm -fr "qtbase-everywhere-src-$QT"
tar xf "qtbase-everywhere-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
mkdir build-host
cd build-host
../configure -developer-build -nomake tests -nomake examples
cmake --build . --target host_tools
cd ..
mkdir build
cd ./build
# Configure Qt for Android
../configure -prefix "$INSTALLDIR" \
    -release \
    -android-ndk "$ANDROID_NDK" \
    -android-sdk "$ANDROID_SDK" \
    -android-abis "$ANDROID_ARCH" \
	-qt-host-path "../build-host" \
    -qt-doubleconversion \
    -ssl -openssl-runtime \
    -opengl es2 \
    -no-dbus \
    -no-xcb \
    -no-gtk \
    -xkbcommon \
    -- -DFEATURE_dbus=OFF \
       -DFEATURE_icu=OFF \
       -DFEATURE_printsupport=OFF \
       -DFEATURE_sql=OFF \
       -DFEATURE_system_png=ON \
       -DFEATURE_system_jpeg=ON \
       -DFEATURE_system_zlib=ON \
       -DFEATURE_system_freetype=ON \
       -DFEATURE_system_harfbuzz=ON
cmake --build . --parallel
ninja install
cd ../../

echo "Building Qt SVG..."
rm -fr "qtsvg-everywhere-src-$QT"
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR"
cmake --build . --parallel
ninja install
cd ../../

echo "Building Qt Image Formats..."
rm -fr "qtimageformats-everywhere-src-$QT"
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DFEATURE_system_webp=ON
cmake --build . --parallel
ninja install
cd ../../

echo "Building Qt Wayland..."
rm -fr "qtwayland-everywhere-src-$QT"
tar xf "qtwayland-everywhere-src-$QT.tar.xz"
cd "qtwayland-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR"
cmake --build . --parallel
ninja install
cd ../../

echo "Installing Qt Tools..."
rm -fr "qttools-everywhere-src-$QT"
tar xf "qttools-everywhere-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
# Force disable clang scanning, it gets very confused.
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
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=ON -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
cmake --build . --parallel
ninja install
cd ../../

echo "Installing Qt Translations..."
rm -fr "qttranslations-everywhere-src-$QT"
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR"
cmake --build . --parallel
ninja install
cd ../../

echo "Building KDDockWidgets..."
rm -fr "KDDockWidgets-$KDDOCKWIDGETS"
tar xf "KDDockWidgets-$KDDOCKWIDGETS.tar.gz"
cd "KDDockWidgets-$KDDOCKWIDGETS"
patch -p1 < "$SCRIPTDIR/../common/kddockwidgets-dodgy-include.patch"
cmake -DCMAKE_BUILD_TYPE=Release 
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" 
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" 
	-DKDDockWidgets_QT6=true 
	-DKDDockWidgets_EXAMPLES=false 
	-DKDDockWidgets_FRONTENDS=qtwidgets 
	-B build 
	-G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

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
cmake -DCMAKE_BUILD_TYPE=Release 
	-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_PATH/build/cmake/android.toolchain.cmake \
	-DANDROID_ABI=arm64-v8a \
	-DANDROID_NATIVE_API_LEVEL=21 \
	-DCMAKE_PREFIX_PATH="$INSTALLDIR" 
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" 
	-DSHADERC_SKIP_TESTS=ON 
	-DSHADERC_SKIP_EXAMPLES=ON 
	-DSHADERC_SKIP_COPYRIGHT_CHECK=ON 
	-B build 
	-G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
