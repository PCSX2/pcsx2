#!/usr/bin/env bash

set -e

if [ "$#" -ne 1 ]; then
    echo "Syntax: $0 <output directory>"
    exit 1
fi

# The bundled ffmpeg has a lot of things disabled to reduce code size.
# Users may want to use system ffmpeg for additional features
: ${BUILD_FFMPEG:=0}

SCRIPTDIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))
NPROCS="$(getconf _NPROCESSORS_ONLN)"
INSTALLDIR="$1"
if [ "${INSTALLDIR:0:1}" != "/" ]; then
	INSTALLDIR="$PWD/$INSTALLDIR"
fi

QT=6.11.0
QTAPNG=1.3.0

FFMPEG=8.1
LIBBACKTRACE=ad106d5fdd5d960bd33fae1c48a351af567fd075
LIBJPEGTURBO=3.1.4.1
LIBPNG=1.6.58
LIBWEBP=1.6.0
NVENC=13.0.19.0
SDL=SDL3-3.4.8
LZ4=1.10.0
VULKAN=1.4.328.1
ZSTD=1.5.7
KDDOCKWIDGETS=2.4.0
PLUTOVG=1.3.2
PLUTOSVG=0.0.7
RAPIDYAML=0.12.1

SHADERC=2026.2
SHADERC_GLSLANG=275822a6261ee689aadb1da5f09a0ec2f058685c
SHADERC_SPIRVHEADERS=58006c901d1d5c37dece6b6610e9af87fa951375
SHADERC_SPIRVTOOLS=6337eb62cadd7d124ac6789bf39c0f71148f0a73

mkdir -p deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

grep . > SHASUMS <<EOF
231ad85979864d914dc9568a1b71c91d6cf20d7b2021d059103bf0eb51cb755e  qtbase-everywhere-src-$QT.tar.xz
d3adb02ac5e2fe24068dbdaee0d7cc68cc3fa8553291c1bfce77c9fe8e940cc8  qtimageformats-everywhere-src-$QT.tar.xz
dfa8d653be07087d9407ed4a4ebae847f8953e0b7abd829f089803ab652a30e6  qtsvg-everywhere-src-$QT.tar.xz
cfb1993d7a10848965b01b9cf33a54b8a4ba4e5e3a6d28d59483e73f10d9fc76  qttools-everywhere-src-$QT.tar.xz
54f48b2fe4316892ff930195f170a5385644acc7393505f3155c066b8e1ffe56  qttranslations-everywhere-src-$QT.tar.xz
e710e6e760f92922b86e4dd68f6bbe94ef6510919519d1b0068e874b5ad84d37  qtwayland-everywhere-src-$QT.tar.xz
f1d3be3489f758efe1a8f12118a212febbe611aa670af32e0159fa3c1feab2a6  QtApng-$QTAPNG.tar.gz

b072aed6871998cce9b36e7774033105ca29e33632be5b6347f3206898e0756a  ffmpeg-$FFMPEG.tar.xz
96e5c2d7f2c482a60d5804da48a2eb9a0db0719b2c65dcc169fbfdcf37f3a45d  libbacktrace-$LIBBACKTRACE.tar.gz
ecae8008e2cc9ade2f2c1bb9d5e6d4fb73e7c433866a056bd82980741571a022  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
28eb403f51f0f7405249132cecfe82ea5c0ef97f1b32c5a65828814ae0d34775  libpng-$LIBPNG.tar.xz
e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564  libwebp-$LIBWEBP.tar.gz
e9fff7467fb60f037e6708da18b25560649e4c63edc2a69bb871b960d9cbfbba  $SDL.tar.gz
eee7dea22ed502868017971c86c63c4ed1e6085de0baebfdcc3d3322f00f3eb0  libpng-$LIBPNG-apng.patch.gz
537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b  lz4-$LZ4.tar.gz
13da39edb3a40ed9713ae390ca89faa2f1202c9dda869ef306a8d4383e242bee  nv-codec-headers-$NVENC.tar.gz
c465aa56757e7746ac707f582b6e2d51546569a4a2488c1172fb543aa5fdfc2c  vulkan-sdk-$VULKAN.tar.gz
eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3  zstd-$ZSTD.tar.gz
51dbf24fe72e43dd7cb9a289d3cab47112010f1a2ed69b6fc8ac0dff31991ed2  KDDockWidgets-$KDDOCKWIDGETS.tar.gz
7bd4e79ce18b1d47517e7e91fbb7cf19d4f01942804a519bc7c0bf32b6325dd5  plutovg-$PLUTOVG.tar.gz
78561b571ac224030cdc450ca2986b4de915c2ba7616004a6d71a379bffd15f3  plutosvg-$PLUTOSVG.tar.gz
e9efcdd17f86287748793cf21d106e461fcad8d103a3e5a23632afe93828660d  rapidyaml-$RAPIDYAML-src.tgz

