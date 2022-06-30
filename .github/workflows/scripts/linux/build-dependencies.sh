#!/bin/bash

set -e
SRCDIR="$HOME"
INSTALLDIR="$HOME/Depends"

QT=6.3.0
SDL=SDL2-2.0.22
PATCHELF_VERSION=0.14.5

if [ "${COMPILER}" = "gcc" ]; then
  export CC=gcc-10; export CXX=g++-10
else
  export CC=clang; export CXX=clang++
fi
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"
export LDFLAGS="-L$INSTALLDIR/lib $LDFLAGS"
export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

export Qt6_DIR="$HOME/Depends/lib/cmake/Qt6"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$HOME/Depends/lib/"

cd "$SRCDIR"

cat > SHASUMS <<EOF
fe7cbf3127882e3fc7259a75a0cb585620272c51745d3852ab9dd87960697f2e  $SDL.tar.gz
b9a46f2989322eb89fa4f6237e20836c57b455aa43a32545ea093b431d982f5c  patchelf-$PATCHELF_VERSION.tar.bz2
b865aae43357f792b3b0a162899d9bf6a1393a55c4e5e4ede5316b157b1a0f99  qtbase-everywhere-src-$QT.tar.xz
d294b029dc2b2d4f65da516fdc3b8088d32643eb7ff77db135a8b9ce904caa37  qtdeclarative-everywhere-src-$QT.tar.xz
3164504d7e3f640439308235739b112605ab5fc9cc517ca0b28f9fb93a8db0e3  qtsvg-everywhere-src-$QT.tar.xz
fce94688ea925782a2879347584991f854630daadba6c52aed6d93e33cd0b19c  qttools-everywhere-src-$QT.tar.xz
e4dd4ef892a34a9514a19238f189a33ed85c76f31dcad6599ced93b1e33440b3  qttranslations-everywhere-src-$QT.tar.xz
e7b567f6e43ffc5918d4aa825ce1eced66a00cb0a87133b2912ba5c1b2a02190  qtwayland-everywhere-src-$QT.tar.xz
EOF

wget -q https://libsdl.org/release/"$SDL".tar.gz \
https://github.com/NixOS/patchelf/releases/download/"$PATCHELF_VERSION/patchelf-$PATCHELF_VERSION.tar.bz2" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtwayland-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtdeclarative-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" 
sha256sum -c SHASUMS

#find sha256sums
sha256sum *.tar*

tar -xf "$SDL.tar.gz"

cd "$SDL"

mkdir build && cd build

cmake -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
-DCMAKE_BUILD_TYPE=MinSizeRel \
-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
-GNinja \
..

ninja -j$(nproc) && ninja install

cd ../..

tar xf patchelf-"$PATCHELF_VERSION".tar.bz2

cd patchelf-"$PATCHELF_VERSION"

./configure --prefix "$INSTALLDIR"
make -j$(nproc) && make install
cd ..

    for f in qt*.tar*; do 
        mkdir "${f/-*/}"
        tar -xf "$f" -C "${f/-*/}" --strip-components=1; 
    done


	echo "Installing Qt Base..."
	cd "qtbase"
	cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -GNinja -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_optimize_size=ON -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF
	ninja -C build -j$(nproc)
	ninja -C build install
	cd ..
	echo "Installing Qt SVG..."
	cd "qtsvg"
	cmake -B build -GNinja -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
	ninja -C build -j$(nproc)
	ninja -C build install
	cd ..
	echo "Installing Qt Tools..."
	cd "qttools"
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
	cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -GNinja -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_assistant=OFF -DFEATURE_pkg_config=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
	ninja -C build -j$(nproc)
	ninja -C build install
	cd ..
	echo "Installing Qt Declarative..."
	cd "qtdeclarative"
	cmake -B build -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -GNinja
	ninja -C build -j$(nproc) 
	ninja -C build install
	cd ..
	echo "Installing Qt Wayland..."
	cd "qtwayland"
	cmake -B build -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -GNinja
	ninja -C build -j$(nproc) 
	ninja -C build install
	cd ..
	echo "Installing Qt Translations..."
	cd "qttranslations"
	cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -GNinja
	ninja -C build -j$(nproc) 
	ninja -C build install
	cd ..
