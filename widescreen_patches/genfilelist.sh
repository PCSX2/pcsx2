#!/bin/sh
# Generates a list of widescreen patch files.
ls -1 *.pnach > "$1.new"
if [ -f "$1" ]; then
    # Do not overwrite the patch list if the file is unchanged - this avoids
    # unnecessary rebuilds of the widescreen zip archive.
    diff -q "$1" "$1.new"
    if [ "$?" = "0" ]; then
        exit 0
    fi
fi
cp "$1.new" "$1"
