#!/usr/bin/env bash

# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

if [ "$#" -ne 4 ]; then
    echo "Syntax: $0 <path to pcsx2 directory> <path to build directory> <deps prefix> <output name>"
    exit 1
fi

PCSX2DIR=$1
BUILDDIR=$2
DEPSDIR=$3
NAME=$4

BINARY=pcsx2-qt
APPDIRNAME=PCSX2.AppDir
STRIP=strip

declare -a MANUAL_LIBS=(
	"libshaderc_shared.so.1"
	"libharfbuzz.so.0"
	"libfreetype.so.6"
)

set -e

LINUXDEPLOY=./linuxdeploy-x86_64.AppImage
LINUXDEPLOY_PLUGIN_QT=./linuxdeploy-plugin-qt-x86_64.AppImage
APPIMAGETOOL=./appimagetool-x86_64.AppImage

if [ ! -f "$LINUXDEPLOY" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
	chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_PLUGIN_QT" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY_PLUGIN_QT" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
	chmod +x "$LINUXDEPLOY_PLUGIN_QT"
fi

if [ ! -f "$APPIMAGETOOL" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$APPIMAGETOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x "$APPIMAGETOOL"
fi

OUTDIR=$(realpath "./$APPDIRNAME")
rm -fr "$OUTDIR"

echo "Locating extra libraries..."
EXTRA_LIBS_ARGS=()
for lib in "${MANUAL_LIBS[@]}"; do
	srcpath=$(find "$DEPSDIR" -name "$lib")
	if [ ! -f "$srcpath" ]; then
		echo "Missing extra library $lib. Exiting."
		exit 1
	fi

	echo "Found $lib at $srcpath."
	EXTRA_LIBS_ARGS+=( "--library=$srcpath" )
done

# Why the nastyness? linuxdeploy strips our main binary, and there's no option to turn it off.
# It also doesn't strip the Qt libs. We can't strip them after running linuxdeploy, because
# patchelf corrupts the libraries (but they still work), but patchelf+strip makes them crash
# on load. So, make a backup copy, strip the original (since that's where linuxdeploy finds
# the libs to copy), then swap them back after we're done.
# Isn't Linux packaging amazing?

rm -fr "$DEPSDIR.bak"
cp -a "$DEPSDIR" "$DEPSDIR.bak"
IFS="
"
for i in $(find "$DEPSDIR" -iname '*.so'); do
  echo "Stripping deps library ${i}"
  strip "$i"
done

echo "Copying desktop file..."
cp "$PCSX2DIR/.github/workflows/scripts/linux/pcsx2-qt.desktop" "net.pcsx2.PCSX2.desktop"
cp "$PCSX2DIR/bin/resources/icons/AppIconLarge.png" "PCSX2.png"

echo "Running linuxdeploy to create AppDir..."
# The wayland platform plugin requires the plugins deployed for the waylandcompositor module
# Interestingly, specifying the module doesn't copy the module, only the required plugins for it
# https://github.com/linuxdeploy/linuxdeploy-plugin-qt/issues/160#issuecomment-2655543893
EXTRA_QT_MODULES="core;gui;svg;waylandclient;waylandcompositor;widgets;xcbqpa" \
EXTRA_PLATFORM_PLUGINS="libqwayland.so" \
DEPLOY_PLATFORM_THEMES="1" \
QMAKE="$DEPSDIR/bin/qmake" \
NO_STRIP="1" \
$LINUXDEPLOY --plugin qt --appdir="$OUTDIR" --executable="$BUILDDIR/bin/pcsx2-qt" ${EXTRA_LIBS_ARGS[@]} \
--desktop-file="net.pcsx2.PCSX2.desktop" --icon-file="PCSX2.png"

echo "Copying resources into AppDir..."
cp -a "$BUILDDIR/bin/resources" "$OUTDIR/usr/bin"

# Restore unstripped deps (for cache).
rm -fr "$DEPSDIR"
mv "$DEPSDIR.bak" "$DEPSDIR"

# Fix up translations.
rm -fr "$OUTDIR/usr/bin/translations" "$OUTDIR/usr/translations"
cp -a "$BUILDDIR/bin/translations" "$OUTDIR/usr/bin"

# Generate AppStream meta-info.
echo "Generating AppStream metainfo..."
mkdir -p "$OUTDIR/usr/share/metainfo"
"$SCRIPTDIR/generate-metainfo.sh" "$OUTDIR/usr/share/metainfo/net.pcsx2.PCSX2.appdata.xml"

echo "Generating AppImage..."
GIT_VERSION=$(git tag --points-at HEAD)

if [[ "${GIT_VERSION}" == "" ]]; then
	# In the odd event that we run this script before the release gets tagged.
	GIT_VERSION=$(git describe --tags || true)
	if [[ "${GIT_VERSION}" == "" ]]; then
		GIT_VERSION=$(git rev-parse HEAD)
	fi
fi

rm -f "$NAME.AppImage"
$APPIMAGETOOL -v "$OUTDIR" "$NAME.AppImage"
