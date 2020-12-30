#!/bin/bash

OUT_DIR=${OUT_DIR:-"../../bin/docs"}

## Generate and Move Markdown->PDFs
### Configuration Guide

pushd Configuration_Guide
pandoc --template "../eisvogel.tex" --toc "Configuration_Guide.md" -o "Configuration_Guide.pdf"
popd
mv -f "./Configuration_Guide/Configuration_Guide.pdf" "$OUT_DIR"

### The rest!

find . -maxdepth 1 -name "*.md" -not -path "./README.md"  | while read filename; do
  filename="${filename%.*}"
  pandoc --template "./eisvogel.tex" --toc "$filename.md" -o "$filename.pdf"
  mv -f "$filename.pdf" "$OUT_DIR"
done
