#!/bin/bash

set -ex

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$HOME/Depends/lib/:$HOME/Depends/plugins:$HOME/Depends/plugins/platforms"


echo "${GUI}"

cd /tmp
curl -sSfLO "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod a+x linuxdeploy*.AppImage
./linuxdeploy*.AppImage --appimage-extract

if [ "${GUI}" == "Qt" ]; then
	curl -sSfL "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" -o /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-qt.AppImage
	chmod a+x /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-qt.AppImage
	export PLUGIN=qt
else
	curl -sSfL "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh" -o /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-gtk.sh
	chmod a+x /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-gtk.sh
	export PLUGIN=gtk
fi
mv /tmp/squashfs-root/usr/bin/patchelf /tmp/squashfs-root/usr/bin/patchelf.orig
sudo cp "$HOME/Depends/bin/patchelf" /tmp/squashfs-root/usr/bin/
cd "$GITHUB_WORKSPACE"
ninja -C build install
cp ./pcsx2/gui/Resources/AppIcon64.png ./squashfs-root/PCSX2.png
cp ./linux_various/PCSX2.desktop.in ./squashfs-root/PCSX2.desktop 
sed -i -e 's|Categories=@PCSX2_MENU_CATEGORIES@|Categories=Game;Emulator;|g' ./squashfs-root/PCSX2.desktop
sed -i -e 's|__GL_THREADED_OPTIMIZATIONS=1|__GL_THREADED_OPTIMIZATIONS=0|g' ./squashfs-root/PCSX2.desktop
curl -sSfL "https://github.com/AppImage/AppImageKit/releases/download/continuous/runtime-x86_64" -o ./squashfs-root/runtime
mkdir -p squashfs-root/usr/share/applications && cp ./squashfs-root/PCSX2.desktop ./squashfs-root/usr/share/applications
mkdir -p squashfs-root/usr/share/icons && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/icons
mkdir -p squashfs-root/usr/share/icons/hicolor/scalable/apps && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/icons/hicolor/scalable/apps
mkdir -p squashfs-root/usr/share/pixmaps && cp ./squashfs-root/PCSX2.png ./squashfs-root/usr/share/pixmaps
mkdir -p squashfs-root/usr/lib/
mkdir -p squashfs-root/usr/optional/libstdc++
mkdir -p squashfs-root/usr/optional/libgcc_s
cp ./.github/workflows/scripts/linux/AppRun "$GITHUB_WORKSPACE"/squashfs-root/AppRun
curl -sSfL "https://github.com/PCSX2/appimage-checkrt-branch/releases/download/AppImage-checkrt/AppRun_patched" -o "$GITHUB_WORKSPACE"/squashfs-root/AppRun-patched
curl -sSfL "https://github.com/PCSX2/appimage-checkrt-branch/releases/download/AppImage-checkrt/exec.so" -o "$GITHUB_WORKSPACE"/squashfs-root/usr/optional/exec.so
chmod a+x ./squashfs-root/AppRun
chmod a+x ./squashfs-root/runtime
chmod a+x ./squashfs-root/AppRun-patched
chmod a+x ./squashfs-root/usr/optional/exec.so
if [ ! -e squashfs-root/usr/bin/pcsx2 ]; then mv squashfs-root/usr/bin/pcsx2-qt squashfs-root/usr/bin/pcsx2; fi
echo "$name" > "$GITHUB_WORKSPACE"/squashfs-root/version.txt
mkdir -p "$GITHUB_WORKSPACE"/squashfs-root/apprun-hooks
cp /usr/lib/x86_64-linux-gnu/libthai.so.0 "$GITHUB_WORKSPACE"/squashfs-root/usr/lib/
cp --dereference /usr/lib/x86_64-linux-gnu/libstdc++.so.6 "$GITHUB_WORKSPACE"/squashfs-root/usr/optional/libstdc++/libstdc++.so.6
cp --dereference /lib/x86_64-linux-gnu/libgcc_s.so.1 "$GITHUB_WORKSPACE"/squashfs-root/usr/optional/libgcc_s/libgcc_s.so.1
chmod +x .github/workflows/scripts/linux/app-variables.sh
cp .github/workflows/scripts/linux/app-variables.sh "$GITHUB_WORKSPACE"/squashfs-root/apprun-hooks
export UPD_INFO="gh-releases-zsync|PCSX2|pcsx2|latest|$name.AppImage.zsync"
/tmp/squashfs-root/AppRun --appdir="$GITHUB_WORKSPACE"/squashfs-root/ --plugin "$PLUGIN" -d "$GITHUB_WORKSPACE"/squashfs-root/PCSX2.desktop -i "$GITHUB_WORKSPACE"/squashfs-root/PCSX2.png
if [ "${GUI}" != "Qt" ]; then
	# see LD_LIBRARY_PATH in app-variables.sh - the intent is to use system wayland if available but fall back to app-image provided
	# a little bit hacky but should ensure maximum compatibility
	mkdir -p squashfs-root/usr/lib/wayland
	mv squashfs-root/usr/lib/libwayland-* squashfs-root/usr/lib/wayland
	rm squashfs-root/usr/lib/libgmodule-2.0.so.0
	echo "wx"
fi
curl -sSfL "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -o ./appimagetool.AppImage
chmod a+x appimagetool.AppImage
./appimagetool.AppImage "$GITHUB_WORKSPACE"/squashfs-root "$name.AppImage"
mkdir -p "$GITHUB_WORKSPACE"/ci-artifacts/
ls -al .
mv "$name.AppImage" "$GITHUB_WORKSPACE"/ci-artifacts # && mv "$name.AppImage.zsync" "$GITHUB_WORKSPACE"/ci-artifacts
chmod -R 777 ./ci-artifacts
cd ./ci-artifacts
ls -al .