f924178e75e3293082481b25ed64d5e48a795b479dac3bd3c83d23070855df42  shaderc-$SHADERC.tar.gz
971848a1cc639ce8dc244e778b17efe0f690e32ac398a75e31d1c67ad06d3e0a  shaderc-glslang-$SHADERC_GLSLANG.tar.gz
a6cb1b300bb8171795e116457e858e555334749f9cacaed8068ae0ef8681110c  shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz
e156be0bd81c8812f1bff8e520422bfa9df61b3045587b9eb483185f1074a7b2  shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz
EOF

if ! shasum -sa 256 --check SHASUMS 2> /dev/null; then
	curl -L \
		-O "https://github.com/ianlancetaylor/libbacktrace/archive/$LIBBACKTRACE/libbacktrace-$LIBBACKTRACE.tar.gz" \
		-O "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO/libjpeg-turbo-$LIBJPEGTURBO.tar.gz" \
		-O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
		-O "https://download.sourceforge.net/libpng-apng/libpng-$LIBPNG-apng.patch.gz" \
		-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
		-O "https://github.com/lz4/lz4/releases/download/v$LZ4/lz4-$LZ4.tar.gz" \
		-O "https://libsdl.org/release/$SDL.tar.gz" \
		-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz" \
		-O "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-$VULKAN.tar.gz" \
		-O "https://github.com/FFmpeg/nv-codec-headers/releases/download/n$NVENC/nv-codec-headers-$NVENC.tar.gz" \
		-O "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \
		-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtwayland-everywhere-src-$QT.tar.xz" \
		-O "https://github.com/jurplel/QtApng/archive/$QTAPNG/QtApng-$QTAPNG.tar.gz" \
		-O "https://github.com/google/shaderc/archive/v$SHADERC/shaderc-$SHADERC.tar.gz" \
		-O "https://github.com/KhronosGroup/glslang/archive/$SHADERC_GLSLANG/shaderc-glslang-$SHADERC_GLSLANG.tar.gz" \
		-O "https://github.com/KhronosGroup/SPIRV-Headers/archive/$SHADERC_SPIRVHEADERS/shaderc-spirv-headers-$SHADERC_SPIRVHEADERS.tar.gz" \
		-O "https://github.com/KhronosGroup/SPIRV-Tools/archive/$SHADERC_SPIRVTOOLS/shaderc-spirv-tools-$SHADERC_SPIRVTOOLS.tar.gz" \
		-O "https://github.com/KDAB/KDDockWidgets/archive/v$KDDOCKWIDGETS/KDDockWidgets-$KDDOCKWIDGETS.tar.gz" \
		-O "https://github.com/sammycage/plutovg/archive/v$PLUTOVG/plutovg-$PLUTOVG.tar.gz" \
		-O "https://github.com/sammycage/plutosvg/archive/v$PLUTOSVG/plutosvg-$PLUTOSVG.tar.gz" \
		-O "https://github.com/biojppm/rapidyaml/releases/download/v$RAPIDYAML/rapidyaml-$RAPIDYAML-src.tgz"
fi

shasum -a 256 --check --strict SHASUMS

