#!/bin/bash

set -e

if [ -n "${GITHUB_ACTIONS}" ]; then
    echo "Warning: Running this script outside of GitHub Actions isn't recommended."
fi

export CCACHE_BASEDIR=${GITHUB_WORKSPACE}
export CCACHE_DIR=${GITHUB_WORKSPACE}/.ccache
export CCACHE_COMPRESS="true"
export CCACHE_COMPRESSLEVEL="6"
export CCACHE_MAXSIZE="400M"

# Prepare the Cache
ccache -p
ccache -z
# Build
ninja
# Save the Cache
ccache -s
