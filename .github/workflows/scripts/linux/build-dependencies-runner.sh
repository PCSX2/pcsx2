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

FREETYPE=2.14.1
HARFBUZZ=12.0.0
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075
LIBPNG=1.6.50
LIBWEBP=1.6.0
SDL=SDL3-3.2.22
LZ4=1.10.0
ZSTD=1.5.7
PLUTOVG=1.3.1
PLUTOSVG=0.0.7

SHADERC=2025.3
SHADERC_GLSLANG=efd24d75bcbc55620e759f6bf42c45a32abac5f8
SHADERC_SPIRVHEADERS=2a611a970fdbc41ac2e3e328802aed9985352dca
SHADERC_SPIRVTOOLS=33e02568181e3312f49a3cf33df470bf96ef293a

mkdir -p deps-build
cd deps-build

cat > SHASUMS <<EOF
32427e8c471ac095853212a37aef816c60b42052d4d9e48230bab3bdf2936ccc  freetype-$FREETYPE.tar.xz
c4a398539c3e0fdc9a82dfe7824d0438cae78c1e2124e7c6ada3dfa600cdb6c8  harfbuzz-$HARFBUZZ.tar.gz
fd6f417fe9e3a071cf1424a5152d926a34c4a3c5070745470be6cf12a404ed79  $LIBBACKTRACE.zip
4df396518620a7aa3651443e87d1b2862e4e88cad135a8b93423e01706232307  libpng-$LIBPNG.tar.xz
e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564  libwebp-$LIBWEBP.tar.gz
f29d00cbcee273c0a54f3f32f86bf5c595e8823a96b1d92a145aac40571ebfcc  $SDL.tar.gz
687ddc0c7cb128a3ea58e159b5129252537c27ede0c32a93f11f03127f0c0165  libpng-$LIBPNG-apng.patch.gz
537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b  lz4-$LZ4.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
a8e4a25e5c2686fd36981e527ed05e451fcfc226bddf350f4e76181371190937  shaderc-$SHADERC.tar.gz
9427deccbdf4bde6a269938df38c6bd75247493786a310d8d733a2c82065ef47  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
c2225a49c3d7efa5c4f4ce4a6b42081e6ea3daca376f3353d9d7c2722d77a28a  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
44d1005880c583fc00a0fb41c839214c68214b000ea8dcb54d352732fee600ff  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
bea672eb96ee36c2cbeb911b9bac66dfe989b3ad9a9943101e00aeb2df2aefdb  plutovg-$PLUTOVG.tar.gz
78561b571ac224030cdc450ca2986b4de915c2ba7616004a6d71a379bffd15f3  plutosvg-$PLUTOSVG.tar.gz
EOF

curl -L \
        -o "freetype-$FREETYPE.tar.xz" "https://sourceforge.net/projects/freetype/files/freetype2/$FREETYPE/freetype-$FREETYPE.tar.xz/download" \
        -o "harfbuzz-$HARFBUZZ.tar.gz" "https://github.com/harfbuzz/harfbuzz/archive/refs/tags/$HARFBUZZ.tar.gz" \
        -O "https://github.com/ianlancetaylor/libbacktrace/archive/$LIBBACKTRACE.zip" \
        -O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
        -O "https://download.sourceforge.net/libpng-apng/libpng-$LIBPNG-apng.patch.gz" \
        -O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
        -O "https://github.com/lz4/lz4/releases/download/v$LZ4/lz4-$LZ4.tar.gz" \
        -O "https://libsdl.org/release/$SDL.tar.gz" \
        -O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
        -o "shaderc-$SHADERC.tar.gz" "https://github.com/google/shaderc/archive/refs/tags/v$SHADERC.tar.gz" \
        -o "shaderc-glslang-$SHADERC_GLSLANG.tar.gz" "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG.tar.gz" \
        -o "shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS.tar.gz" \
        -o "shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS.tar.gz" \
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
gunzip -d -f "libpng-$LIBPNG-apng.patch.gz"
cd "libpng-$LIBPNG"
patch -p1 < "../libpng-$LIBPNG-apng.patch"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building LZ4..."
rm -fr "lz4-$LZ4"
tar xf "lz4-$LZ4.tar.gz"
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

echo "Building FreeType without HarfBuzz..."
rm -fr "freetype-$FREETYPE"
tar xf "freetype-$FREETYPE.tar.xz"
cd "freetype-$FREETYPE"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_DISABLE_HARFBUZZ=TRUE -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building HarfBuzz..."
rm -fr "harfbuzz-$HARFBUZZ"
tar xf "harfbuzz-$HARFBUZZ.tar.gz"
cd "harfbuzz-$HARFBUZZ"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DHB_BUILD_UTILS=OFF -DHB_HAVE_FREETYPE=ON -B build -G Ninja
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

echo "Building FreeType with HarfBuzz..."
rm -fr "freetype-$FREETYPE"
tar xf "freetype-$FREETYPE.tar.xz"
cd "freetype-$FREETYPE"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_REQUIRE_HARFBUZZ=TRUE -B build -G Ninja
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
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DPLUTOSVG_ENABLE_FREETYPE=ON -DPLUTOSVG_BUILD_EXAMPLES=OFF -B build -G Ninja
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
