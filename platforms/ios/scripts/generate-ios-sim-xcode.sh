#!/usr/bin/env bash
set -euo pipefail

# Generates a CMake Xcode project configured for the iOS Simulator (SDK
# iphonesimulator, ARMSX2_REAL_DEVICE=OFF). This is the project XcodeBuildMCP
# should target for simulator workflows (debug, test, UI automation). Device
# IPAs continue to use generate-ios-xcode.sh + build-ios-ipa.sh.
#
# The sim project disables the -weak_framework MetalFX link flag (see
# pcsx2/CMakeLists.txt) because MetalFX.framework is absent from the
# iphonesimulator SDK; all source-level MetalFX references are also
# compile-guarded by PCSX2_HAS_METALFX=0 (see GSDeviceMTL.h).

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/app/src/main/cpp"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ios-sim-xcode}"
BUNDLE_ID="${BUNDLE_ID:-com.armsx2.ios}"

if ! command -v cmake >/dev/null 2>&1; then
	echo "error: cmake is required to generate the iOS Simulator Xcode project." >&2
	echo "Install CMake from https://cmake.org/download/ or through your package manager." >&2
	exit 1
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Xcode \
	-DCMAKE_SYSTEM_NAME=iOS \
	-DARMSX2_REAL_DEVICE=OFF \
	-DARMSX2_BUNDLE_IDENTIFIER="$BUNDLE_ID"

cat <<EOF

Generated iOS Simulator Xcode project:
  $BUILD_DIR/ARMSX2iOS.xcodeproj

Open in Xcode:
  open "$BUILD_DIR/ARMSX2iOS.xcodeproj"

Simulator build:
  xcodebuild -project "$BUILD_DIR/ARMSX2iOS.xcodeproj" -scheme ARMSX2iOS -configuration Debug -sdk iphonesimulator build

XcodeBuildMCP defaults (set after first build):
  projectPath: $BUILD_DIR/ARMSX2iOS.xcodeproj

EOF
