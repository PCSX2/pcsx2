#!/usr/bin/env bash
#
# package-sdl.sh — assemble a self-contained yaps2-sdl handheld tarball.
#
# yaps2 fork: the SDL3 frontend is a bare-kmsdrm handheld build (VK_KHR_display
# direct-display; Rocknix / Batocera run Wayland and use the Qt build instead).
# AppImages want FUSE and a desktop stack; a plain folder tarball (no FUSE /
# AppImage runtime) is the right shape for a kmsdrm device. Portable layout:
#
#   <name>/
#     yaps2-sdl            (the frontend, rpath $ORIGIN/lib)
#     pcsx2-gsrunner       (GS-dump replayer, optional, rpath $ORIGIN/lib)
#     resources/           (shaders + GameDB, loaded at runtime from AppRoot)
#     lib/                 (ONLY the libs we built ourselves — SDL3, shaderc,
#                           freetype, harfbuzz, plutosvg, webp, zstd, …)
#
# We deliberately bundle only the self-built libraries (those resolved from the
# deps prefix). glibc / libstdc++ / mesa / libdrm / libgbm / the Vulkan loader
# come from the device's own base system, exactly like a hand-assembled
# on-device lib/ bundle.

set -e

if [ "$#" -ne 4 ]; then
    echo "Syntax: $0 <pcsx2 dir> <build dir> <deps prefix> <output name>"
    exit 1
fi

PCSX2DIR=$1
BUILDDIR=$2
DEPSDIR=$(realpath "$3")
NAME=$4

STAGE="$NAME"
rm -rf "$STAGE"
mkdir -p "$STAGE/lib"

echo "Staging binaries…"
cp -v "$BUILDDIR/bin/yaps2-sdl" "$STAGE/"
if [ -x "$BUILDDIR/bin/pcsx2-gsrunner" ]; then
    cp -v "$BUILDDIR/bin/pcsx2-gsrunner" "$STAGE/"
fi

echo "Staging resources…"
cp -a "$BUILDDIR/bin/resources" "$STAGE/"

# Copy every shared lib that resolves out of the deps prefix (i.e. one we built).
gather_from_depsdir() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '{print $3}' | while read -r so; do
        [ -z "$so" ] && continue
        case "$so" in
            "$DEPSDIR"/*) cp -Lvn "$so" "$STAGE/lib/" ;;
        esac
    done
}

echo "Gathering self-built shared libraries…"
gather_from_depsdir "$STAGE/yaps2-sdl"
[ -f "$STAGE/pcsx2-gsrunner" ] && gather_from_depsdir "$STAGE/pcsx2-gsrunner"

# Chase transitive deps of the libs we just copied (a few passes converge).
for _pass in 1 2 3 4; do
    for so in "$STAGE"/lib/*.so*; do
        [ -e "$so" ] || continue
        gather_from_depsdir "$so"
    done
done

echo "Retargeting rpaths to \$ORIGIN…"
patchelf --set-rpath '$ORIGIN/lib' "$STAGE/yaps2-sdl"
[ -f "$STAGE/pcsx2-gsrunner" ] && patchelf --set-rpath '$ORIGIN/lib' "$STAGE/pcsx2-gsrunner"
for so in "$STAGE"/lib/*.so*; do
    [ -e "$so" ] || continue
    patchelf --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
done

echo "Bundled libraries:"
ls -1 "$STAGE/lib" | sed 's/^/    /'

echo "Creating $NAME.tar.zst…"
rm -f "$NAME.tar.zst"
tar --zstd -cf "$NAME.tar.zst" "$STAGE"
echo "Done: $(du -h "$NAME.tar.zst" | cut -f1) $NAME.tar.zst"
