#!/bin/sh

set -e
set -x

datagen -g16KB      | lz4 -12      | lz4 -t
datagen -P10        | lz4 -12B4    | lz4 -t
datagen -g256K      | lz4 -12B4D   | lz4 -t
datagen -g512K -P25 | lz4 -12BD    | lz4 -t
datagen -g1M        | lz4 -12B5    | lz4 -t
datagen -g1M -s2    | lz4 -12B4D   | lz4 -t
datagen -g2M -P99   | lz4 -11B4D   | lz4 -t
datagen -g4M        | lz4 -11vq    | lz4 -qt
datagen -g8M        | lz4 -11B4    | lz4 -t
datagen -g16M -P90  | lz4 -11B5    | lz4 -t
datagen -g32M -P10  | lz4 -11B5D   | lz4 -t
