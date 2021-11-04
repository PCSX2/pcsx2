#!/bin/bash

export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"
export BINARY_NAME=$(basename "$ARGV0")
if [[ ! -e "$PWD/$BINARY_NAME.config" ]]; then
        mkdir "$PWD/$BINARY_NAME.config"
fi       
export XDG_CONFIG_HOME="$PWD/$BINARY_NAME.config"

mkdir -p $HOME/.local/share/icons/hicolor/scalable/apps && cp $APPDIR/PCSX2.png $HOME/.local/share/icons/hicolor/scalable/apps
