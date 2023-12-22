#!/bin/bash

# Artifact Naming Scheme:
# PCSX2-<OS>-Qt-[ARCH]-[SIMD]-[pr\[PR_NUM\]]-[title|sha\[SHA|PR_TITLE\]
# -- limited to 200 chars
# Outputs:
# - artifact-name

# Inputs as env-vars
# OS
# CMAKE_FLAGS
# BUILD_CONFIGURATION
# BUILD_SYSTEM
# ARCH
# SIMD
# EVENT_NAME
# PR_TITLE
# PR_NUM
# PR_SHA

NAME=""

if [ "${OS}" == "macos" ]; then
  # MacOS has combined binaries for x64 and ARM64.
  NAME="PCSX2-${OS}-Qt"
elif [[ ("${OS}" == "windows" && "$BUILD_SYSTEM" != "cmake") ]]; then
  NAME="PCSX2-${OS}-Qt-${ARCH}-${SIMD}"
else
  NAME="PCSX2-${OS}-Qt-${ARCH}"
fi

# Add cmake if used to differentate it from msbuild builds
# Else the two artifacts will have the same name and the files will be merged
if [[ ! -z "${BUILD_SYSTEM}" ]]; then
  if [[ "${BUILD_SYSTEM}" == "cmake" ]] || [[ "${BUILD_SYSTEM}" == "flatpak" ]]; then
    NAME="${NAME}-${BUILD_SYSTEM}"
  fi
fi

# Isolate artifacts produced with different build systems, otherwise they overwrite each other
# and you have no idea what you got!
if [[ ! -z "${BUILD_SYSTEM}" ]]; then
  # differentiate between clang and msvc
  # edge case for cmake since cmake presets aren't used, instead flags are manually passed in
  if [[ "${BUILD_SYSTEM}" == "cmake" ]]; then
    if [[ "${CMAKE_FLAGS,,}" == *"clang"* ]]; then
      NAME="${NAME}-clang"
    else
        NAME="${NAME}-msvc"
    fi
  else
    if [[ "${BUILD_CONFIGURATION,,}" == *"clang"* ]]; then
      NAME="${NAME}-clang"
    else
        NAME="${NAME}-msvc"
    fi
  fi
  # also differentiate between sse4 and avx2
  if [[ "${BUILD_CONFIGURATION,,}" == *"avx2"* ]]; then
      NAME="${NAME}-avx2"
  else
      NAME="${NAME}-sse4"
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
