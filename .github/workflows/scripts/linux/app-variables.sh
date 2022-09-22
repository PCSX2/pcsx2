#!/bin/bash

export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"

# use system wayland if available otherwise use the appimage provided wayland
if [[ $(ldconfig -p | grep libwayland-client | wc -l) -lt 1 ]]; then
        export LD_LIBRARY_PATH="$APPDIR/usr/lib/wayland:$LD_LIBRARY_PATH"
fi

export BINARY_NAME=$(basename "$ARGV0")
if [[ ! -e "$PWD/$BINARY_NAME.config" ]]; then
        mkdir "$PWD/$BINARY_NAME.config"
fi
export XDG_CONFIG_HOME="$PWD/$BINARY_NAME.config"

mkdir -p "$HOME"/.local/share/icons/hicolor/scalable/apps && cp "$APPDIR"/PCSX2.png "$HOME"/.local/share/icons/hicolor/scalable/apps
