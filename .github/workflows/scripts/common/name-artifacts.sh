#!/bin/bash

# Artifact Naming Scheme:
# <PREFIX>-[pr-<PR_NUM>]-sha-<SHA>[-title-<PR_TITLE>]
# -- limited to 200 chars
# Outputs:
# - artifact-name
#
# NOTE (yaps2 fork): upstream PCSX2 wrapped the metadata in square brackets
# (e.g. "...-sha[e880a2749]"). We use plain dashes instead because '[' and ']'
# are shell/glob metacharacters — bracketed names broke `gh release create`'s
# own asset globbing in the nightly publish step (a literal "[abc]" is read as
# a character class and never matches the file). Spaces are collapsed to dashes
# for the same reason. Keep this scheme shell-safe.

# Example - yaps2-linux-arm64-qt-sha-e880a2749

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
    NAME="${NAME}-pr-${PR_NUM}"
  fi
  NAME="${NAME}-sha-${PR_SHA}"
  if [ ! -z "${PR_TITLE}" ]; then
    # Keep alnum/_/- , collapse runs of whitespace to a single dash, and drop
    # any leading/trailing dashes. printf (not echo) so no newline leaks in.
    PR_TITLE=$(printf '%s' "${PR_TITLE}" | tr -cd '[a-zA-Z0-9[:space:]]_-' | tr -s '[:space:]' '-' | sed -E 's/^-+//; s/-+$//')
    NAME="${NAME}-title-${PR_TITLE}"
  fi
else
  SHA=$(git rev-parse --short "$GITHUB_SHA")
  NAME="${NAME}-sha-${SHA}"
fi

# Trim the Name to 200 chars
NAME=$(printf "%.200s" "$NAME")
echo "${NAME}"
echo "artifact-name=${NAME}" >> "$GITHUB_OUTPUT"
