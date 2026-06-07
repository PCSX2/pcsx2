#!/bin/sh

### Retrieve the latest git tag
### - Assumes atleast git 2.18
LATEST_SEMVER=$(git -c 'versionsort.suffix=-' ls-remote --exit-code --refs --sort='version:refname' --tags https://github.com/PCSX2/pcsx2.git 'v*[0-9].*[0-9].*[0-9]' | tail --lines=1 | cut --delimiter='/' --fields=3)

OUT_DIR=${OUT_DIR:-"../../bin/docs"}
GIT_TAG_PATTERN="{LATEST-GIT-TAG}"
PDF_METADATA_START_PATTERN="<!-- PDF METADATA STARTS "
PDF_METADATA_END_PATTERN=" PDF METADATA ENDS -->"

## Generate and Move Markdown->PDFs
prepare_markdown_file() {
  ORIG_FILE="$1".md
  TEMP_FILE="$1".temp.md
  TEMPLATE_FILE="$2"
  OUTPUT_PDF_FILE="$1".pdf

  cp "$ORIG_FILE" "$TEMP_FILE"
  # Make PDF Metadata Visible
  sed -i 's/'"$PDF_METADATA_START_PATTERN"'//' "$TEMP_FILE"
  sed -i 's/'"$PDF_METADATA_END_PATTERN"'//' "$TEMP_FILE"
  # Update Git Tag
  sed -i 's/'"$GIT_TAG_PATTERN"'/'"$LATEST_SEMVER"'/' "$TEMP_FILE"
  # Render PDF
  pandoc --template "$TEMPLATE_FILE" --toc "$TEMP_FILE" -o "$OUTPUT_PDF_FILE"
  # Delete Temp File
  rm "$TEMP_FILE"
}

### Configuration Guide, handled differently as it's in it's own folder
pushd ./Configuration_Guide
prepare_markdown_file "Configuration_Guide" "../eisvogel.tex"
popd
mv -f ./Configuration_Guide/Configuration_Guide.pdf "$OUT_DIR"

### The rest!
find . -maxdepth 1 -name "*.md" -not -path "./README.md"  | while read filename; do
  filename="${filename%.*}"
  prepare_markdown_file "$filename" "eisvogel.tex"
done
