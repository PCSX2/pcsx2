#!/bin/bash

### Retrieve the latest git tag
### - Assumes atleast git 2.18
LATEST_SEMVER=$(git -c 'versionsort.suffix=-' ls-remote --exit-code --refs --sort='version:refname' --tags https://github.com/PCSX2/pcsx2.git 'v*[0-9].*[0-9].*[0-9]' | tail --lines=1 | cut --delimiter='/' --fields=3)

OUT_DIR=${OUT_DIR:-"../../bin/docs"}

## Generate and Move Markdown->PDFs
### Configuration Guide

pushd ./Configuration_Guide
cp Configuration_Guide.md Configuration_Guide.temp.md
sed -i 's/{LATEST-GIT-TAG}/'"$LATEST_SEMVER"'/' Configuration_Guide.temp.md
pandoc --template ../eisvogel.tex --toc Configuration_Guide.temp.md -o Configuration_Guide.pdf
rm Configuration_Guide.temp.md
popd
mv -f ./Configuration_Guide/Configuration_Guide.pdf "$OUT_DIR"

### The rest!

find . -maxdepth 1 -name "*.md" -not -path "./README.md"  | while read filename; do
  filename="${filename%.*}"
  cp "$filename.md" "$filename.temp.md"
  sed -i 's/{LATEST-GIT-TAG}/'"$LATEST_SEMVER"'/' "$filename.temp.md"
  pandoc --template ./eisvogel.tex --toc "$filename.temp.md" -o "$filename.pdf"
  mv -f "$filename.pdf" "$OUT_DIR"
  rm "$filename.temp.md"
done
