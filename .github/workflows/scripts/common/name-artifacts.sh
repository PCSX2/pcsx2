#!/bin/bash

# Artifact Naming Scheme:
# PCSX2-<OS>-Qt-[BUILD_SYSTEM]-[ARCH]-[SIMD]-[pr\[PR_NUM\]]-[title|sha\[SHA|PR_TITLE\]
# -- limited to 200 chars
# Outputs:
# - artifact-name

# Example - PCSX2-linux-Qt-x64-flatpak-sse4-sha[e880a2749]

# Inputs as env-vars
# PREFIX
# EVENT_NAME
# PR_TITLE
# PR_NUM
# PR_SHA

if [[ -z "${PREFIX}" ]]; then
  echo "PREFIX is not set, can't name artifact without it!"
  exit 1
fi

NAME="${PREFIX}"

# Add PR / Commit Metadata
if [ "$EVENT_NAME" == "pull_request" ]; then
  PR_SHA=$(git rev-parse --short "${PR_SHA}")
  if [ ! -z "${PR_NUM}" ]; then
    NAME="${NAME}-pr[${PR_NUM}]"
  fi
  NAME="${NAME}-sha[${PR_SHA}]"
  if [ ! -z "${PR_TITLE}" ]; then
    PR_TITLE=$(echo "${PR_TITLE}" | tr -cd '[a-zA-Z0-9[:space:]]_-')
    NAME="${NAME}-title[${PR_TITLE}"
  fi
else
  SHA=$(git rev-parse --short "$GITHUB_SHA")
  NAME="${NAME}-sha[${SHA}"
fi

# Trim the Name
NAME=$(printf "%.199s]" "$NAME")
echo "${NAME}"
echo "artifact-name=${NAME}" >> "$GITHUB_OUTPUT"
