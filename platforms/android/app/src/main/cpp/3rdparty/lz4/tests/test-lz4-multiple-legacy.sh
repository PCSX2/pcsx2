#!/bin/sh

FPREFIX="tmp-lml"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

datagen -s1        > "${FPREFIX}1" 2> $FPREFIX-trash
datagen -s2 -g100K > "${FPREFIX}2" 2> $FPREFIX-trash
datagen -s3 -g200K > "${FPREFIX}3" 2> $FPREFIX-trash
# compress multiple files using legacy format: one .lz4 per source file
lz4 -f -l -m $FPREFIX*
test -f ${FPREFIX}1.lz4
test -f ${FPREFIX}2.lz4
test -f ${FPREFIX}3.lz4
# decompress multiple files compressed using legacy format: one output file per .lz4
mv ${FPREFIX}1 ${FPREFIX}1-orig
mv ${FPREFIX}2 ${FPREFIX}2-orig
mv ${FPREFIX}3 ${FPREFIX}3-orig
lz4 -d -f -m $FPREFIX*.lz4
lz4 -l -d -f -m $FPREFIX*.lz4 # -l mustn't impact -d option
cmp ${FPREFIX}1 ${FPREFIX}1-orig   # must be identical
cmp ${FPREFIX}2 ${FPREFIX}2-orig
cmp ${FPREFIX}3 ${FPREFIX}3-orig
# compress multiple files into stdout using legacy format
cat ${FPREFIX}1.lz4 ${FPREFIX}2.lz4 ${FPREFIX}3.lz4 > $FPREFIX-concat1
rm -f $FPREFIX*.lz4
lz4 -l -m ${FPREFIX}1 ${FPREFIX}2 ${FPREFIX}3 -c > $FPREFIX-concat2
test ! -f ${FPREFIX}1.lz4  # must not create .lz4 artefact
cmp $FPREFIX-concat1 $FPREFIX-concat2  # must be equivalent
# # # decompress multiple files into stdout using legacy format
rm $FPREFIX-concat1 $FPREFIX-concat2
lz4 -l -f -m ${FPREFIX}1 ${FPREFIX}2 ${FPREFIX}3   # generate .lz4 to decompress
cat ${FPREFIX}1 ${FPREFIX}2 ${FPREFIX}3 > ${FPREFIX}-concat1   # create concatenated reference
rm ${FPREFIX}1 ${FPREFIX}2 ${FPREFIX}3
lz4 -d -m ${FPREFIX}1.lz4 ${FPREFIX}2.lz4 ${FPREFIX}3.lz4 -c > $FPREFIX-concat2
lz4 -d -l -m ${FPREFIX}1.lz4 ${FPREFIX}2.lz4 ${FPREFIX}3.lz4 -c > $FPREFIX-concat2 # -l mustn't impact option -d
test ! -f ${FPREFIX}1  # must not create file artefact
cmp $FPREFIX-concat1 $FPREFIX-concat2  # must be equivalent
# # # compress multiple files, one of which is absent (must fail)
rm -f $FPREFIX-concat2.lz4
lz4 -f -l -m $FPREFIX-concat1 notHere-legacy $FPREFIX-concat2 && exit 1 # must fail : notHere-legacy not present
test -f $FPREFIX-concat2.lz4 # notHere was a non-blocking error, concat2.lz4 should be present
true
