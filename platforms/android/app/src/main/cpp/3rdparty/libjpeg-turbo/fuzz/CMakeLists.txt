if(NOT ENABLE_STATIC)
  message(FATAL_ERROR "Fuzz targets require static libraries.")
endif()
if(NOT WITH_TURBOJPEG)
  message(FATAL_ERROR "Fuzz targets require the TurboJPEG API library.")
endif()

set(FUZZ_BINDIR "" CACHE PATH
  "Directory into which fuzz targets should be installed")
if(NOT FUZZ_BINDIR)
  message(FATAL_ERROR "FUZZ_BINDIR must be specified.")
endif()
message(STATUS "FUZZ_BINDIR = ${FUZZ_BINDIR}")

set(FUZZ_LIBRARY "" CACHE STRING
  "Path to fuzzer library or flags necessary to link with it")
if(NOT FUZZ_LIBRARY)
  message(FATAL_ERROR "FUZZ_LIBRARY must be specified.")
endif()
message(STATUS "FUZZ_LIBRARY = ${FUZZ_LIBRARY}")

enable_language(CXX)

set(EFFECTIVE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UC}}")
message(STATUS "C++ Compiler flags = ${EFFECTIVE_CXX_FLAGS}")

add_executable(cjpeg_fuzzer${FUZZER_SUFFIX} cjpeg.cc ../src/cdjpeg.c
  ../src/rdbmp.c ../src/rdgif.c
  ../src/wrapper/rdpng-8.c ../src/wrapper/rdpng-12.c ../src/wrapper/rdpng-16.c
  ${ZLIB_STATIC_SOURCES} ${SPNG_STATIC_SOURCES}
  ../src/wrapper/rdppm-8.c ../src/wrapper/rdppm-12.c ../src/wrapper/rdppm-16.c
  ../src/rdswitch.c ../src/rdtarga.c)
set_property(TARGET cjpeg_fuzzer${FUZZER_SUFFIX} PROPERTY COMPILE_FLAGS
  ${CDJPEG_COMPILE_FLAGS})
target_link_libraries(cjpeg_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY} jpeg-static)
if(SPNG_LIBRARY)
  target_link_libraries(cjpeg_fuzzer${FUZZER_SUFFIX} ${SPNG_LIBRARY})
endif()
if(ZLIB_LIBRARY)
  target_link_libraries(cjpeg_fuzzer${FUZZER_SUFFIX} ${ZLIB_LIBRARY})
endif()
install(TARGETS cjpeg_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

macro(add_fuzz_target target source_file)
  add_executable(${target}_fuzzer${FUZZER_SUFFIX} ${source_file})
  target_link_libraries(${target}_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
    turbojpeg-static)
  install(TARGETS ${target}_fuzzer${FUZZER_SUFFIX}
    RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)
endmacro()

add_fuzz_target(compress compress.cc)

add_fuzz_target(compress_yuv compress_yuv.cc)

add_fuzz_target(compress_lossless compress_lossless.cc)

add_fuzz_target(compress12 compress12.cc)

add_fuzz_target(compress12_lossless compress12_lossless.cc)

add_fuzz_target(compress16_lossless compress16_lossless.cc)

# NOTE: This target is named libjpeg_turbo_fuzzer instead of decompress_fuzzer
# in order to preserve the corpora from Google's OSS-Fuzz target for
# libjpeg-turbo, which this target replaces.
add_fuzz_target(libjpeg_turbo decompress.cc)

add_executable(decompress_libjpeg_fuzzer${FUZZER_SUFFIX} decompress_libjpeg.cc)
target_link_libraries(decompress_libjpeg_fuzzer${FUZZER_SUFFIX} ${FUZZ_LIBRARY}
  jpeg-static)
install(TARGETS decompress_libjpeg_fuzzer${FUZZER_SUFFIX}
  RUNTIME DESTINATION ${FUZZ_BINDIR} COMPONENT bin)

add_fuzz_target(decompress_yuv decompress_yuv.cc)

add_fuzz_target(transform transform.cc)
