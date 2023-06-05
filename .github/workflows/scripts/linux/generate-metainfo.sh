#!/usr/bin/env bash

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")

if [[ $# -lt 1 ]]; then
    echo "Output file must be provided as a parameter"
    exit 1
fi

OUTFILE=$1
GIT_DATE=$(git log -1 --pretty=%cd --date=short)
GIT_VERSION=$(git tag --points-at HEAD)

if [[ "${GIT_VERSION}" == "" ]]; then
    GIT_VERSION=$(git rev-parse HEAD)
fi

echo "GIT_DATE: ${GIT_DATE}"
echo "GIT_VERSION: ${GIT_VERSION}"

cp "${SCRIPTDIR}"/pcsx2-qt.metainfo.xml.in "${OUTFILE}"

sed -i -e "s/@GIT_VERSION@/${GIT_VERSION}/" "${OUTFILE}"
sed -i -e "s/@GIT_DATE@/${GIT_DATE}/" "${OUTFILE}"
