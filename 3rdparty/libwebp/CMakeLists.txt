# Intentionally only includes the decoder, not the encoder, to save on binary size.

add_library(pcsx2-libwebp
  libwebp/src/dec/alpha_dec.c
  libwebp/src/dec/alphai_dec.h
  libwebp/src/dec/buffer_dec.c
  libwebp/src/dec/common_dec.h
  libwebp/src/dec/frame_dec.c
  libwebp/src/dec/idec_dec.c
  libwebp/src/dec/io_dec.c
  libwebp/src/dec/quant_dec.c
  libwebp/src/dec/tree_dec.c
  libwebp/src/dec/vp8_dec.c
  libwebp/src/dec/vp8_dec.h
  libwebp/src/dec/vp8i_dec.h
  libwebp/src/dec/vp8l_dec.c
  libwebp/src/dec/vp8li_dec.h
  libwebp/src/dec/webp_dec.c
  libwebp/src/dec/webpi_dec.h
  libwebp/src/dsp/alpha_processing.c
  libwebp/src/dsp/alpha_processing_sse2.c
  libwebp/src/dsp/alpha_processing_sse41.c
  libwebp/src/dsp/common_sse2.h
  libwebp/src/dsp/common_sse41.h
  libwebp/src/dsp/cpu.c
  libwebp/src/dsp/cpu.h
  libwebp/src/dsp/dec.c
  libwebp/src/dsp/dec_clip_tables.c
  libwebp/src/dsp/dec_sse2.c
  libwebp/src/dsp/dec_sse41.c
  libwebp/src/dsp/dsp.h
  libwebp/src/dsp/filters.c
  libwebp/src/dsp/filters_sse2.c
  libwebp/src/dsp/lossless.c
  libwebp/src/dsp/lossless.h
  libwebp/src/dsp/lossless_common.h
  libwebp/src/dsp/lossless_sse2.c
  libwebp/src/dsp/lossless_sse41.c
  libwebp/src/dsp/rescaler.c
  libwebp/src/dsp/rescaler_sse2.c
  libwebp/src/dsp/upsampling.c
  libwebp/src/dsp/upsampling_sse2.c
  libwebp/src/dsp/upsampling_sse41.c
  libwebp/src/dsp/yuv.c
  libwebp/src/dsp/yuv.h
  libwebp/src/dsp/yuv_sse2.c
  libwebp/src/dsp/yuv_sse41.c
  libwebp/src/utils/bit_reader_utils.c
  libwebp/src/utils/bit_reader_utils.h
  libwebp/src/utils/color_cache_utils.c
  libwebp/src/utils/color_cache_utils.h
  libwebp/src/utils/huffman_utils.c
  libwebp/src/utils/huffman_utils.h
  libwebp/src/utils/quant_levels_dec_utils.c
  libwebp/src/utils/quant_levels_dec_utils.h
  libwebp/src/utils/random_utils.c
  libwebp/src/utils/random_utils.h
  libwebp/src/utils/rescaler_utils.c
  libwebp/src/utils/rescaler_utils.h
  libwebp/src/utils/thread_utils.c
  libwebp/src/utils/thread_utils.h
  libwebp/src/utils/utils.c
  libwebp/src/utils/utils.h
  libwebp/src/webp/decode.h
  libwebp/src/webp/demux.h
  libwebp/src/webp/encode.h
  libwebp/src/webp/format_constants.h
  libwebp/src/webp/mux.h
  libwebp/src/webp/mux_types.h
  libwebp/src/webp/types.h
)

if(NOT MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# clang-cl won't build SSE4 functions without -msse4.1.
	target_compile_options(pcsx2-libwebp PRIVATE "-msse4.1")
endif()

target_include_directories(pcsx2-libwebp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libwebp")
target_include_directories(pcsx2-libwebp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/libwebp/src")
add_library(WebP::libwebp ALIAS pcsx2-libwebp)

disable_compiler_warnings_for_target(pcsx2-libwebp)
