#!/bin/sh

FPREFIX="tmp-dict"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

datagen -g16KB > $FPREFIX
datagen -g32KB > $FPREFIX-sample-32k
< $FPREFIX-sample-32k lz4 -D $FPREFIX | lz4 -dD $FPREFIX | diff - $FPREFIX-sample-32k
datagen -g16MB > $FPREFIX-sample-16m
lz4 -v -B5 $FPREFIX-sample-16m -D $FPREFIX -c | lz4 -d -D $FPREFIX | diff - $FPREFIX-sample-16m

# Check dictionary compression efficiency
size_dict=$( lz4 -3 -B4 $FPREFIX-sample-16m -D $FPREFIX -c | wc -c)
size_nodict=$( lz4 -3 -B4 $FPREFIX-sample-16m -c | wc -c)
if [ "$size_dict" -lt "$size_nodict" ]; then
    echo "Test Passed: dictionary is effective."
else
    echo "Test Failed: dictionary wasn't effective."
    exit 1
fi

touch $FPREFIX-sample-0
< $FPREFIX-sample-0 lz4 -D $FPREFIX | lz4 -dD $FPREFIX | diff - $FPREFIX-sample-0

< $FPREFIX-sample-32k lz4 -D $FPREFIX-sample-0 | lz4 -dD $FPREFIX-sample-0 | diff - $FPREFIX-sample-32k
< $FPREFIX-sample-0 lz4 -D $FPREFIX-sample-0 | lz4 -dD $FPREFIX-sample-0 | diff - $FPREFIX-sample-0
lz4 -bi0 -D $FPREFIX $FPREFIX-sample-32k $FPREFIX-sample-32k

echo "---- test lz4 dictionary loading ----"
datagen -g128KB > $FPREFIX-data-128KB
set -e; \
for l in 0 1 4 128 32767 32768 32769 65535 65536 65537 98303 98304 98305 131071 131072 131073; do \
    datagen -g$l > $FPREFIX-$l; \
    dd if=$FPREFIX-$l of=$FPREFIX-$l-tail bs=1 count=65536 skip=$((l > 65536 ? l - 65536 : 0)); \
    < $FPREFIX-$l      lz4 -D stdin $FPREFIX-data-128KB -c | lz4 -dD $FPREFIX-$l-tail | diff - $FPREFIX-data-128KB; \
    < $FPREFIX-$l-tail lz4 -D stdin $FPREFIX-data-128KB -c | lz4 -dD $FPREFIX-$l      | diff - $FPREFIX-data-128KB; \
done
