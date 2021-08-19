#!/bin/bash

set -e

if [ -n "${GITHUB_ACTIONS}" ]; then
    echo "Warning: Running this script outside of GitHub Actions isn't recommended."
fi

# Prepare the Cache
ccache -p
ccache -z
# Build
ninja
# Save the Cache
ccache -s
