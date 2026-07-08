#!/bin/sh

FPREFIX="tmp-tlb"

set -e

remove () {
    rm -rf $FPREFIX*
}

trap remove EXIT

set -x

datagen -g0       | lz4 -v     | lz4 -t
datagen -g16KB    | lz4 -9     | lz4 -t
datagen -g20KB > $FPREFIX-dg20k
lz4 < $FPREFIX-dg20k | lz4 -d > $FPREFIX-dec
diff -q $FPREFIX-dg20k $FPREFIX-dec
lz4 --no-frame-crc < $FPREFIX-dg20k | lz4 -d > $FPREFIX-dec
diff -q $FPREFIX-dg20k $FPREFIX-dec
datagen           | lz4 -BI    | lz4 -t
datagen           | lz4 --no-crc | lz4 -t
datagen -g6M -P99 | lz4 -9BD   | lz4 -t
datagen -g17M     | lz4 -9v    | lz4 -qt
datagen -g33M     | lz4 --no-frame-crc | lz4 -t
datagen -g256MB   | lz4 -vqB4D | lz4 -t --no-crc
echo "hello world" > $FPREFIX-hw
lz4 --rm -f $FPREFIX-hw $FPREFIX-hw.lz4
test ! -f $FPREFIX-hw                   # must fail (--rm)
test   -f $FPREFIX-hw.lz4
lz4 -d --rm -f $FPREFIX-hw.lz4
test ! -f $FPREFIX-hw.lz4
lz4 --rm -f $FPREFIX-hw > /dev/null
test   -f $FPREFIX-hw.lz4               # no more implicit stdout
lz4cat $FPREFIX-hw.lz4 | grep "hello world"
unlz4 --rm $FPREFIX-hw.lz4 $FPREFIX-hw
test   -f $FPREFIX-hw
test ! -f $FPREFIX-hw.lz4               # must fail (--rm)
test ! -f $FPREFIX-hw.lz4.lz4           # must fail (unlz4)
lz4cat $FPREFIX-hw                      # pass-through mode
test   -f $FPREFIX-hw
test ! -f $FPREFIX-hw.lz4               # must fail (lz4cat)
lz4 $FPREFIX-hw $FPREFIX-hw.lz4         # creates $FPREFIX-hw.lz4
lz4cat < $FPREFIX-hw.lz4 > ${FPREFIX}3  # checks lz4cat works with stdin (#285)
diff -q $FPREFIX-hw ${FPREFIX}3
lz4cat < $FPREFIX-hw > ${FPREFIX}2      # checks lz4cat works in pass-through mode
diff -q $FPREFIX-hw ${FPREFIX}2
cp $FPREFIX-hw ./-d
lz4 --rm -- -d -d.lz4                   # compresses ./d into ./-d.lz4
test   -f ./-d.lz4
test ! -f ./-d
mv ./-d.lz4 ./-z
lz4 -d --rm -- -z ${FPREFIX}4           # uncompresses ./-z into $FPREFIX4
test ! -f ./-z
diff -q $FPREFIX-hw ${FPREFIX}4
lz4 ${FPREFIX}2 ${FPREFIX}3 ${FPREFIX}4 && exit 1  # must fail: refuse to handle 3+ file names
mkdir -p ${FPREFIX}-dir
lz4 ${FPREFIX}-dir && exit 1          # must fail: refuse to compress directory
test ! -f ${FPREFIX}-dir.lz4          # must not create artifact (#1211)
lz4 -f $FPREFIX-hw                    # create $FPREFIX-hw.lz4, for next tests
lz4 --list $FPREFIX-hw.lz4            # test --list on valid single-frame file
lz4 --list < $FPREFIX-hw.lz4          # test --list from stdin (file only)
cat $FPREFIX-hw >> $FPREFIX-hw.lz4
lz4 -f $FPREFIX-hw.lz4 && exit 1      # uncompress valid frame followed by invalid data (must fail now)
lz4 -BX $FPREFIX-hw -c -q | lz4 -tv   # test block checksum
# datagen -g20KB generates the same file every single time
# cannot save output of datagen -g20KB as input file to lz4 because the following shell commands are run before datagen -g20KB
test "$(datagen -g20KB | lz4 -c --fast | wc -c)" -lt "$(datagen -g20KB | lz4 -c --fast=9 | wc -c)" # -1 vs -9
test "$(datagen -g20KB | lz4 -c -1 | wc -c)" -lt "$(datagen -g20KB| lz4 -c --fast=1 | wc -c)" # 1 vs -1
test "$(datagen -g20KB | lz4 -c --fast=1 | wc -c)" -eq "$(datagen -g20KB| lz4 -c --fast| wc -c)" # checks default fast compression is -1
lz4 -c --fast=0 $FPREFIX-dg20K && exit 1  # lz4 should fail when fast=0
lz4 -c --fast=-1 $FPREFIX-dg20K && exit 1 # lz4 should fail when fast=-1
# Multithreading commands
datagen -g16M | lz4 -T2 | lz4 -t
datagen -g16M | lz4 --threads=2 | lz4 -t
# High --fast values can result in out-of-bound dereferences #876
datagen -g1M | lz4 -c --fast=999999999 > $FPREFIX-trash
# Test for #596
echo "TEST" > $FPREFIX-test
lz4 -m $FPREFIX-test
lz4 $FPREFIX-test.lz4 $FPREFIX-test2
diff -q $FPREFIX-test $FPREFIX-test2
# bug #1374
datagen -g4194302 | lz4 -B4 -c > $FPREFIX-test3
