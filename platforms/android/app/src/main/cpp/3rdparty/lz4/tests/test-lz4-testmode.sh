#!/bin/sh

FPREFIX="tmp-ltm"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

lz4 -bi0
datagen > $FPREFIX
lz4 -f $FPREFIX -c > $FPREFIX.lz4
lz4 -bdi0 $FPREFIX.lz4 # test benchmark decode-only mode
lz4 -bdi0 --no-crc $FPREFIX.lz4 # test benchmark decode-only mode
echo "---- test mode ----"
datagen | lz4 -t && exit 1
datagen | lz4 -tf && exit 1
echo "---- pass-through mode ----"
echo "Why hello there " > ${FPREFIX}2.lz4
lz4 -f ${FPREFIX}2.lz4 > $FPREFIX-trash && exit 1
datagen | lz4 -dc  > $FPREFIX-trash && exit 1
datagen | lz4 -df > $FPREFIX-trash && exit 1
datagen | lz4 -dcf > $FPREFIX-trash
echo "Hello World !" > ${FPREFIX}1
lz4 -dcf ${FPREFIX}1
echo "from underground..." > ${FPREFIX}2
lz4 -dcfm ${FPREFIX}1 ${FPREFIX}2
echo "---- non-existing source (must fail cleanly) ----"
lz4     file-does-not-exist && exit 1
lz4 -f  file-does-not-exist && exit 1
lz4 -t  file-does-not-exist && exit 1
lz4 -fm file1-dne file2-dne && exit 1
true
