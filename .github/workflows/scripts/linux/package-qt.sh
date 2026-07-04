#!/usr/bin/env bash
#
# package-qt.sh — build a portable, self-contained yaps2-qt tarball.
#
# yaps2 fork: this uses the very same linuxdeploy + linuxdeploy-plugin-qt
# bundling that an AppImage build would (it is what correctly gathers the Qt
# libraries, platform plugins and rpaths), but stops at the AppDir and tars it
# up instead of wrapping it in an AppImage. No FUSE is needed to run it, which
# keeps it consistent with the SDL handheld tarball. Extract it and launch with
#
#     ./<dir>/AppRun            (or ./<dir>/usr/bin/yaps2-qt)
#
# The <dir> is a normal directory tree: AppRun launcher, usr/bin/yaps2-qt,
# usr/lib (Qt + our deps), usr/plugins (Qt platform/imageformat plugins),
# usr/bin/resources, usr/bin/translations.

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

if [ "$#" -ne 4 ]; then
    echo "Syntax: $0 <path to pcsx2 directory> <path to build directory> <deps prefix> <output name>"
    exit 1
fi

PCSX2DIR=$1
BUILDDIR=$2
DEPSDIR=$3
NAME=$4

declare -a MANUAL_LIBS=(
	"libshaderc_shared.so.1"
)

set -e

# aarch64-only project: pull the arm64 linuxdeploy tooling. (uname -m reports
# "aarch64" on the ubuntu-*-arm runners.) No appimagetool — we tar the AppDir.
ARCH=$(uname -m)

LINUXDEPLOY=./linuxdeploy-$ARCH.AppImage
LINUXDEPLOY_PLUGIN_QT=./linuxdeploy-plugin-qt-$ARCH.AppImage

if [ ! -f "$LINUXDEPLOY" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage
	chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_PLUGIN_QT" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY_PLUGIN_QT" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage
	chmod +x "$LINUXDEPLOY_PLUGIN_QT"
fi

# Build the AppDir directly under the artifact name so the extracted top-level
# folder is self-describing.
OUTDIR=$(realpath "./$NAME")
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
unset IFS

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
$LINUXDEPLOY --plugin qt --appdir="$OUTDIR" --executable="$BUILDDIR/bin/yaps2-qt" ${EXTRA_LIBS_ARGS[@]} \
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

echo "Creating $NAME.tar.zst..."
rm -f "$NAME.tar.zst"
# -C to the parent so the archive holds a single top-level "$NAME/" directory.
tar --zstd -cf "$NAME.tar.zst" -C "$(dirname "$OUTDIR")" "$(basename "$OUTDIR")"
echo "Done: $(du -h "$NAME.tar.zst" | cut -f1) $NAME.tar.zst"
