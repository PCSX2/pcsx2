#!/bin/bash

set -ex

echo "${PLATFORM}"
if [ "${PLATFORM}" == "x86" ]; then
  APPARCH="i686"
  ARCH="i386"
  LIBARCH="i386-linux-gnu"
else
  APPARCH="x86_64"
  ARCH="x86_64"
  LIBARCH="x86_64-linux-gnu"
fi
BUILDPATH="$GITHUB_WORKSPACE"/build
BUILDBIN="$BUILDPATH"/pcsx2
cd /tmp
curl -sSfLO "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
chmod a+x linuxdeploy*.AppImage
./linuxdeploy-"$ARCH".AppImage --appimage-extract
curl -sSfL "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh" -o /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-gtk.sh
chmod a+x /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-gtk.sh
mv /tmp/squashfs-root/usr/bin/patchelf /tmp/squashfs-root/usr/bin/patchelf.orig
sudo cp /usr/local/bin/patchelf /tmp/squashfs-root/usr/bin/patchelf
cd "$GITHUB_WORKSPACE"
mkdir -p squashfs-root/usr/bin
ls -al "$BUILDBIN"
cp -P "$BUILDBIN"/PCSX2 "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/
patchelf --set-rpath /tmp/PCSX2 "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/PCSX2
cp ./pcsx2/gui/Resources/AppIcon64.png ./squashfs-root/PCSX2.png
cp ./linux_various/PCSX2.desktop.in ./squashfs-root/PCSX2.desktop 
sed -i -e 's|Categories=@PCSX2_MENU_CATEGORIES@|Categories=Game;Emulator;|g' ./squashfs-root/PCSX2.desktop
sed -i -e 's|__GL_THREADED_OPTIMIZATIONS=1|__GL_THREADED_OPTIMIZATIONS=0|g' ./squashfs-root/PCSX2.desktop
curl -sSfL "https://github.com/AppImage/AppImageKit/releases/download/continuous/runtime-$APPARCH" -o ./squashfs-root/runtime
mkdir -p squashfs-root/usr/share/applications && cp ./squashfs-root/PCSX2.desktop ./squashfs-root/usr/share/applications
mkdir -p squashfs-root/usr/share/icons && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/icons
mkdir -p squashfs-root/usr/share/icons/hicolor/scalable/apps && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/icons/hicolor/scalable/apps
mkdir -p squashfs-root/usr/share/pixmaps && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/pixmaps
mkdir -p squashfs-root/usr/lib/
cp ./.github/workflows/scripts/linux/AppRun "$GITHUB_WORKSPACE"/squashfs-root/AppRun
curl -sSfL "https://github.com/AppImage/AppImageKit/releases/download/continuous/AppRun-$APPARCH" -o "$GITHUB_WORKSPACE"/squashfs-root/AppRun-patched
chmod a+x ./squashfs-root/AppRun
chmod a+x ./squashfs-root/runtime
chmod a+x ./squashfs-root/AppRun-patched
echo "$name" > "$GITHUB_WORKSPACE"/squashfs-root/version.txt
mkdir -p "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/app
cp -r "$GITHUB_WORKSPACE"/bin/Langs "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/
cp "$GITHUB_WORKSPACE"/bin/docs/{Configuration_Guide.pdf,PCSX2_FAQ.pdf} "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/app
cp "$GITHUB_WORKSPACE"/bin/cheats_ws.zip "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/app
cp ./bin/GameIndex.yaml "$GITHUB_WORKSPACE"/squashfs-root/usr/bin/app/GameIndex.yaml
cp /usr/lib/$LIBARCH/libthai.so.0 "$GITHUB_WORKSPACE"/squashfs-root/usr/lib/
export UPD_INFO="gh-releases-zsync|PCSX2|pcsx2|latest|$name.AppImage.zsync"
export OUTPUT="$name.AppImage"
/tmp/squashfs-root/AppRun --appdir="$GITHUB_WORKSPACE"/squashfs-root/ --plugin gtk -d "$GITHUB_WORKSPACE"/squashfs-root/PCSX2.desktop -i "$GITHUB_WORKSPACE"/squashfs-root/PCSX2.png --output appimage
mkdir -p "$GITHUB_WORKSPACE"/artifacts/
ls -al .
mv "$name.AppImage" "$GITHUB_WORKSPACE"/artifacts # && mv "$name.AppImage.zsync" "$GITHUB_WORKSPACE"/artifacts
chmod -R 777 ./artifacts
cd ./artifacts
ls -al .
