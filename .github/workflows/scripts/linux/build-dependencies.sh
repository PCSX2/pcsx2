#!/bin/bash
set -e
SRCDIR="$HOME"
INSTALLDIR="$HOME/Depends"
SDL=SDL2-2.0.22
PATCHELF_VERSION=0.14.5


export CXX=/usr/bin/clang++
export C=/usr/bin/clang
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"
export LDFLAGS="-L$INSTALLDIR/lib $LDFLAGS"
export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

cd "$SRCDIR"

cat > SHASUMS <<EOF
fe7cbf3127882e3fc7259a75a0cb585620272c51745d3852ab9dd87960697f2e  $SDL.tar.gz
b9a46f2989322eb89fa4f6237e20836c57b455aa43a32545ea093b431d982f5c  patchelf-$PATCHELF_VERSION.tar.bz2
EOF

wget https://libsdl.org/release/$SDL.tar.gz \
https://github.com/NixOS/patchelf/releases/download/"$PATCHELF_VERSION/patchelf-$PATCHELF_VERSION.tar.bz2"
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

