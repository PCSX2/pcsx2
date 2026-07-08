#!/bin/sh

FPREFIX="tmp-lfh"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

datagen -g6GB    | lz4 -vB5 | lz4 -qt
# test large file size [2-4] GB
datagen -g3G -P100 | lz4 -vv | lz4 --decompress --force --sparse - ${FPREFIX}1
ls -ls ${FPREFIX}1
datagen -g3G -P100 | lz4 --quiet --content-size | lz4 --verbose --decompress --force --sparse - ${FPREFIX}2
ls -ls ${FPREFIX}2
diff -s ${FPREFIX}1 ${FPREFIX}2
