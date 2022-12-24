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

if [ "$#" -ne 3 ]; then
    echo "Syntax: $0 <path to PCSX2 directory> <deps prefix> <output name>"
    exit 1
fi

PCSX2DIR=$1
DEPSDIR=$2
NAME=$3

BINDIR="$PCSX2DIR/bin"

BINARY=pcsx2-qt
APPDIRNAME=PCSX2.AppDir
STRIP=llvm-strip-12

declare -a SYSLIBS=(
	"libaio.so.1"
	"libz.so.1"
	"libuuid.so.1"
	"libapparmor.so.1"
	"libblkid.so.1"
	"libbsd.so.0"
	"libdbus-1.so.3"
	"libgcrypt.so.20"
	"liblzma.so.5"
	"libmount.so.1"
	"libnsl.so.1"
	"libpcre.so.3"
	"libselinux.so.1"
	"libsystemd.so.0"
	"libudev.so.1"
	"libwrap.so.0"
	"libharfbuzz.so.0"
	"libFLAC.so.8"
	"libSoundTouch.so.1"
	"libXau.so.6"
	"libXcomposite.so.1"
	"libXcursor.so.1"
	"libXdamage.so.1"
	"libXdmcp.so.6"
	"libXext.so.6"
	"libXfixes.so.3"
	"libXi.so.6"
	"libXinerama.so.1"
	"libXrandr.so.2"
	"libXrender.so.1"
	"libXxf86vm.so.1"
	"libasyncns.so.0"
	"libcrypto.so.1.1"
	"libjpeg.so.8"
	"liblz4.so.1"
	"libogg.so.0"
	"libpcap.so.0.8"
	"libpng16.so.16"
	"libpulse.so.0"
	"libsamplerate.so.0"
	"libsndfile.so.1"
	"libvorbis.so.0"
	"libvorbisenc.so.2"
	"libxcb.so.1"
	"libxcb-render.so.0"
	"libxcb-shm.so.0"
	"libxkbcommon.so.0"
	"libxkbcommon-x11.so.0"
	"pulseaudio/libpulsecommon-13.99.so"
	"libasound.so.2"
	"libfreetype.so.6"
	"libpcre2-16.so.0"
	"libexpat.so.1"
	"libffi.so.7"
	"libgraphite2.so.3"
	"libresolv.so.2"
	"libgpg-error.so.0"
	"libpcre2-16.so.0"
	"libpng16.so.16"
	"libxcb-icccm.so.4"
	"libxcb-image.so.0"
	"libxcb-keysyms.so.1"
	"libxcb-randr.so.0"
	"libxcb-render.so.0"
	"libxcb-render-util.so.0"
	"libxcb-shape.so.0"
	"libxcb-sync.so.1"
	"libxcb-util.so.1"
	"libxcb-xfixes.so.0"
	"libxcb-xkb.so.1"
	"libevdev.so.2"
	"libgudev-1.0.so.0"
	"libinput.so.10"
	"libjpeg.so.8"
	"libmtdev.so.1"
	"libpng16.so.16"
	"libudev.so.1"
	"libuuid.so.1"
	"libcurl.so.4"
	"libnghttp2.so.14"
	"libidn2.so.0"
	"librtmp.so.1"
	"libssh.so.4"
	"libpsl.so.5"
	"libssl.so.1.1"
	"libnettle.so.7"
	"libgnutls.so.30"
	"libgssapi_krb5.so.2"
	"libldap_r-2.4.so.2"
	"liblber-2.4.so.2"
	"libbrotlidec.so.1"
	"libunistring.so.2"
	"libhogweed.so.5"
	"libgmp.so.10"
	"libp11-kit.so.0"
	"libtasn1.so.6"
	"libkrb5.so.3"
	"libk5crypto.so.3"
	"libcom_err.so.2"
	"libkrb5support.so.0"
	"libsasl2.so.2"
	"libgssapi.so.3"
	"libbrotlicommon.so.1"
	"libkeyutils.so.1"
	"libheimntlm.so.0"
	"libkrb5.so.26"
	"libasn1.so.8"
	"libhcrypto.so.4"
	"libroken.so.18"
	"libwind.so.0"
	"libheimbase.so.1"
	"libhx509.so.5"
	"libsqlite3.so.0"
	"libcrypt.so.1"
)

declare -a DEPLIBS=(
	"libSDL2-2.0.so.0"
)

declare -a QTLIBS=(
	"libQt6Core.so.6"
	"libQt6Gui.so.6"
	"libQt6Network.so.6"
	"libQt6OpenGL.so.6"
	"libQt6Svg.so.6"
	"libQt6WaylandClient.so.6"
	"libQt6WaylandCompositor.so.6"
	"libQt6WaylandEglClientHwIntegration.so.6"
	"libQt6WaylandEglCompositorHwIntegration.so.6"
	"libQt6Widgets.so.6"
	"libQt6XcbQpa.so.6"
)

