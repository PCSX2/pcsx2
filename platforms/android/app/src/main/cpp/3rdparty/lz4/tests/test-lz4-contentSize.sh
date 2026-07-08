#!/bin/sh

FPREFIX="tmp-lzc"

set -e

remove () {
    rm $FPREFIX*
}

trap remove EXIT

set -x

datagen -g15M > $FPREFIX
lz4 -v $FPREFIX -c | lz4 -t
lz4 -v --content-size $FPREFIX -c | lz4 -d > $FPREFIX-dup
diff $FPREFIX $FPREFIX-dup
lz4 -f $FPREFIX -c > $FPREFIX.lz4 # compressed with content size
lz4 --content-size $FPREFIX -c > $FPREFIX-wcz.lz4
diff $FPREFIX.lz4 $FPREFIX-wcz.lz4 && exit 1 # must differ, due to content size
lz4 --content-size < $FPREFIX > $FPREFIX-wcz2.lz4 # can determine content size because stdin is just a file
diff $FPREFIX-wcz.lz4 $FPREFIX-wcz2.lz4  # both must contain content size
cat $FPREFIX | lz4 > $FPREFIX-ncz.lz4
diff $FPREFIX.lz4 $FPREFIX-ncz.lz4  # both don't have content size
cat $FPREFIX | lz4 --content-size > $FPREFIX-ncz2.lz4 # can't determine content size
diff $FPREFIX.lz4 $FPREFIX-ncz2.lz4  # both don't have content size
