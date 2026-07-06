#!/usr/bin/env bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

if [[ $# -lt 1 ]]; then
	echo "Output file must be provided as a parameter"
	exit 1
fi

OUTFILE=$1
GIT_DATE=$(git log -1 --pretty=%cd --date=iso8601)
GIT_VERSION=$(git tag --points-at HEAD)
GIT_HASH=$(git rev-parse HEAD)

if [[ -z "${GIT_VERSION}" ]]; then
    if git branch -r --contains HEAD | grep -q 'origin/master'; then
        # Our master doesn't have a tagged commit
        # This happens when the commit is "ci skip" 
        # abbrev so we have just the latest tag
        # ie v2.3.420 (Yes, that's the current master at the time of writing)
        GIT_VERSION=$(git describe --tags --abbrev=0)
    else
        # We are probably building a PR
        # Keep the short SHA in the version
        # ie v2.3.420-1-g10dc1a2da
        GIT_VERSION=$(git describe --tags)
    fi

    if [[ -z "${GIT_VERSION}" ]]; then
        # Fallback to raw commit hash
        GIT_VERSION=$(git rev-parse HEAD)
    fi
fi

echo "GIT_DATE: ${GIT_DATE}"
echo "GIT_VERSION: ${GIT_VERSION}"
echo "GIT_HASH: ${GIT_HASH}"

cp "${SCRIPTDIR}"/pcsx2-qt.metainfo.xml.in "${OUTFILE}"

sed -i -e "s/@GIT_VERSION@/${GIT_VERSION}/" "${OUTFILE}"
sed -i -e "s/@GIT_DATE@/${GIT_DATE}/" "${OUTFILE}"
sed -i -e "s/@GIT_HASH@/${GIT_HASH}/" "${OUTFILE}"

