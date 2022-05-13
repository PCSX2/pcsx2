#!/bin/bash

set -e

export MACOSX_DEPLOYMENT_TARGET=10.13
INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.0.22
PNG=1.6.37
JPG=9e
SAMPLERATE=libsamplerate-0.1.9
PORTAUDIO=pa_stable_v190700_20210406
SOUNDTOUCH=soundtouch-2.3.1
WXWIDGETS=3.1.6

mkdir deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib -dead_strip $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"

cat > SHASUMS <<EOF
fe7cbf3127882e3fc7259a75a0cb585620272c51745d3852ab9dd87960697f2e  $SDL.tar.gz
505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca  libpng-$PNG.tar.xz
4077d6a6a75aeb01884f708919d25934c93305e49f7e3f36db9129320e6f4f3d  jpegsrc.v$JPG.tar.gz
0a7eb168e2f21353fb6d84da152e4512126f7dc48ccb0be80578c565413444c1  $SAMPLERATE.tar.gz
47efbf42c77c19a05d22e627d42873e991ec0c1357219c0d74ce6a2948cb2def  $PORTAUDIO.tgz
6900996607258496ce126924a19fe9d598af9d892cf3f33d1e4daaa9b42ae0b1  $SOUNDTOUCH.tar.gz
4980e86c6494adcd527a41fc0a4e436777ba41d1893717d7b7176c59c2061c25  wxWidgets-$WXWIDGETS.tar.bz2
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$PNG/libpng-$PNG.tar.xz" \
	-O "https://www.ijg.org/files/jpegsrc.v$JPG.tar.gz" \
	-O "http://www.mega-nerd.com/SRC/$SAMPLERATE.tar.gz" \
	-O "http://files.portaudio.com/archives/$PORTAUDIO.tgz" \
	-O "https://www.surina.net/soundtouch/$SOUNDTOUCH.tar.gz" \
	-O "https://github.com/wxWidgets/wxWidgets/releases/download/v$WXWIDGETS/wxWidgets-$WXWIDGETS.tar.bz2" \

shasum -a 256 --check SHASUMS

echo "Installing SDL..."
tar xf "$SDL.tar.gz"
cd "$SDL"
./configure --prefix "$INSTALLDIR" --without-x
make "-j$NPROCS"
make install
cd ..

echo "Installing libpng..."
tar xf "libpng-$PNG.tar.xz"
cd "libpng-$PNG"
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking
make "-j$NPROCS"
make install
cd ..

echo "Installing libjpeg..."
tar xf "jpegsrc.v$JPG.tar.gz"
cd "jpeg-$JPG"
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking
make "-j$NPROCS"
make install
cd ..

echo "Installing libsamplerate..."
tar xf "$SAMPLERATE.tar.gz"
cd "$SAMPLERATE"
sed -i "" "s/Carbon.h/Carbon\\/Carbon.h/" examples/audio_out.c
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking --disable-sndfile
make "-j$NPROCS"
make install
cd ..

echo "Installing portaudio..."
tar xf "$PORTAUDIO.tgz"
cd portaudio
./configure --prefix "$INSTALLDIR" --enable-mac-universal=no
make "-j$NPROCS"
make install
cd ..

echo "Installing soundtouch..."
tar xf "$SOUNDTOUCH.tar.gz"
cd "$SOUNDTOUCH"
cmake -B build -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing wx..."
tar xf "wxWidgets-$WXWIDGETS.tar.bz2"
cd "wxWidgets-$WXWIDGETS"
./configure --prefix "$INSTALLDIR" --with-macosx-version-min="$MACOSX_DEPLOYMENT_TARGET" --enable-clipboard --enable-dnd --enable-std_string --with-cocoa --with-libiconv --with-libjpeg --with-libpng --with-zlib --without-libtiff --without-regex
make "-j$NPROCS"
make install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
