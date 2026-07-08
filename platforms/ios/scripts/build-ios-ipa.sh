#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-ios-xcode}"
PROJECT="$BUILD_DIR/ARMSX2iOS.xcodeproj"
IPA_NAME="${IPA_NAME:-ARMSX2-iOS-unsigned.ipa}"
APP_PATH="$BUILD_DIR/Release-iphoneos/ARMSX2iOS.app"
STAGING_DIR="$BUILD_DIR/ipa-staging"
BUILD_LOG="$BUILD_DIR/xcodebuild.log"
ENTITLEMENTS_FILE="${ENTITLEMENTS_FILE:-$ROOT_DIR/app/src/main/cpp/Entitlements.plist}"
SIGN_IDENTITY="${SIGN_IDENTITY:-}"
AD_HOC_SIGN="${AD_HOC_SIGN:-0}"

refresh_generated_git_metadata() {
	local short_hash full_hash git_date pbxproj svnrev_file
	short_hash="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || true)"
	full_hash="$(git -C "$ROOT_DIR" rev-parse HEAD 2>/dev/null || true)"
	git_date="$(git -C "$ROOT_DIR" log -1 --format=%cd --date=local 2>/dev/null || true)"
	[[ -n "$short_hash" ]] || return 0

	pbxproj="$PROJECT/project.pbxproj"
	if [[ -f "$pbxproj" ]]; then
		SHORT_HASH="$short_hash" perl -0pi -e 's/ARMSX2_GIT_HASH=\\"[0-9A-Fa-f]+\\"/ARMSX2_GIT_HASH=\\"$ENV{SHORT_HASH}\\"/g' "$pbxproj"
	fi

	svnrev_file="$BUILD_DIR/common/include/svnrev.h"
	if [[ -f "$svnrev_file" ]]; then
		SHORT_HASH="$short_hash" FULL_HASH="$full_hash" GIT_DATE_TEXT="$git_date" perl -0pi -e '
			s/(#define GIT_REV "[^"]*-g)[0-9A-Fa-f]+(")/$1$ENV{SHORT_HASH}$2/g;
			s/(#define GIT_HASH ")[^"]*(")/$1$ENV{FULL_HASH}$2/g;
			s/(#define GIT_DATE ")[^"]*(")/$1$ENV{GIT_DATE_TEXT}$2/g;
		' "$svnrev_file"
	fi
}

mkdir -p "$BUILD_DIR"

if ! command -v xcodebuild >/dev/null 2>&1; then
	echo "error: xcodebuild was not found. Install full Xcode from Apple, then run:" >&2
	echo "  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
	exit 1
fi

if ! xcrun --sdk iphoneos --find metal >/dev/null 2>&1 || ! xcrun --sdk iphoneos --find metallib >/dev/null 2>&1; then
	echo "error: the iPhoneOS Metal compiler tools were not found." >&2
	echo "Make sure full Xcode is selected, not Command Line Tools only:" >&2
	echo "  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
	echo "  xcodebuild -downloadPlatform iOS" >&2
	exit 1
fi

echo "Using Xcode:"
xcodebuild -version
echo "Developer directory: $(xcode-select -p)"
echo "Metal compiler: $(xcrun --sdk iphoneos --find metal)"
echo

if command -v cmake >/dev/null 2>&1; then
	"$ROOT_DIR/scripts/generate-ios-xcode.sh"
elif [[ ! -d "$PROJECT" ]]; then
	echo "error: cmake is required to generate the iOS Xcode project." >&2
	echo "Install CMake from https://cmake.org/download/ or through your package manager." >&2
	exit 1
else
	echo "cmake not found; reusing existing generated Xcode project."
fi
refresh_generated_git_metadata

set +e
xcodebuild \
	-project "$PROJECT" \
	-scheme ARMSX2iOS \
	-configuration Release \
	-sdk iphoneos \
	CODE_SIGNING_ALLOWED=NO \
	CODE_SIGNING_REQUIRED=NO \
	CODE_SIGN_IDENTITY="" \
	build 2>&1 | tee "$BUILD_LOG"
XCODEBUILD_STATUS=${PIPESTATUS[0]}
set -e

if [[ "$XCODEBUILD_STATUS" -ne 0 ]]; then
	if grep -q "CompileMetalFile" "$BUILD_LOG"; then
		echo >&2
		echo "Metal shader compilation failed. Common fixes:" >&2
		echo "  1. Select full Xcode: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
		echo "  2. Install the iOS platform in Xcode Settings > Platforms, or run: xcodebuild -downloadPlatform iOS" >&2
		echo "  3. Open $BUILD_LOG and search above the CompileMetalFile line for the first shader error." >&2
	fi
	exit "$XCODEBUILD_STATUS"
fi

if [[ ! -d "$APP_PATH" ]]; then
	echo "error: built app was not found at $APP_PATH" >&2
	exit 1
fi

rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/Payload"
ditto "$APP_PATH" "$STAGING_DIR/Payload/ARMSX2iOS.app"
STAGED_APP="$STAGING_DIR/Payload/ARMSX2iOS.app"

CONTROLLER_SKINS_DIR="$ROOT_DIR/app/src/main/assets/app_icons/controller_skins"
if [[ -d "$CONTROLLER_SKINS_DIR" ]]; then
	ditto "$CONTROLLER_SKINS_DIR" "$STAGED_APP/controller_skins"
fi

if [[ -n "$SIGN_IDENTITY" || "$AD_HOC_SIGN" == "1" ]]; then
	if [[ ! -f "$ENTITLEMENTS_FILE" ]]; then
		echo "error: entitlements file was not found at $ENTITLEMENTS_FILE" >&2
		exit 1
	fi

	IDENTITY="$SIGN_IDENTITY"
	if [[ -z "$IDENTITY" ]]; then
		IDENTITY="-"
	fi

	echo "Signing staged app with identity '$IDENTITY' and entitlements:"
	echo "  $ENTITLEMENTS_FILE"

	if [[ -d "$STAGED_APP/Frameworks" ]]; then
		while IFS= read -r -d '' nested; do
			codesign --force --sign "$IDENTITY" --timestamp=none "$nested"
		done < <(find "$STAGED_APP/Frameworks" -type d -name '*.framework' -print0)
		while IFS= read -r -d '' nested; do
			codesign --force --sign "$IDENTITY" --timestamp=none "$nested"
		done < <(find "$STAGED_APP/Frameworks" -type f \( -name '*.dylib' -o -perm -111 \) -print0)
	fi

	codesign --force --sign "$IDENTITY" --entitlements "$ENTITLEMENTS_FILE" --timestamp=none "$STAGED_APP"
	codesign -d --entitlements :- "$STAGED_APP" 2>&1 | sed -n '1,80p'
fi

(cd "$STAGING_DIR" && zip -qry "$BUILD_DIR/$IPA_NAME" Payload)

if [[ -n "$SIGN_IDENTITY" || "$AD_HOC_SIGN" == "1" ]]; then
	echo "Created signed IPA:"
else
	echo "Created unsigned IPA:"
fi
echo "  $BUILD_DIR/$IPA_NAME"
