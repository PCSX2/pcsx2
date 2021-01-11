add_library(FLAC STATIC
  include/FLAC/all.h
  include/FLAC/assert.h
  include/FLAC/callback.h
  include/FLAC/export.h
  include/FLAC/format.h
  include/FLAC/metadata.h
  include/FLAC/ordinals.h
  include/FLAC/stream_decoder.h
  include/share/alloc.h
  include/share/compat.h
  include/share/endswap.h
  include/share/macros.h
  include/share/safe_str.h
  include/share/windows_unicode_filenames.h
  include/share/win_utf8_io.h
  src/bitmath.c
  src/bitreader.c
  src/cpu.c
  src/crc.c
  src/fixed.c
  src/fixed_intrin_sse2.c
  src/fixed_intrin_ssse3.c
  src/float.c
  src/format.c
  src/include/private/all.h
  src/include/private/bitmath.h
  src/include/private/bitreader.h
  src/include/private/cpu.h
  src/include/private/crc.h
  src/include/private/fixed.h
  src/include/private/float.h
  src/include/private/format.h
  src/include/private/lpc.h
  src/include/private/macros.h
  src/include/private/md5.h
  src/include/private/memory.h
  src/include/private/metadata.h
  src/include/private/window.h
  src/include/protected/stream_decoder.h
  src/lpc.c
  src/lpc_intrin_avx2.c
  src/lpc_intrin_sse.c
  src/lpc_intrin_sse2.c
  src/lpc_intrin_sse41.c
  src/md5.c
  src/memory.c
  src/metadata_iterators.c
  src/metadata_object.c
  src/stream_decoder.c
  src/window.c
)

target_compile_definitions(FLAC PRIVATE PACKAGE_VERSION="1.3.3" FLAC__HAS_OGG=0 HAVE_LROUND HAVE_STDINT_H HAVE_STDLIB_H)

if(CPU_ARCH STREQUAL "x64" OR CPU_ARCH STREQUAL "x86")
  target_compile_definitions(FLAC PRIVATE FLAC__HAS_X86INTRIN=1)
endif()

if(WIN32)
  target_compile_definitions(FLAC PUBLIC FLAC__NO_DLL)
  target_sources(FLAC PRIVATE
    src/windows_unicode_filenames.c
  )
endif()

target_include_directories(FLAC PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/include")
target_include_directories(FLAC INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

