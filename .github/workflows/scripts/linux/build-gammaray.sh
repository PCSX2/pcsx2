#!/usr/bin/env bash

set -e

if [ "$#" -ne 2 ]; then
	echo "Syntax: $0 <deps directory> <output directory>"
	exit 1
fi

DEPSDIR=$(realpath "$1")
INSTALLDIR=$(realpath "$2")

if [ ! -d "$DEPSDIR/include/QtCore" ]; then
	echo "Error: The build-dependencies-qt.sh script must be run on the deps directory first."
	exit 1
fi

GAMMARAY=master

mkdir -p gammaray-build
cd gammaray-build

echo "Downloading..."
curl -L -o "GammaRay-$GAMMARAY.tar.gz" https://github.com/KDAB/GammaRay/archive/$GAMMARAY.tar.gz

rm -fr "GammaRay-$GAMMARAY"

echo "Extracting..."
tar xf "GammaRay-$GAMMARAY.tar.gz"

cd "GammaRay-$GAMMARAY"
mkdir build
cd build

echo "Configuring..."
cmake -DCMAKE_PREFIX_PATH="$DEPSDIR" -G Ninja -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DGAMMARAY_BUILD_DOCS=false ..

echo "Building..."
cmake --build . --parallel

echo "Installing..."
cmake --build . --target install

cd ../..

echo "Cleaning up..."
cd ..
rm -r gammaray-build
