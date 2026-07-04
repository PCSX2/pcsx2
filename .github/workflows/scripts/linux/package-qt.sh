#!/usr/bin/env bash
#
# package-qt.sh — build portable yaps2-qt artifacts: an .AppImage AND a .tar.zst.
#
# yaps2 fork: linuxdeploy + linuxdeploy-plugin-qt bundle the Qt libraries,
# platform plugins and rpaths into an AppDir. From that single AppDir we emit
# BOTH formats so users can take whichever they prefer:
#
#   * <NAME>.AppImage  — the classic single-file AppImage (needs FUSE to run)
#   * <NAME>.tar.zst   — the same tree tarred up (no FUSE; run ./<dir>/AppRun
#                        or ./<dir>/usr/bin/yaps2-qt)

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

if [ "$#" -ne 4 ]; then
    echo "Syntax: $0 <path to pcsx2 directory> <path to build directory> <deps prefix> <output name>"
    exit 1
fi

PCSX2DIR=$1
BUILDDIR=$2
DEPSDIR=$3
NAME=$4

# Fixed AppDir name (ends in .AppDir, no brackets) so appimagetool is happy;
# we rename to "$NAME" only for the tarball.
APPDIRNAME=yaps2.AppDir

declare -a MANUAL_LIBS=(
	"libshaderc_shared.so.1"
)

set -e

# aarch64-only project: pull the arm64 tooling. (uname -m reports "aarch64" on
# the ubuntu-*-arm runners.) appimagetool reads $ARCH to pick its runtime.
export ARCH=$(uname -m)

LINUXDEPLOY=./linuxdeploy-$ARCH.AppImage
LINUXDEPLOY_PLUGIN_QT=./linuxdeploy-plugin-qt-$ARCH.AppImage
APPIMAGETOOL=./appimagetool-$ARCH.AppImage

if [ ! -f "$LINUXDEPLOY" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage
	chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_PLUGIN_QT" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$LINUXDEPLOY_PLUGIN_QT" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage
	chmod +x "$LINUXDEPLOY_PLUGIN_QT"
fi

if [ ! -f "$APPIMAGETOOL" ]; then
	"$PCSX2DIR/tools/retry.sh" wget -O "$APPIMAGETOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage
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
# From the git-tracked source tree (matches the checked-out commit exactly and
# is always present, unlike $BUILDDIR/bin/resources which only the Qt build's
# POST_BUILD populates). Same approach rocknix-deploy uses.
cp -a "$PCSX2DIR/bin/resources" "$OUTDIR/usr/bin"

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

# ---- Emit the AppImage ----------------------------------------------------
echo "Creating $NAME.AppImage..."
rm -f "$NAME.AppImage"
$APPIMAGETOOL -v "$OUTDIR" "$NAME.AppImage"

# ---- Emit the portable tarball (same AppDir tree, cleanly-named top dir) ---
echo "Creating $NAME.tar.zst..."
rm -f "$NAME.tar.zst"
rm -rf "./$NAME"
mv "$OUTDIR" "./$NAME"
tar --zstd -cf "$NAME.tar.zst" "$NAME"

echo "Done:"
echo "  $(du -h "$NAME.AppImage" | cut -f1) $NAME.AppImage"
echo "  $(du -h "$NAME.tar.zst" | cut -f1) $NAME.tar.zst"
