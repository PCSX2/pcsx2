#!/bin/sh

FPREFIX="tmp-tls"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

datagen -g5M  -P100 > ${FPREFIX}dg5M
lz4 -B4D ${FPREFIX}dg5M -c | lz4 -dv --sparse > ${FPREFIX}cB4
diff -s ${FPREFIX}dg5M ${FPREFIX}cB4
lz4 -B5D ${FPREFIX}dg5M -c | lz4 -dv --sparse > ${FPREFIX}cB5
diff -s ${FPREFIX}dg5M ${FPREFIX}cB5
lz4 -B6D ${FPREFIX}dg5M -c | lz4 -dv --sparse > ${FPREFIX}cB6
diff -s ${FPREFIX}dg5M ${FPREFIX}cB6
lz4 -B7D ${FPREFIX}dg5M -c | lz4 -dv --sparse > ${FPREFIX}cB7
diff -s ${FPREFIX}dg5M ${FPREFIX}cB7
lz4 ${FPREFIX}dg5M -c | lz4 -dv --no-sparse > ${FPREFIX}nosparse
diff -s ${FPREFIX}dg5M ${FPREFIX}nosparse
ls -ls $FPREFIX*
datagen -s1 -g1200007 -P100 | lz4 | lz4 -dv --sparse > ${FPREFIX}odd   # Odd size file (to generate non-full last block)
datagen -s1 -g1200007 -P100 | diff -s - ${FPREFIX}odd
ls -ls ${FPREFIX}odd
rm $FPREFIX*
printf "\n Compatibility with Console :"
echo "Hello World 1 !" | lz4 | lz4 -d -c
echo "Hello World 2 !" | lz4 | lz4 -d | cat
echo "Hello World 3 !" | lz4 --no-frame-crc | lz4 -d -c
printf "\n Compatibility with Append :"
datagen -P100 -g1M > ${FPREFIX}dg1M
cat ${FPREFIX}dg1M ${FPREFIX}dg1M > ${FPREFIX}2M
lz4 -B5 -v ${FPREFIX}dg1M ${FPREFIX}c
lz4 -d -v ${FPREFIX}c ${FPREFIX}r
lz4 -d -v ${FPREFIX}c -c >> ${FPREFIX}r
ls -ls $FPREFIX*
diff ${FPREFIX}2M ${FPREFIX}r
