#!/bin/sh

FPREFIX="tmp-lfc"

set -e
remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

echo > $FPREFIX-empty
echo hi > $FPREFIX-nonempty
cat $FPREFIX-nonempty $FPREFIX-empty $FPREFIX-nonempty > $FPREFIX-src
lz4 -zq $FPREFIX-empty -c > $FPREFIX-empty.lz4
lz4 -zq $FPREFIX-nonempty -c > $FPREFIX-nonempty.lz4
cat $FPREFIX-nonempty.lz4 $FPREFIX-empty.lz4 $FPREFIX-nonempty.lz4 > $FPREFIX-concat.lz4
lz4 -d $FPREFIX-concat.lz4 -c > $FPREFIX-result
cmp $FPREFIX-src $FPREFIX-result
