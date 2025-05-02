#!/usr/bin/env bash

set -e

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
SDL=SDL3-3.2.10
QT=6.9.0
ZSTD=1.5.7
KDDOCKWIDGETS=2.2.3
PLUTOVG=0.0.13
PLUTOSVG=0.0.6

SHADERC=2024.1
SHADERC_GLSLANG=142052fa30f9eca191aa9dcf65359fcaed09eeec
SHADERC_SPIRVHEADERS=5e3ad389ee56fca27c9705d093ae5387ce404df4
SHADERC_SPIRVTOOLS=dd4b663e13c07fea4fbb3f70c1c91c86731099f7

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
926485350139ffb51ef69760db35f78846c805fef3d59bfdcb2fba704663f370  libpng-$LIBPNG.tar.xz
7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c  libwebp-$LIBWEBP.tar.gz
0728800155f3ed0a0c87e03addbd30ecbe374f7b080678bbca1506051d50dec3  $LZ4.tar.gz
f87be7b4dec66db4098e9c167b2aa34e2ca10aeb5443bdde95ae03185ed513e0  $SDL.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
c1800c2ea835801af04a05d4a32321d79a93954ee3ae2172bbeacf13d1f0598c  qtbase-everywhere-src-$QT.tar.xz
2047c6242a57bf97cf40079fa9f91752c137cd9ae84760faa9a2e5e8a440606f  qtimageformats-everywhere-src-$QT.tar.xz
ec359d930c95935ea48af58b100c2f5d0d275968ec8ca1e0e76629b7159215fc  qtsvg-everywhere-src-$QT.tar.xz
fa645589cc3f939022401a926825972a44277dead8ec8607d9f2662e6529c9a4  qttools-everywhere-src-$QT.tar.xz
1d5581ef5fc7c7bc556f2403017983683993bbebfcdf977ef8f180f604668c3f  qttranslations-everywhere-src-$QT.tar.xz
503416fcb04db503bd130e6a49c45e3e546f091e83406f774a0c703130c91805  qtwayland-everywhere-src-$QT.tar.xz
eb3b5f0c16313d34f208d90c2fa1e588a23283eed63b101edd5422be6165d528  shaderc-$SHADERC.tar.gz
aa27e4454ce631c5a17924ce0624eac736da19fc6f5a2ab15a6c58da7b36950f  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
5d866ce34a4b6908e262e5ebfffc0a5e11dd411640b5f24c85a80ad44c0d4697  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
03ee1a2c06f3b61008478f4abe9423454e53e580b9488b47c8071547c6a9db47  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
b8529755b2d54205341766ae168e83177c6120660539f9afba71af6bca4b81ec  KDDockWidgets-$KDDOCKWIDGETS.tar.gz
f49d62709d6bf1808ddc9b8f71e22a755484f75c7bbb0fb368f7fb2ffc7cf645  plutovg-$PLUTOVG.tar.gz
01f8aee511bd587a602a166642a96522cc9522efd1e38c2d00e4fbc0aa22d7a0  plutosvg-$PLUTOSVG.tar.gz
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
	-o "KDDockWidgets-$KDDOCKWIDGETS.tar.gz" "https://github.com/KDAB/KDDockWidgets/archive/v$KDDOCKWIDGETS.tar.gz" \
	-o "plutovg-$PLUTOVG.tar.gz" "https://github.com/sammycage/plutovg/archive/v$PLUTOVG.tar.gz" \
	-o "plutosvg-$PLUTOSVG.tar.gz" "https://github.com/sammycage/plutosvg/archive/v$PLUTOSVG.tar.gz"

shasum -a 256 --check SHASUMS

echo "Building libbacktrace..."
rm -fr "libbacktrace-$LIBBACKTRACE"
unzip "$LIBBACKTRACE.zip"
cd "libbacktrace-$LIBBACKTRACE"
./configure --prefix="$INSTALLDIR"
make
make install
cd ..

echo "Building libpng..."
rm -fr "libpng-$LIBPNG"
tar xf "libpng-$LIBPNG.tar.xz"
cd "libpng-$LIBPNG"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building libjpegturbo..."
rm -fr "libjpeg-turbo-$LIBJPEGTURBO"
tar xf "libjpeg-turbo-$LIBJPEGTURBO.tar.gz"
cd "libjpeg-turbo-$LIBJPEGTURBO"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DENABLE_STATIC=OFF -DENABLE_SHARED=ON -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building LZ4..."
rm -fr "lz4-$LZ4"
tar xf "$LZ4.tar.gz"
cd "lz4-$LZ4"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DLZ4_BUILD_CLI=OFF -DLZ4_BUILD_LEGACY_LZ4C=OFF -B build-dir -G Ninja build/cmake
cmake --build build-dir --parallel
ninja -C build-dir install
cd ..

echo "Building Zstandard..."
rm -fr "zstd-$ZSTD"
tar xf "zstd-$ZSTD.tar.gz"
cd "zstd-$ZSTD"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DZSTD_BUILD_SHARED=ON -DZSTD_BUILD_STATIC=OFF -DZSTD_BUILD_PROGRAMS=OFF -B build -G Ninja build/cmake
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building WebP..."
rm -fr "libwebp-$LIBWEBP"
tar xf "libwebp-$LIBWEBP.tar.gz"
cd "libwebp-$LIBWEBP"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -B build -G Ninja \
  -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
  -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF -DBUILD_SHARED_LIBS=ON
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building SDL..."
rm -fr "$SDL"
tar xf "$SDL.tar.gz"
cd "$SDL"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -G Ninja
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
mkdir build
cd build
../configure -prefix "$INSTALLDIR" -release -dbus-linked -gui -widgets -fontconfig -qt-doubleconversion -ssl -openssl-runtime -opengl desktop -qpa xcb,wayland -xkbcommon -xcb -gtk -- -DFEATURE_dbus=ON -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON
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
mkdir build
cd build
"$INSTALLDIR/bin/qt-configure-module" .. -- -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
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
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DKDDockWidgets_QT6=true -DKDDockWidgets_EXAMPLES=false -DKDDockWidgets_FRONTENDS=qtwidgets -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building PlutoVG..."
rm -fr "plutovg-$PLUTOVG"
tar xf "plutovg-$PLUTOVG.tar.gz"
cd "plutovg-$PLUTOVG"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DPLUTOVG_BUILD_EXAMPLES=OFF -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building PlutoSVG..."
rm -fr "plutosvg-$PLUTOSVG"
tar xf "plutosvg-$PLUTOSVG.tar.gz"
cd "plutosvg-$PLUTOSVG"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DPLUTOSVG_ENABLE_FREETYPE=OFF -DPLUTOSVG_BUILD_EXAMPLES=OFF -B build -G Ninja
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
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
