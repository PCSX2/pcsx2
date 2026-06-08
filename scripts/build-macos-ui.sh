#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_DIR="$ROOT_DIR/macos/ARMSX2Mac"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-macos-ui}"
CONFIGURATION="${CONFIGURATION:-release}"
APP_NAME="${APP_NAME:-ARMSX2-MacOS 2.0}"
BUNDLE_ID="${BUNDLE_ID:-com.armsx2.macos.ui}"
SWIFTPM_BUILD_DIR="${SWIFTPM_BUILD_DIR:-$BUILD_DIR/swiftpm}"
IOS_ICON_SOURCE="${IOS_ICON_SOURCE:-$ROOT_DIR/../ARMSX2-iOS-pcsx2-2.7-core/app/src/main/assets/Assets.xcassets/AppIcon.appiconset/icon-1024.png}"
REQUIRE_BACKEND="${REQUIRE_BACKEND:-0}"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"
SKIP_CODESIGN="${SKIP_CODESIGN:-0}"

sign_macho_files() {
	local root="$1"

	while IFS= read -r -d '' item; do
		if file "$item" | grep -q "Mach-O"; then
			codesign --force --sign "$SIGN_IDENTITY" "$item"
		fi
	done < <(find "$root" -type f -print0)
}

swift build --package-path "$PACKAGE_DIR" --scratch-path "$SWIFTPM_BUILD_DIR" -c "$CONFIGURATION"

EXECUTABLE="$SWIFTPM_BUILD_DIR/$(uname -m)-apple-macosx/$CONFIGURATION/ARMSX2Mac"
if [[ ! -x "$EXECUTABLE" ]]; then
	EXECUTABLE="$SWIFTPM_BUILD_DIR/$CONFIGURATION/ARMSX2Mac"
fi
if [[ ! -x "$EXECUTABLE" ]]; then
	echo "error: built executable not found" >&2
	exit 1
fi

APP_DIR="$BUILD_DIR/$APP_NAME.app"
CONTENTS="$APP_DIR/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"
HELPERS="$CONTENTS/Helpers"

for legacy_name in "ARMSX2 macOS" "ARMSX2Mac"; do
	if [[ "$legacy_name" != "$APP_NAME" ]]; then
		rm -rf "$BUILD_DIR/$legacy_name.app"
	fi
done
rm -rf "$APP_DIR"
mkdir -p "$MACOS" "$RESOURCES" "$HELPERS"
cp "$EXECUTABLE" "$MACOS/ARMSX2Mac"

if [[ -f "$IOS_ICON_SOURCE" ]]; then
	ICONSET="$BUILD_DIR/ARMSX2Mac.iconset"
	rm -rf "$ICONSET"
	mkdir -p "$ICONSET"
	sips -z 16 16 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_16x16.png" >/dev/null
	sips -z 32 32 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_16x16@2x.png" >/dev/null
	sips -z 32 32 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_32x32.png" >/dev/null
	sips -z 64 64 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_32x32@2x.png" >/dev/null
	sips -z 128 128 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_128x128.png" >/dev/null
	sips -z 256 256 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_128x128@2x.png" >/dev/null
	sips -z 256 256 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_256x256.png" >/dev/null
	sips -z 512 512 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_256x256@2x.png" >/dev/null
	sips -z 512 512 "$IOS_ICON_SOURCE" --out "$ICONSET/icon_512x512.png" >/dev/null
	cp "$IOS_ICON_SOURCE" "$ICONSET/icon_512x512@2x.png"
	iconutil -c icns "$ICONSET" -o "$RESOURCES/ARMSX2Mac.icns"
fi

for resource in RedumpDatabase.yaml GameIndex.yaml; do
	if [[ -f "$ROOT_DIR/bin/resources/$resource" ]]; then
		cp "$ROOT_DIR/bin/resources/$resource" "$RESOURCES/$resource"
	fi
done

if [[ -d "$ROOT_DIR/bin/resources/icons/flags" ]]; then
	mkdir -p "$RESOURCES/icons"
	ditto "$ROOT_DIR/bin/resources/icons/flags" "$RESOURCES/icons/flags"
fi

BACKEND_APP_SOURCE="${BACKEND_APP_SOURCE:-}"
BACKEND_CANDIDATES=()
if [[ -n "$BACKEND_APP_SOURCE" ]]; then
	BACKEND_CANDIDATES+=("$BACKEND_APP_SOURCE")
fi
BACKEND_CANDIDATES+=(
	"$ROOT_DIR/build/pcsx2-qt/ARMSX2.app"
	"$ROOT_DIR/build/pcsx2-qt/pcsx2-qt.app"
	"$ROOT_DIR/build-release/pcsx2-qt/ARMSX2.app"
	"$ROOT_DIR/build-release/pcsx2-qt/pcsx2-qt.app"
	"$ROOT_DIR/ARMSX2.app"
	"$ROOT_DIR/pcsx2-qt.app"
)

BACKEND_COPIED=0
for candidate in "${BACKEND_CANDIDATES[@]}"; do
	if [[ -d "$candidate" ]]; then
		if [[ "$(basename "$candidate")" == "ARMSX2.app" ]]; then
			ditto "$candidate" "$HELPERS/ARMSX2.app"
		else
			ditto "$candidate" "$HELPERS/pcsx2-qt.app"
		fi
		BACKEND_COPIED=1
		break
	fi
done

if [[ "$REQUIRE_BACKEND" == "1" && "$BACKEND_COPIED" != "1" ]]; then
	echo "error: no bundled backend app was found; build/copy ARMSX2.app before making a release bundle" >&2
	exit 1
fi

cat > "$CONTENTS/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key>
  <string>ARMSX2Mac</string>
  <key>CFBundleIdentifier</key>
  <string>$BUNDLE_ID</string>
  <key>CFBundleName</key>
  <string>$APP_NAME</string>
  <key>CFBundleDisplayName</key>
  <string>$APP_NAME</string>
  <key>CFBundleIconFile</key>
  <string>ARMSX2Mac</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>2.0</string>
  <key>CFBundleVersion</key>
  <string>1</string>
  <key>LSMinimumSystemVersion</key>
  <string>13.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
PLIST

if [[ "$SKIP_CODESIGN" != "1" ]]; then
	if [[ -d "$HELPERS" ]]; then
		sign_macho_files "$HELPERS"
		while IFS= read -r -d '' helper_app; do
			codesign --force --sign "$SIGN_IDENTITY" "$helper_app"
		done < <(find "$HELPERS" -type d -name "*.app" -print0)
	fi

	codesign --force --sign "$SIGN_IDENTITY" "$MACOS/ARMSX2Mac"
	codesign --force --sign "$SIGN_IDENTITY" "$APP_DIR"
	codesign --verify --deep --strict --verbose=2 "$APP_DIR"
fi

echo "Created macOS app:"
echo "  $APP_DIR"
