#!/bin/bash

set -e

export CCACHE_BASEDIR=${GITHUB_WORKSPACE}
export CCACHE_DIR=${GITHUB_WORKSPACE}/.ccache
export CCACHE_COMPRESS="true"
export CCACHE_COMPRESSLEVEL="6"
export CCACHE_MAXSIZE="400M"

# Prepare the Cache
ccache -p
ccache -z
# Build
make -j4 install
# Save the Cache
ccache -s
