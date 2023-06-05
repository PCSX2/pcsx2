#!/bin/bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
source "$SCRIPTDIR/functions.sh"

set -e

ARCH=x86_64
KDE_BRANCH=6.5
BRANCH=22.08

# Build packages.
declare -a BUILD_PACKAGES=(
  "flatpak"
  "flatpak-builder"
  "appstream-util"
)

# Flatpak runtimes and SDKs.
declare -a FLATPAK_PACKAGES=(
  "org.kde.Platform/${ARCH}/${KDE_BRANCH}"
  "org.kde.Sdk/${ARCH}/${KDE_BRANCH}"
  "org.freedesktop.Platform.ffmpeg-full/${ARCH}/${BRANCH}"
  "org.freedesktop.Sdk.Extension.llvm16/${ARCH}/${BRANCH}"
)

retry_command sudo apt-get -qq update

# Install packages needed for building
echo "Will install the following packages for building - ${BUILD_PACKAGES[*]}"
retry_command sudo apt-get -y install "${BUILD_PACKAGES[@]}"

sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Install packages needed for building
echo "Will install the following packages for building - ${FLATPAK_PACKAGES[*]}"
retry_command sudo flatpak -y install "${FLATPAK_PACKAGES[@]}"

