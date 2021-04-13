#!/bin/bash

set -e

function notify {
	echo "$1 $(echo "$2" | sed -e 's/[^/]*\///')..."
}

find gsdumps -name "*.gs.xz" -print0 | while read -d $'\0' file
do
	output="${file/gsdumps/output}"
	mkdir -p "$(dirname "$output")"
	notify Rendering "$file"
	xvfb-run -s '-screen 0 1280x1024x24' build/plugins/GSdx/pcsx2_GSReplayLoader -c "$(dirname "$file")" -o "$(dirname "$output")/$(basename "$output" .gs.xz).%d.png" -r "$RENDERER" "$file"
done
echo "Comparing images..."
find output -name "*.png" -print0 | while read -d $'\0' file
do
	reference="${file/output/images}"
	if [ ! -f "$reference" ]; then
		notify "New image" "$file"
		continue
	fi
	if cmp -s "$file" "$reference"; then # Fast binary compare
		different="NO"
	elif compare -metric AE "$file" "$reference" /dev/null 2>/dev/null; then # Slower image compare
		different="NO"
	else
		different="YES"
	fi

	if [ "$different" == "YES" ]; then
		notify "Changed image:" "$file"
		changes="${file/output/changes}"
		changesbase="$(dirname "$changes")/$(basename "$changes" .png)"
		mkdir -p "$(dirname "$changes")"
		cp "$reference" "$changesbase.old.png"
		cp "$file" "$changesbase.new.png"
		compare -metric AE "$file" "$reference" "$changesbase.diff.png" 2>/dev/null || true # Don't fail because of this
	fi
done
