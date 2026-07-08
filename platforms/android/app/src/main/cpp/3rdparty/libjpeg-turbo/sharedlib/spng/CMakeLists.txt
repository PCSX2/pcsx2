set(SRC ${CMAKE_SOURCE_DIR}/src/spng)

if(NOT WITH_SYSTEM_ZLIB)
  include_directories(${SRC}/zlib)

  add_library(zlib OBJECT ${SRC}/zlib/adler32.c ${SRC}/zlib/compress.c
    ${SRC}/zlib/crc32.c ${SRC}/zlib/deflate.c ${SRC}/zlib/inffast.c
    ${SRC}/zlib/inflate.c ${SRC}/zlib/inftrees.c ${SRC}/zlib/trees.c
    ${SRC}/zlib/zutil.c)

  if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(zlib PRIVATE -w)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES SunPro)
    target_compile_options(zlib PRIVATE -erroff)
  endif()
endif()

include_directories(${SRC})
add_definitions(-DSPNG_STATIC)

add_library(spng OBJECT ${SRC}/spng.c)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(spng PRIVATE -w)
endif()
if(CMAKE_C_COMPILER_ID MATCHES SunPro)
  target_compile_options(spng PRIVATE -erroff)
endif()
