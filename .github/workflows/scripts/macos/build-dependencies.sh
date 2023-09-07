#!/bin/bash

set -e

export MACOSX_DEPLOYMENT_TARGET=10.14

INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.28.2
PNG=1.6.37
JPG=9e
FFMPEG=6.0
QT=6.4.3 # Currently stuck on Qt 6.4 due to 6.5 requiring macOS 11.0.

mkdir deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib -dead_strip $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"

cat > SHASUMS <<EOF
64b1102fa22093515b02ef33dd8739dee1ba57e9dbba6a092942b8bbed1a1c5e  $SDL.tar.gz
505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca  libpng-$PNG.tar.xz
4077d6a6a75aeb01884f708919d25934c93305e49f7e3f36db9129320e6f4f3d  jpegsrc.v$JPG.tar.gz
57be87c22d9b49c112b6d24bc67d42508660e6b718b3db89c44e47e289137082  ffmpeg-$FFMPEG.tar.xz
5087c9e5b0165e7bc3c1a4ab176b35d0cd8f52636aea903fa377bdba00891a60  qtbase-everywhere-src-$QT.tar.xz
0aff58062e74b84617c5da8325d8cdad5368d8f4d2a11ceafcd58329fe99b798  qtimageformats-everywhere-src-$QT.tar.xz
88315f886cf81898705e487cedba6e6160724359d23c518c92c333c098879a4a  qtsvg-everywhere-src-$QT.tar.xz
867df829cd5cd3ae8efe62e825503123542764b13c96953511e567df70c5a091  qttools-everywhere-src-$QT.tar.xz
79e56b7800d49649a8a8010818538c367a829e0b7a09d5f60bd3aecf5abe972c  qttranslations-everywhere-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$PNG/libpng-$PNG.tar.xz" \
	-O "https://www.ijg.org/files/jpegsrc.v$JPG.tar.gz" \
	-O "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtbase-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtimageformats-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttools-everywhere-src-$QT.tar.xz" \
	-O "https://download.qt.io/official_releases/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere-src-$QT.tar.xz" \

shasum -a 256 --check SHASUMS

echo "Installing SDL..."
tar xf "$SDL.tar.gz"
cd "$SDL"

# MFI causes multiple joystick connection events, I'm guessing because both the HIDAPI and MFI interfaces
# race each other, and sometimes both end up getting through. So, just force MFI off.
patch -u CMakeLists.txt <<EOF
--- CMakeLists.txt	2023-08-03 01:33:11
+++ CMakeLists.txt	2023-08-26 12:58:53
@@ -2105,7 +2105,7 @@
           #import <Foundation/Foundation.h>
           #import <CoreHaptics/CoreHaptics.h>
           int main() { return 0; }" HAVE_FRAMEWORK_COREHAPTICS)
-      if(HAVE_FRAMEWORK_GAMECONTROLLER AND HAVE_FRAMEWORK_COREHAPTICS)
+      if(HAVE_FRAMEWORK_GAMECONTROLLER AND HAVE_FRAMEWORK_COREHAPTICS AND FALSE)
         # Only enable MFI if we also have CoreHaptics to ensure rumble works
         set(SDL_JOYSTICK_MFI 1)
         set(SDL_FRAMEWORK_GAMECONTROLLER 1)

EOF

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DSDL_X11=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing libpng..."
tar xf "libpng-$PNG.tar.xz"
cd "libpng-$PNG"
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking
make "-j$NPROCS"
make install
cd ..

echo "Installing libjpeg..."
tar xf "jpegsrc.v$JPG.tar.gz"
cd "jpeg-$JPG"
./configure --prefix "$INSTALLDIR" --disable-dependency-tracking
make "-j$NPROCS"
make install
cd ..

echo "Installing FFmpeg..."
tar xf "ffmpeg-$FFMPEG.tar.xz"
cd "ffmpeg-$FFMPEG"
./configure --prefix="$INSTALLDIR" --disable-all --disable-autodetect --disable-static --enable-shared \
	--enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
	--enable-audiotoolbox --enable-videotoolbox \
	--enable-encoder=ffv1,qtrle,pcm_s16be,pcm_s16le,*_at,*_videotoolbox \
	--enable-muxer=avi,matroska,mov,mp3,mp4,wav \
	--enable-protocol=file
make "-j$NPROCS"
make install
cd ..

echo "Installing Qt Base..."
tar xf "qtbase-everywhere-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
# Qt's panel:shouldEnableURL: implementation does a whole bunch of things that activate macOS's sandbox permissions dialog
# Since this is called on every file being displayed in the open/save panel, that spams users with permissions dialogs
# Simple solution: Hopefully no one needs any filters that aren't simple file extension filters, remove all other handling
patch -u src/plugins/platforms/cocoa/qcocoafiledialoghelper.mm <<EOF
--- src/plugins/platforms/cocoa/qcocoafiledialoghelper.mm
+++ src/plugins/platforms/cocoa/qcocoafiledialoghelper.mm
@@ -133,7 +133,5 @@
     NSURL *url = [NSURL fileURLWithPath:filepath isDirectory:info.isDir()];
