#!/bin/sh

SKIPFILE="goldenSamples/skip.bin"
FPREFIX="tmp-lsk"

set -e

remove () {
    rm "$FPREFIX"*
}

trap remove EXIT

set -x

lz4 -dc $SKIPFILE
lz4 -dc < $SKIPFILE
printf "Hello from Valid Frame!\n" | lz4 -c > $FPREFIX.lz4
cat $SKIPFILE $FPREFIX.lz4 $SKIPFILE | lz4 -dc
