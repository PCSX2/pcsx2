#!/bin/bash

set -e

# This script copies all the source files needed for the GNU demangler from a
# copy of GCC to the specified output directory.

if [ "$#" -ne 2 ]; then
	echo "usage: $0 <gcc dir> <out dir>"
	exit 1
fi

GCC_DIR=$1
OUT_DIR=$2

cp "$GCC_DIR/include/ansidecl.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/demangle.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/dyn-string.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/environ.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/getopt.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/libiberty.h" "$OUT_DIR/include/"
cp "$GCC_DIR/include/safe-ctype.h" "$OUT_DIR/include/"
cp "$GCC_DIR/libiberty/alloca.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/argv.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/cp-demangle.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/cp-demangle.h" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/cplus-dem.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/d-demangle.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/dyn-string.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/getopt.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/getopt1.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/rust-demangle.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/safe-ctype.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/xexit.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/xmalloc.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/xmemdup.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/xstrdup.c" "$OUT_DIR/src/"
cp "$GCC_DIR/libiberty/testsuite/demangle-expected" "$OUT_DIR/testsuite/"
cp "$GCC_DIR/libiberty/testsuite/demangler-fuzzer.c" "$OUT_DIR/testsuite/"
cp "$GCC_DIR/libiberty/testsuite/test-demangle.c" "$OUT_DIR/testsuite/"
