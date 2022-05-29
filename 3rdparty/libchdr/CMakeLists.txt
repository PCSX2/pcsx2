add_library(chdr-static STATIC
  libchdr/src/libchdr_bitstream.c
  libchdr/src/libchdr_cdrom.c
  libchdr/src/libchdr_chd.c
  libchdr/src/libchdr_flac.c
  libchdr/src/libchdr_huffman.c
)

target_include_directories(chdr-static PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libchdr/include")
target_link_libraries(chdr-static PRIVATE ZLIB::ZLIB LZMA::LZMA)
target_compile_options(chdr-static PRIVATE "-w")
