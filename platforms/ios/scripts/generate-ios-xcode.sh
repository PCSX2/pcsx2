#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="$ROOT_DIR/app/src/main/cpp"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ios-xcode}"
BUNDLE_ID="${BUNDLE_ID:-com.armsx2.ios}"
TEAM_ID="${TEAM_ID:-}"

if ! command -v cmake >/dev/null 2>&1; then
	echo "error: cmake is required to generate the iOS Xcode project." >&2
	echo "Install CMake from https://cmake.org/download/ or through your package manager." >&2
	exit 1
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Xcode \
	-DCMAKE_SYSTEM_NAME=iOS \
	-DARMSX2_REAL_DEVICE=ON \
	-DARMSX2_BUNDLE_IDENTIFIER="$BUNDLE_ID" \
	-DARMSX2_DEVELOPMENT_TEAM="$TEAM_ID"

cat <<EOF

Generated Xcode project:
  $BUILD_DIR/ARMSX2iOS.xcodeproj

Open in Xcode:
  open "$BUILD_DIR/ARMSX2iOS.xcodeproj"

Unsigned device build:
  xcodebuild -project "$BUILD_DIR/ARMSX2iOS.xcodeproj" -scheme ARMSX2iOS -configuration Release -sdk iphoneos CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" build

To use automatic signing, regenerate with:
  TEAM_ID=YOUR_APPLE_TEAM_ID BUNDLE_ID=com.your.bundle.id ./scripts/generate-ios-xcode.sh

EOF
