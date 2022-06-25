#!/bin/bash

set -e
SRCDIR="$HOME"
INSTALLDIR="$HOME/Depends"

QT=6.2.4
SDL=SDL2-2.0.22
PATCHELF_VERSION=0.14.5

export CC=gcc-10; export CXX=g++-10
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"
export LDFLAGS="-L$INSTALLDIR/lib $LDFLAGS"
export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

export Qt6_DIR="$HOME/Depends/lib/cmake/Qt6"
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/Depends/lib/

cd "$SRCDIR"

cat > SHASUMS <<EOF
fe7cbf3127882e3fc7259a75a0cb585620272c51745d3852ab9dd87960697f2e  $SDL.tar.gz
b9a46f2989322eb89fa4f6237e20836c57b455aa43a32545ea093b431d982f5c  patchelf-$PATCHELF_VERSION.tar.bz2
d9924d6fd4fa5f8e24458c87f73ef3dfc1e7c9b877a5407c040d89e6736e2634  qtbase-everywhere-src-$QT.tar.xz
23ec4c14259d799bb6aaf1a07559d6b1bd2cf6d0da3ac439221ebf9e46ff3fd2  qtsvg-everywhere-src-$QT.tar.xz
17f40689c4a1706a1b7db22fa92f6ab79f7b698a89e100cab4d10e19335f8267  qttools-everywhere-src-$QT.tar.xz
bd1aac74a892c60b2f147b6d53bb5b55ab7a6409e63097d38198933f8024fa51  qttranslations-everywhere-src-$QT.tar.xz
ef7217d25608b6f3f3cd92fa783c8bfca50821906729887fe9d392db7e74920e qtwayland-everywhere-src-$QT.tar.xz
fa6a8b5bfe43203daf022b7949c7874240ec28b9c4b7ebc52b2e5ea440e6e94f qtdeclarative-everywhere-src-$QT.tar.xz
EOF

wget https://libsdl.org/release/$SDL.tar.gz \
https://github.com/NixOS/patchelf/releases/download/"$PATCHELF_VERSION/patchelf-$PATCHELF_VERSION.tar.bz2" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtwayland-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qtdeclarative-everywhere-src-$QT.tar.xz" \
https://download.qt.io/official_releases/qt/"${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" 
sha256sum -c SHASUMS

tar -xvf "$SDL.tar.gz"

cd "$SDL"

mkdir build && cd build

cmake -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
-DCMAKE_BUILD_TYPE=MinSizeRel \
-DCMAKE_PREFIX_PATH="$INSTALLDIR" \
-GNinja \
..

ninja -j$(nproc) && ninja install

cd ../..

tar xvf patchelf-$PATCHELF_VERSION.tar.bz2

cd patchelf-$PATCHELF_VERSION

./configure --prefix "$INSTALLDIR"
make -j$(nproc) && make install
cd ..

    for f in qt*.tar*; do 
        mkdir "${f/-*/}"
        tar -xvf "$f" -C "${f/-*/}" --strip-components=1; 
    done


	echo "Installing Qt Base..."
	cd "qtbase"
	cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -GNinja -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_optimize_size=ON -DFEATURE_framework=OFF  -DFEATURE_icu=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF
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
	echo "Installing Qt Wayland..."
	cd "qtwayland"
	cmake -B build -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -GNinja
	ninja -C build -j$(nproc) 
	ninja -C build install
	cd ..
	echo "Installing Qt Declarative..."
	cd "qtdeclarative"
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