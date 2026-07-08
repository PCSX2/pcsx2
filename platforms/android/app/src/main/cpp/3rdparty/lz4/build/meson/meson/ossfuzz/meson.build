lz4_source_root = '../../../..'

fuzzers = [
  'compress_frame_fuzzer',
  'compress_fuzzer',
  'compress_hc_fuzzer',
  'decompress_frame_fuzzer',
  'decompress_fuzzer',
  'round_trip_frame_uncompressed_fuzzer',
  'round_trip_fuzzer',
  'round_trip_hc_fuzzer',
  'round_trip_stream_fuzzer'
]

c_args = cc.get_supported_arguments(
  '-Wno-unused-function',
  '-Wno-sign-compare',
  '-Wno-declaration-after-statement'
)

foreach f : fuzzers
  lib = static_library(
    f,
    lz4_source_root / 'ossfuzz/@0@.c'.format(f),
    lz4_source_root / 'ossfuzz/lz4_helpers.c',
    lz4_source_root / 'ossfuzz/fuzz_data_producer.c',
    c_args: c_args,
    dependencies: [liblz4_internal_dep]
  )

  executable(
    f,
    lz4_source_root / 'ossfuzz/standaloneengine.c',
    link_with: lib,
    dependencies: [liblz4_internal_dep]
  )
endforeach