if [ "$BUILD_FFMPEG" -ne 0 ]; then
	echo "Installing vulkan headers..."
	rm -fr "Vulkan-Headers-vulkan-sdk-$VULKAN"
	tar xf "vulkan-sdk-$VULKAN.tar.gz"
	cd "Vulkan-Headers-vulkan-sdk-$VULKAN"
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR"
	make -C build install
	cd ..

	echo "Installing nvenc headers..."
	rm -fr "nv-codec-headers-$NVENC"
	tar xf "nv-codec-headers-$NVENC.tar.gz"
	make -C "nv-codec-headers-$NVENC" PREFIX="$INSTALLDIR" install

	echo "Installing FFmpeg..."
	rm -fr "ffmpeg-$FFMPEG"
	tar xf "ffmpeg-$FFMPEG.tar.xz"
	cd "ffmpeg-$FFMPEG"
	CFLAGS="-Os $CFLAGS" CXXFLAGS="-Os $CXXFLAGS" \
		./configure --prefix="$INSTALLDIR" \
		--disable-all --disable-autodetect --disable-static --enable-shared \
		--enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
		--enable-gpl --enable-libx264 --enable-libopus --enable-vulkan --enable-ffnvcodec --enable-nvenc --enable-vaapi --enable-libvpl \
		--enable-encoder=ffv1,qtrle,libx264*,aac,flac,libopus,pcm_s16be,pcm_s16le,*_vulkan,*_qsv,*_nvenc,*_vaapi \
		--enable-muxer=avi,matroska,mov,mp3,mp4,wav \
		--enable-protocol=file
	make "-j$NPROCS"
	make install
	cd ..
fi

echo "Building libbacktrace..."
rm -fr "libbacktrace-$LIBBACKTRACE"
tar xf "libbacktrace-$LIBBACKTRACE.tar.gz"
cd "libbacktrace-$LIBBACKTRACE"
./configure --prefix="$INSTALLDIR" --with-pic
make
make install
cd ..

echo "Building libpng..."
rm -fr "libpng-$LIBPNG"
tar xf "libpng-$LIBPNG.tar.xz"
gzip -kd -f "libpng-$LIBPNG-apng.patch.gz"
cd "libpng-$LIBPNG"
patch -p1 < "../libpng-$LIBPNG-apng.patch"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DBUILD_SHARED_LIBS=ON -DPNG_TESTS=OFF -DPNG_STATIC=OFF -DPNG_SHARED=ON -DPNG_TOOLS=OFF -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building libjpegturbo..."
rm -fr "libjpeg-turbo-$LIBJPEGTURBO"
tar xf "libjpeg-turbo-$LIBJPEGTURBO.tar.gz"
cd "libjpeg-turbo-$LIBJPEGTURBO"
# On non debian or debian based Linux systems, libjpeg-turbo will set CMAKE_INSTALL_DEFAULT_LIBDIR "lib64" (or libx32)
# That will prevent CMake from finding the deps libjpeg later on. if we set CMAKE_INSTALL_DEFAULT_LIBDIR, libjpeg-turbo will leave it as is, so set it to "lib"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DENABLE_STATIC=OFF -DENABLE_SHARED=ON -DCMAKE_INSTALL_DEFAULT_LIBDIR="lib" -B build -G Ninja
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
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_VIDEO=OFF -DSDL_POWER=OFF -DSDL_SENSOR=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_UNIX_CONSOLE_BUILD=ON -G Ninja
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
../configure -prefix "$INSTALLDIR" -release -dbus-linked -gui -widgets -fontconfig -qt-doubleconversion -ssl -openssl-runtime -opengl desktop -qpa xcb,wayland -xkbcommon -xcb -- --log-level=STATUS -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DFEATURE_dbus=ON -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_system_png=ON -DFEATURE_system_jpeg=ON -DFEATURE_system_zlib=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=ON -DFEATURE_gtk3=OFF
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

echo "Building Qt APNG..."
rm -fr "QtApng-$QTAPNG"
tar xf "QtApng-$QTAPNG.tar.gz"
cd "QtApng-$QTAPNG"
patch -p1 < "$SCRIPTDIR/../common/qtapng-cmake.patch"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -B build -G Ninja
cmake --build build --parallel
ninja -C build install
cd ..

echo "Building KDDockWidgets..."
rm -fr "KDDockWidgets-$KDDOCKWIDGETS"
tar xf "KDDockWidgets-$KDDOCKWIDGETS.tar.gz"
cd "KDDockWidgets-$KDDOCKWIDGETS"
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

echo "Building RapidYAML..."
rm -fr "rapidyaml-$RAPIDYAML-src"
tar xf "rapidyaml-$RAPIDYAML-src.tgz"
cd "rapidyaml-$RAPIDYAML-src"
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DBUILD_SHARED_LIBS=ON -B build -G Ninja
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
