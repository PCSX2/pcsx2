#!/bin/bash

# Assumes you have python and clang-format installed

declare -a FORMATTED_DIRECTORIES=(
  "./pcsx2/CDVD"
)

DIRS=""
for i in "${FORMATTED_DIRECTORIES[@]}"; do
  DIRS="${DIRS}${i} "
done

python ./3rdparty/run-clang-format/run-clang-format.py -i -r ${DIRS}
