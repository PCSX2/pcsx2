#!/bin/bash

set -e

# Packages - Build Environment
declare -a BUILD_PACKAGES=(
  "ccache"
  "cmake"
  "g++-8-multilib"
  "ninja-build"
)

declare -a GCC_PACKAGES=(
  # Nothing Unique Needed
)

declare -a CLANG_PACKAGES=(
  "clang-format"
  "clang-tidy"
  "clang-tools"
  "clang"
  "clangd-10"
  "libc++-dev"
  "libc++1"
  "libc++abi-dev"
  "libc++abi1"
  "libclang-dev"
  "libclang1"
  "liblldb-10-dev"
  "libllvm-10-ocaml-dev"
  "libomp-dev"
  "libomp5"
  "lld"
  "lldb"
  "llvm-dev"
  "llvm-runtime"
  "llvm"
  "python3-clang-10"
)

# Packages - PCSX2
declare -a PCSX2_PACKAGES=(
  "curl"
  "fuse"
  "gettext"
  "libaio-dev"
  "libasound2-dev"
  "libatk1.0-dev"
  "libatk-bridge2.0-dev"
  "libbz2-dev"
  "libcairo2-dev"
  "libcggl"
  "libdbus-1-dev"
  "libegl1-mesa-dev"
  "libfontconfig1-dev"
  "libgdk-pixbuf2.0-dev"
  "libgirepository-1.0-1"
  "libgl-dev"
  "libgl1-mesa-dev"
  "libgl1-mesa-dri"
  "libgl1"
  "libgles2-mesa-dev"
  "libglew-dev"
  "libglib2.0-dev"
  "libglu1-mesa-dev"
  "libglu1-mesa"
  "libglvnd-dev"
  "libglx-mesa0"
  "libglx0"
  "libgtk-3-dev"
  "libgtk2.0-dev"
  "libharfbuzz-dev"
  "libibus-1.0-dev"
  "libjack-jackd2-dev"
  "libjpeg-dev"
  "libllvm10"
  "liblzma-dev"
  "liblzma5"
  "libpango1.0-dev"
  "libpcap0.8-dev"
  "libpng-dev"
  "libportaudiocpp0"
  "libpulse-dev"
  "librsvg2-dev"
  "libsdl1.2-dev"
  "libsdl2-dev"
  "libsamplerate0-dev"
  "libsoundtouch-dev"
  "libwxgtk3.0-dev"
  "libwxgtk3.0-gtk3-0v5"
  "libwxgtk3.0-gtk3-dev"
  "libx11-xcb-dev"
  "libxext-dev"
  "libxft-dev"
  "libxml2-dev"
  "nvidia-cg-toolkit"
  "pkg-config"
  "portaudio19-dev"
  "python"
  "zlib1g-dev"
)

# - https://github.com/actions/virtual-environments/blob/main/images/linux/Ubuntu2004-README.md
ARCH=""
echo "${PLATFORM}"
if [ "${PLATFORM}" == "x86" ]; then
  ARCH=":i386"
  sudo dpkg --add-architecture i386
fi

sudo apt-get -qq update

# Install packages needed for building
BUILD_PACKAGE_STR=""
for i in "${BUILD_PACKAGES[@]}"; do
  BUILD_PACKAGE_STR="${BUILD_PACKAGE_STR} ${i}"
done

if [ "${COMPILER}" = "gcc" ]; then
  for i in "${GCC_PACKAGES[@]}"; do
    BUILD_PACKAGE_STR="${BUILD_PACKAGE_STR} ${i}"
  done
else
  for i in "${CLANG_PACKAGES[@]}"; do
    BUILD_PACKAGE_STR="${BUILD_PACKAGE_STR} ${i}"
  done
fi

echo "Will install the following packages for building - ${BUILD_PACKAGE_STR}"
#sudo apt remove gcc-9 g++-9
sudo apt-get -y install ${BUILD_PACKAGE_STR}

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 10
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30

# Install packages needed by pcsx2
PCSX2_PACKAGES_STR=""
for i in "${PCSX2_PACKAGES[@]}"; do
  PCSX2_PACKAGES_STR="${PCSX2_PACKAGES_STR} ${i}${ARCH}"
done
if [ "${PLATFORM}" == "x86" ]; then
echo "Installing workaround attempt"
sudo apt-get -y install libgcc-s1:i386
fi
echo "Will install the following packages for pcsx2 - ${PCSX2_PACKAGES_STR}"
sudo apt-get -y install ${PCSX2_PACKAGES_STR}

cd /tmp
curl -sSfLO https://github.com/NixOS/patchelf/releases/download/0.12/patchelf-0.12.tar.bz2        
tar xvf patchelf-0.12.tar.bz2
cd patchelf-0.12*/ 
./configure
make && sudo make install
