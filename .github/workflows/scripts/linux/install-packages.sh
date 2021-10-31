#!/bin/bash

set -e

# Packages - Build Environment
declare -a BUILD_PACKAGES=(
  "ccache"
  "cmake"
  "ninja-build"
)

# Packages - PCSX2
declare -a PCSX2_PACKAGES=(
  "libaio-dev"
  "libbz2-dev"
  "libegl1-mesa-dev"
  "libgdk-pixbuf2.0-dev"
  "libgl1-mesa-dev"
  "libgtk-3-dev"
  "libharfbuzz-dev"
  "libjpeg-dev"
  "liblzma-dev"
  "libpcap0.8-dev"
  "libpng-dev"
  "libpulse-dev"
  "librsvg2-dev"
  "libsdl2-dev"
  "libsamplerate0-dev"
  "libsoundtouch-dev"
  "libwxgtk3.0-gtk3-dev"
  "libx11-xcb-dev"
  "libxml2-dev"
  "pkg-config"
  "portaudio19-dev"
  "zlib1g-dev"
)

if [ "${COMPILER}" = "gcc" ]; then
  BUILD_PACKAGES+=("g++-10-multilib")
else
  BUILD_PACKAGES+=("clang-9")
  PCSX2_PACKAGES+=("libstdc++-10-dev")
fi

# - https://github.com/actions/virtual-environments/blob/main/images/linux/Ubuntu2004-README.md
ARCH=""
echo "${PLATFORM}"
if [ "${PLATFORM}" == "x86" ]; then
  ARCH=":i386"
  sudo dpkg --add-architecture i386
fi

sudo apt-get -qq update

# Install packages needed for building
echo "Will install the following packages for building - ${BUILD_PACKAGES[*]}"
#sudo apt remove gcc-9 g++-9
sudo apt-get -y install "${BUILD_PACKAGES[@]}"

# Install packages needed by pcsx2
PCSX2_PACKAGES=("${PCSX2_PACKAGES[@]/%/"${ARCH}"}")
echo "Will install the following packages for pcsx2 - ${PCSX2_PACKAGES[*]}"
sudo apt-get -y install "${PCSX2_PACKAGES[@]}"

cd /tmp
curl -sSfLO https://github.com/NixOS/patchelf/releases/download/0.12/patchelf-0.12.tar.bz2        
tar xvf patchelf-0.12.tar.bz2
cd patchelf-0.12*/ 
./configure
make && sudo make install
