#!/bin/bash

# Assumes you have python and clang-format installed

declare -a FORMATTED_DIRECTORIES=(
  "./pcsx2/CDVD"
)

DIRS=""
for i in "${FORMATTED_DIRECTORIES[@]}"; do
  DIRS="${DIRS}${i} "
done
echo "Checking the following directories for clang-format violations - ${BUILD_PACKAGE_STR}"

python ./3rdparty/run-clang-format/run-clang-format.py -r ${DIRS} --color always
