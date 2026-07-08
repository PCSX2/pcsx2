# #############################################################################
# Copyright (c) 2018-present        lzutao <taolzu(at)gmail.com>
# Copyright (c) 2022-present        Tristan Partin <tristan(at)partin.io>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

lz4_source_root = '../../../..'

examples = {
  'print_version': 'print_version.c',
  'blockStreaming_doubleBuffer': 'blockStreaming_doubleBuffer.c',
  'dictionaryRandomAccess': 'dictionaryRandomAccess.c',
  'blockStreaming_ringBuffer': 'blockStreaming_ringBuffer.c',
  'streamingHC_ringBuffer': 'streamingHC_ringBuffer.c',
  'blockStreaming_lineByLine': 'blockStreaming_lineByLine.c',
  'frameCompress': 'frameCompress.c',
  'bench_functions': 'bench_functions.c',
  'simple_buffer': 'simple_buffer.c',
}

foreach e, src : examples
  executable(
    e,
    lz4_source_root / 'examples' / src,
    dependencies: [liblz4_internal_dep],
    install: false
  )
endforeach
