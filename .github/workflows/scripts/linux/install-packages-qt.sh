#!/bin/bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
source "$SCRIPTDIR/functions.sh"

set -e

# Packages - Build and Qt
declare -a BUILD_PACKAGES=(
	"build-essential"
	"git"
	"cmake"
	"ccache"
	"ninja-build"
	"libclang-dev" # Qt goes hunting for libclang-11 specifically.
	"libclang-11-dev"
	"libclang-12-dev"
	"patchelf"
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
	"libsoundtouch-dev"
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

if [ "${COMPILER}" = "gcc" ]; then
	BUILD_PACKAGES+=("g++-10")
else
	BUILD_PACKAGES+=("llvm-12" "lld-12" "clang-12")
fi

retry_command sudo apt-get -qq update && break

# Install packages needed for building
echo "Will install the following packages for building - ${BUILD_PACKAGES[*]}"
retry_command sudo apt-get -y install "${BUILD_PACKAGES[@]}"

# Install packages needed by pcsx2
PCSX2_PACKAGES=("${PCSX2_PACKAGES[@]}")
echo "Will install the following packages for pcsx2 - ${PCSX2_PACKAGES[*]}"
retry_command sudo apt-get -y install "${PCSX2_PACKAGES[@]}"
