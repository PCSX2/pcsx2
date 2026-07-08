#!/bin/bash

set -u
set -e

FUZZER_SUFFIX=
if [ $# -ge 1 ]; then
	FUZZER_SUFFIX="$1"
	FUZZER_SUFFIX="`echo $1 | sed 's/\./_/g'`"
fi

if [ "$SANITIZER" = "memory" ]; then
	export CFLAGS="$CFLAGS -DZERO_BUFFERS=1"
fi

cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_STATIC=1 -DENABLE_SHARED=0 \
	-DCMAKE_C_FLAGS_RELWITHDEBINFO="-g -DNDEBUG" \
	-DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-g -DNDEBUG" -DCMAKE_INSTALL_PREFIX=$WORK \
	-DWITH_FUZZ=1 -DFUZZ_BINDIR=$OUT -DFUZZ_LIBRARY=$LIB_FUZZING_ENGINE \
	-DFUZZER_SUFFIX="$FUZZER_SUFFIX"
make "-j$(nproc)" "--load-average=$(nproc)"
make install

for fuzzer in cjpeg \
	compress \
	compress_yuv \
	compress_lossless \
	compress12 \
	compress12_lossless \
	compress16_lossless; do
	cp $SRC/compress_fuzzer_seed_corpus.zip $OUT/${fuzzer}_fuzzer${FUZZER_SUFFIX}_seed_corpus.zip
done

FUZZ_DIR=$(dirname "$0")

for fuzzer in libjpeg_turbo \
	decompress_libjpeg \
	decompress_yuv \
	transform; do
	cp $SRC/decompress_fuzzer_seed_corpus.zip $OUT/${fuzzer}_fuzzer${FUZZER_SUFFIX}_seed_corpus.zip
	if [ -f "$FUZZ_DIR/jpeg.dict" ]; then
		cp "$FUZZ_DIR/jpeg.dict" $OUT/${fuzzer}_fuzzer${FUZZER_SUFFIX}.dict
	fi
done