-    bool selectable = (m_options->acceptMode() == QFileDialogOptions::AcceptSave)
-        || [self panel:m_panel shouldEnableURL:url];
 
     m_panel.directoryURL = [NSURL fileURLWithPath:m_currentDirectory];
-    m_panel.nameFieldStringValue = selectable ? info.fileName().toNSString() : @"";
+    m_panel.nameFieldStringValue = info.fileName().toNSString();
 
@@ -203,61 +201,2 @@
     return hidden;
-}
-
-- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url
-{
-    Q_UNUSED(sender);
-
-    NSString *filename = url.path;
-    if (!filename.length)
-        return NO;
-
-    // Always accept directories regardless of their names (unless it is a bundle):
-    NSFileManager *fm = NSFileManager.defaultManager;
-    NSDictionary *fileAttrs = [fm attributesOfItemAtPath:filename error:nil];
-    if (!fileAttrs)
-        return NO; // Error accessing the file means 'no'.
-    NSString *fileType = fileAttrs.fileType;
-    bool isDir = [fileType isEqualToString:NSFileTypeDirectory];
-    if (isDir) {
-        if (!m_panel.treatsFilePackagesAsDirectories) {
-            if ([NSWorkspace.sharedWorkspace isFilePackageAtPath:filename] == NO)
-                return YES;
-        }
-    }
-
-    // Treat symbolic links and aliases to directories like directories
-    QFileInfo fileInfo(QString::fromNSString(filename));
-    if (fileInfo.isSymLink() && QFileInfo(fileInfo.symLinkTarget()).isDir())
-        return YES;
-
-    QString qtFileName = fileInfo.fileName();
-    // No filter means accept everything
-    bool nameMatches = m_selectedNameFilter->isEmpty();
-    // Check if the current file name filter accepts the file:
-    for (int i = 0; !nameMatches && i < m_selectedNameFilter->size(); ++i) {
-        if (QDir::match(m_selectedNameFilter->at(i), qtFileName))
-            nameMatches = true;
-    }
-    if (!nameMatches)
-        return NO;
-
-    QDir::Filters filter = m_options->filter();
-    if ((!(filter & (QDir::Dirs | QDir::AllDirs)) && isDir)
-        || (!(filter & QDir::Files) && [fileType isEqualToString:NSFileTypeRegular])
-        || ((filter & QDir::NoSymLinks) && [fileType isEqualToString:NSFileTypeSymbolicLink]))
-        return NO;
-
-    bool filterPermissions = ((filter & QDir::PermissionMask)
-                              && (filter & QDir::PermissionMask) != QDir::PermissionMask);
-    if (filterPermissions) {
-        if ((!(filter & QDir::Readable) && [fm isReadableFileAtPath:filename])
-            || (!(filter & QDir::Writable) && [fm isWritableFileAtPath:filename])
-            || (!(filter & QDir::Executable) && [fm isExecutableFileAtPath:filename]))
-            return NO;
-    }
-    if (!(filter & QDir::Hidden)
-        && (qtFileName.startsWith(u'.') || [self isHiddenFileAtURL:url]))
-            return NO;
-
-    return YES;
 }
@@ -406,5 +345,2 @@
 {
-    if (m_options->acceptMode() != QFileDialogOptions::AcceptSave)
-        return nil; // panel:shouldEnableURL: does the file filtering for NSOpenPanel
-
     QStringList fileTypes;
EOF
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_optimize_size=ON -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt SVG..."
tar xf "qtsvg-everywhere-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt Image Formats..."
tar xf "qtimageformats-everywhere-src-$QT.tar.xz"
cd "qtimageformats-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=MinSizeRel
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt Tools..."
tar xf "qttools-everywhere-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
# Linguist relies on a library in the Designer target, which takes 5-7 minutes to build on the CI
# Avoid it by not building Linguist, since we only need the tools that come with it
patch -u src/linguist/CMakeLists.txt <<EOF
--- src/linguist/CMakeLists.txt
+++ src/linguist/CMakeLists.txt
@@ -14,7 +14,7 @@
 add_subdirectory(lrelease-pro)
 add_subdirectory(lupdate)
 add_subdirectory(lupdate-pro)
-if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND NOT no-png)
+if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND TARGET Qt::PrintSupport AND NOT no-png)
     add_subdirectory(linguist)
 endif()
EOF
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..
echo "Installing Qt Translations..."
tar xf "qttranslations-everywhere-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
