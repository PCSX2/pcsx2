#!/bin/bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
source "$SCRIPTDIR/functions.sh"

set -e

# Packages - Build and Qt
declare -a BUILD_PACKAGES=(
	"build-essential"
	"g++"
	"git"
	"cmake"
	"ccache"
	"ninja-build"
	"patchelf"
	"libfuse2"
	"libglib2.0-dev"
	"libfontconfig1-dev"
	"libharfbuzz-dev"
	"libjpeg-dev"
	"libpng-dev"
	"libfreetype-dev"
	"libinput-dev"
	"libxcb-*-dev"
	"libxkbcommon-dev"
	"libxkbcommon-x11-dev"
	"libxrender-dev"
	"libwayland-dev"
	"libgl1-mesa-dev"
	"libegl-dev"
	"libegl1-mesa-dev"
	"libgl1-mesa-dev"
	"libssl-dev"
)

# Packages - PCSX2
declare -a PCSX2_PACKAGES=(
	"extra-cmake-modules"
	"libaio-dev"
	"libasound2-dev"
	"libbz2-dev"
	"libcurl4-openssl-dev"
	"libegl1-mesa-dev"
	"libgl1-mesa-dev"
	"libgtk-3-dev"
	"libharfbuzz-dev"
	"libjpeg-dev"
	"liblzma-dev"
	"libpcap0.8-dev"
	"libpng-dev"
	"libpulse-dev"
	"librsvg2-dev"
	"libsamplerate0-dev"
	"libudev-dev"
	"libx11-xcb-dev"
	"libavcodec-dev"
	"libavformat-dev"
	"libavutil-dev"
	"libswresample-dev"
	"libswscale-dev"
	"pkg-config"
	"zlib1g-dev"
)

if [ "${COMPILER}" = "clang" ]; then
	BUILD_PACKAGES+=("llvm-16" "lld-16" "clang-16")

	# Ubuntu 22.04 doesn't ship with LLVM 16, so we need to pull it from the llvm.org repos.
	retry_command wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
	sudo apt-add-repository -n 'deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-16 main'
fi

retry_command sudo apt-get -qq update && break

# Install packages needed for building
echo "Will install the following packages for building - ${BUILD_PACKAGES[*]}"
retry_command sudo apt-get -y install "${BUILD_PACKAGES[@]}"

# Install packages needed by pcsx2
PCSX2_PACKAGES=("${PCSX2_PACKAGES[@]}")
echo "Will install the following packages for pcsx2 - ${PCSX2_PACKAGES[*]}"
retry_command sudo apt-get -y install "${PCSX2_PACKAGES[@]}"
