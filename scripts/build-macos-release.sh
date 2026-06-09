#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_BIN="${CMAKE_BIN:-$ROOT_DIR/../cmake-3.30.5-macos-universal/CMake.app/Contents/bin/cmake}"
DEPS_DIR="${DEPS_DIR:-$ROOT_DIR/pcsx2-deps}"
BACKEND_BUILD_DIR="${BACKEND_BUILD_DIR:-$ROOT_DIR/build-release}"
BUILD_FFMPEG="${BUILD_FFMPEG:-0}"
USE_LINKED_FFMPEG="${USE_LINKED_FFMPEG:-OFF}"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"
NPROCS="$(sysctl -n hw.ncpu)"

sign_macho_files() {
	local root="$1"

	while IFS= read -r -d '' item; do
		if file "$item" | grep -q "Mach-O"; then
			codesign --force --sign "$SIGN_IDENTITY" "$item"
		fi
	done < <(find "$root" -type f -print0)
}

if [[ ! -x "$CMAKE_BIN" ]]; then
	CMAKE_BIN="cmake"
fi
if [[ -x "$CMAKE_BIN" ]]; then
	export PATH="$(dirname "$CMAKE_BIN"):$PATH"
fi

deps_ready=0
if [[ -d "$DEPS_DIR" ]]; then
	if find "$DEPS_DIR" -path '*/Qt6Config.cmake' -print -quit | grep -q . &&
	   find "$DEPS_DIR" \( -name 'libpng*.dylib' -o -name 'libpng*.a' \) -print -quit | grep -q .; then
		deps_ready=1
	fi
fi

if [[ "$deps_ready" != "1" ]]; then
	echo "Building macOS dependency prefix at $DEPS_DIR"
	BUILD_FFMPEG="$BUILD_FFMPEG" bash "$ROOT_DIR/.github/workflows/scripts/macos/build-dependencies-universal.sh" "$DEPS_DIR"
fi

"$CMAKE_BIN" \
	-DCMAKE_PREFIX_PATH="$DEPS_DIR" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_ARCHITECTURES="arm64" \
	-DDISABLE_ADVANCE_SIMD=ON \
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
	-DUSE_LINKED_FFMPEG="$USE_LINKED_FFMPEG" \
	-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
	-B "$BACKEND_BUILD_DIR" \
	"$ROOT_DIR"

"$CMAKE_BIN" --build "$BACKEND_BUILD_DIR" --target pcsx2-qt -j"$NPROCS"
"$CMAKE_BIN" --build "$BACKEND_BUILD_DIR" --target pcsx2-postprocess-bundle

BACKEND_APP="$BACKEND_BUILD_DIR/pcsx2-qt/ARMSX2.app"
if [[ ! -d "$BACKEND_APP" ]]; then
	echo "error: backend build did not produce $BACKEND_APP" >&2
	exit 1
fi

sign_macho_files "$BACKEND_APP/Contents"
codesign --force --deep --sign "$SIGN_IDENTITY" "$BACKEND_APP"
codesign --verify --deep --strict --verbose=2 "$BACKEND_APP"

BACKEND_APP_SOURCE="$BACKEND_APP" REQUIRE_BACKEND=1 "$ROOT_DIR/scripts/build-macos-ui.sh"
