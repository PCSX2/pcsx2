#!/bin/bash

set -e

# Create AppDir, and copy over necessary contents from ./bin
# Assumes being ran from root of the repository
rm -R ./.appdir | true
mkdir -p ./.appdir/usr/bin/
cp -R ./bin/docs ./.appdir/usr/bin/
cp -R ./bin/Langs ./.appdir/usr/bin/
cp -R ./bin/shaders ./.appdir/usr/bin/
# Assumes plugins have been built
cp -R ./bin/plugins ./.appdir/usr/bin/
cp ./bin/cheats_ws.zip ./.appdir/usr/bin/
cp ./bin/GameIndex.dbf ./.appdir/usr/bin/
# NOTE - we cannot distribute a portable AppImage because then it tries to
# write to it's read-only file-system
# cp ./bin/portable.ini ./.appdir/usr/bin/
cp ./bin/PCSX2 ./.appdir/usr/bin/
# Copy in desktop and icon file
mkdir -p ./.appdir/usr/share/applications
cp ./bin/PCSX2.desktop ./.appdir/usr/share/applications
mkdir -p ./.appdir/usr/share/icons/hicolor/64x64/apps
cp ./bin/PCSX2.png ./.appdir/usr/share/icons/hicolor/64x64/apps

sudo apt install fuse libcairo2
sudo modprobe fuse
sudo groupadd fuse
user="$(whoami)"
sudo usermod -a -G fuse $user

if [ "${PLATFORM}" = "x86" ]; then
  wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-i386.AppImage
  chmod +x ./linuxdeploy-i386.AppImage
  ./linuxdeploy-i386.AppImage --appdir .appdir --output appimage
else
  wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
  chmod +x ./linuxdeploy-x86_64.AppImage
  ./linuxdeploy-x86_64.AppImage --appdir .appdir --output appimage
fi

mv PCSX2-*.AppImage PCSX2.AppImage
chmod +x PCSX2.AppImage

echo "AppImage Bundled /bin folder"
ls ./.appdir/usr/bin
echo "AppImage Bundled /bin/plugins "
ls ./.appdir/usr/bin/plugins
echo "AppImage Bundled Libraries:"
ls ./.appdir/usr/lib
