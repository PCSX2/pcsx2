#!/bin/bash

# Artifact Naming Scheme:
# PCSX2-<OS>-<GUI>-[ARCH]-[SIMD]-[pr\[PR_NUM\]]-[title|sha\[SHA|PR_TITLE\]
# -- limited to 200 chars
# Outputs:
# - artifact-name

# Inputs as env-vars
# OS
# BUILD_SYSTEM
# GUI_FRAMEWORK
# ARCH
# SIMD
# EVENT_NAME
# PR_TITLE
# PR_NUM
# PR_SHA

NAME=""

if [ "${OS}" == "macos" ]; then
  NAME="PCSX2-${OS}-${GUI_FRAMEWORK}"
elif [[ ("${OS}" == "windows" && "$BUILD_SYSTEM" != "cmake") ]]; then
  NAME="PCSX2-${OS}-${GUI_FRAMEWORK}-${ARCH}-${SIMD}"
else
  NAME="PCSX2-${OS}-${GUI_FRAMEWORK}-${ARCH}"
fi

# Add cmake if used to differentate it from msbuild builds
# Else the two artifacts will have the same name and the files will be merged
if [[ ! -z "${BUILD_SYSTEM}" ]]; then
  if [[ "${BUILD_SYSTEM}" == "cmake" ]] || [[ "${BUILD_SYSTEM}" == "flatpak" ]]; then
    NAME="${NAME}-${BUILD_SYSTEM}"
  fi
fi

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