declare -a QTPLUGINS=(
	"plugins/iconengines"
	"plugins/imageformats"
	"plugins/platforms"
	#"plugins/platformthemes" # Enable this if we want to ship GTK+ themes at any point.
	"plugins/tls"
	"plugins/wayland-decoration-client"
	"plugins/wayland-graphics-integration-client"
	"plugins/wayland-graphics-integration-server"
	"plugins/wayland-shell-integration"
	"plugins/xcbglintegrations"
)

set -e

if [ ! -f appimagetool-x86_64.AppImage ]; then
	wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x appimagetool-x86_64.AppImage
fi

OUTDIR=$(realpath "./$APPDIRNAME")
SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
rm -fr "$OUTDIR"
mkdir "$OUTDIR"

mkdir -p "$OUTDIR/usr/bin" "$OUTDIR/usr/lib" "$OUTDIR/usr/lib/pulseaudio"

echo "Copying binary and resources..."
cp -a "$BINDIR/$BINARY" "$BINDIR/resources" "$OUTDIR/usr/bin"

# Patch RPATH so the binary goes hunting for shared libraries in the AppDir instead of system.
echo "Patching RPATH in ${BINARY}..."
patchelf --set-rpath '$ORIGIN/../lib' "$OUTDIR/usr/bin/$BINARY"

# Currently we leave the main binary unstripped, uncomment if this is not desired.
#$STRIP "$OUTDIR/usr/bin/$BINARY"

# Libraries we pull in from the system.
echo "Copying system libraries..."
for lib in "${SYSLIBS[@]}"; do
	blib=$(basename "$lib")
	if [ -f "/lib/x86_64-linux-gnu/$lib" ]; then
		cp "/lib/x86_64-linux-gnu/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/usr/lib/x86_64-linux-gnu/$lib" ]; then
		cp "$CHROOT/usr/lib/x86_64-linux-gnu/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/lib/$lib" ]; then
		cp "$CHROOT/lib/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/usr/lib/$lib" ]; then
		cp "$CHROOT/usr/lib/$lib" "$OUTDIR/usr/lib/$blib"
	else
		echo "*** Failed to find '$blib'"
		exit 1
	fi

	$STRIP "$OUTDIR/usr/lib/$blib"
done

# Dependencies we built, at this point it's just SDL.
echo "Copying dependency libraries..."
for lib in "${DEPLIBS[@]}"; do
	blib=$(basename "$lib")
	if [ -f "$DEPSDIR/lib/$lib" ]; then
		cp "$DEPSDIR/lib/$lib" "$OUTDIR/usr/lib/$blib"
	else
		echo "*** Failed to find '$blib'"
		exit 1
	fi

	$STRIP "$OUTDIR/usr/lib/$blib"
done

echo "Copying Qt libraries..."
for lib in "${QTLIBS[@]}"; do
	cp -aL "$DEPSDIR/lib/$lib" "$OUTDIR/usr/lib"
	$STRIP "$OUTDIR/usr/lib/$lib"
done

echo "Copying Qt plugins..."
mkdir -p "$OUTDIR/usr/lib/plugins"
for plugin in "${QTPLUGINS[@]}"; do
	mkdir -p "$OUTDIR/usr/lib/$plugin"
	cp -aL "$DEPSDIR/$plugin"/*.so "$OUTDIR/usr/lib/$plugin/"
done

for so in $(find "$OUTDIR/usr/lib/plugins" -iname '*.so'); do
	# This is ../../ because it's usually plugins/group/name.so
	echo "Patching RPATH in ${so}..."
	patchelf --set-rpath '$ORIGIN/../..' "$so"
	$STRIP "$so"
done

for so in $(find "$OUTDIR/usr/lib" -maxdepth 1); do
	if [ -f "$so" ]; then
		echo "Patching RPATH in ${so}"
		patchelf --set-rpath '$ORIGIN' "$so"
	fi
done

echo "Creating qt.conf..."
cat > "$OUTDIR/usr/bin/qt.conf" << EOF
[Paths]
Plugins = ../lib/plugins
EOF

echo "Copy desktop/icon..."
cp "$PCSX2DIR/pcsx2/Resources/AppIcon64.png" "$OUTDIR/PCSX2.png"
cp "$SCRIPTDIR/pcsx2-qt.desktop" "$OUTDIR/PCSX2.desktop"
cp "$SCRIPTDIR/AppRun-qt" "$OUTDIR/AppRun"

echo "Generate AppImage"
./appimagetool-x86_64.AppImage -v "$OUTDIR" "$NAME.AppImage"
