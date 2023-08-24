cmake_minimum_required(VERSION 3.5.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if (${CMAKE_VERSION} VERSION_LESS "3.17.0")
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-compat)
endif()

project(libzip
  VERSION 1.10.1
  LANGUAGES C)

set(ENABLE_FDOPEN ON)
set(HAVE_LIBZSTD TRUE)
set(ZIP_STATIC TRUE)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceRuns)
include(CheckCSourceCompiles)
include(CheckStructHasMember)
include(TestBigEndian)

# Checks

# Request ISO C secure library functions (memcpy_s &c)
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D__STDC_WANT_LIB_EXT1__=1)

check_function_exists(_close HAVE__CLOSE)
check_function_exists(_dup HAVE__DUP)
check_function_exists(_fdopen HAVE__FDOPEN)
check_function_exists(_fileno HAVE__FILENO)
check_function_exists(_setmode HAVE__SETMODE)
check_symbol_exists(_snprintf stdio.h HAVE__SNPRINTF)
check_symbol_exists(_snprintf_s stdio.h HAVE__SNPRINTF_S)
check_symbol_exists(_snwprintf_s stdio.h HAVE__SNWPRINTF_S)
check_function_exists(_strdup HAVE__STRDUP)
check_symbol_exists(_stricmp string.h HAVE__STRICMP)
check_function_exists(_strtoi64 HAVE__STRTOI64)
check_function_exists(_strtoui64 HAVE__STRTOUI64)
check_function_exists(_unlink HAVE__UNLINK)
check_function_exists(arc4random HAVE_ARC4RANDOM)
check_function_exists(clonefile HAVE_CLONEFILE)
check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)
check_function_exists(explicit_memset HAVE_EXPLICIT_MEMSET)
check_function_exists(fchmod HAVE_FCHMOD)
check_function_exists(fileno HAVE_FILENO)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(ftello HAVE_FTELLO)
check_function_exists(getprogname HAVE_GETPROGNAME)
check_symbol_exists(localtime_r time.h HAVE_LOCALTIME_R)
check_symbol_exists(localtime_s time.h HAVE_LOCALTIME_S)
check_function_exists(memcpy_s HAVE_MEMCPY_S)
check_function_exists(random HAVE_RANDOM)
check_function_exists(setmode HAVE_SETMODE)
check_symbol_exists(snprintf stdio.h HAVE_SNPRINTF)
check_symbol_exists(snprintf_s stdio.h HAVE_SNPRINTF_S)
check_symbol_exists(strcasecmp strings.h HAVE_STRCASECMP)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strerror_s HAVE_STRERROR_S)
check_function_exists(strerrorlen_s HAVE_STRERRORLEN_S)
check_function_exists(stricmp HAVE_STRICMP)
check_function_exists(strncpy_s HAVE_STRNCPY_S)
check_function_exists(strtoll HAVE_STRTOLL)
check_function_exists(strtoull HAVE_STRTOULL)

check_include_files("sys/types.h;sys/stat.h;fts.h" HAVE_FTS_H)
# fts functions may be in external library
if(HAVE_FTS_H)
  check_function_exists(fts_open HAVE_FTS_OPEN)
  if(NOT HAVE_FTS_OPEN)
    check_library_exists(fts fts_open "" HAVE_LIB_FTS)
  else(NOT HAVE_FTS_OPEN)
    set(HAVE_LIB_FTS "" CACHE INTERNAL "")
  endif(NOT HAVE_FTS_OPEN)
else(HAVE_FTS_H)
  set(HAVE_LIB_FTS "" CACHE INTERNAL "")
endif(HAVE_FTS_H)

if(HAVE_LIB_FTS)
  set(FTS_LIB fts CACHE INTERNAL "")
else()
  set(FTS_LIB "" CACHE INTERNAL "")
endif()

check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(unistd.h HAVE_UNISTD_H)

check_include_files(inttypes.h HAVE_INTTYPES_H_LIBZIP)
check_include_files(stdint.h HAVE_STDINT_H_LIBZIP)
check_include_files(sys/types.h HAVE_SYS_TYPES_H_LIBZIP)

# TODO: fix test
# this test does not find __progname even when it exists
#check_symbol_exists(__progname stdlib.h HAVE___PROGNAME)

check_type_size(__int8 __INT8_LIBZIP)
check_type_size(int8_t INT8_T_LIBZIP)
check_type_size(uint8_t UINT8_T_LIBZIP)
check_type_size(__int16 __INT16_LIBZIP)
check_type_size(int16_t INT16_T_LIBZIP)
check_type_size(uint16_t UINT16_T_LIBZIP)
check_type_size(__int32 __INT32_LIBZIP)
check_type_size(int32_t INT32_T_LIBZIP)
check_type_size(uint32_t UINT32_T_LIBZIP)
check_type_size(__int64 __INT64_LIBZIP)
check_type_size(int64_t INT64_T_LIBZIP)
check_type_size(uint64_t UINT64_T_LIBZIP)
check_type_size("short" SHORT_LIBZIP)
check_type_size("int" INT_LIBZIP)
check_type_size("long" LONG_LIBZIP)
check_type_size("long long" LONG_LONG_LIBZIP)
check_type_size("off_t" SIZEOF_OFF_T)
check_type_size("size_t" SIZEOF_SIZE_T)

check_c_source_compiles("#include <sys/ioctl.h>
#include <linux/fs.h>
int main(int argc, char *argv[]) { unsigned long x = FICLONERANGE; }" HAVE_FICLONERANGE)

check_c_source_compiles("
int foo(char * _Nullable bar);
int main(int argc, char *argv[]) { }" HAVE_NULLABLE)

test_big_endian(WORDS_BIGENDIAN)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

# fixed size integral types

if(HAVE_INTTYPES_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>")
elseif(HAVE_STDINT_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#include <stdint.h>")
elseif(HAVE_SYS_TYPES_H_LIBZIP)
  set(LIBZIP_TYPES_INCLUDE "#include <sys/types.h>")
endif()

if(HAVE_INT8_T_LIBZIP)
  set(ZIP_INT8_T int8_t)
elseif(HAVE___INT8_LIBZIP)
  set(ZIP_INT8_T __int8)
else()
  set(ZIP_INT8_T "signed char")
endif()

if(HAVE_UINT8_T_LIBZIP)
  set(ZIP_UINT8_T uint8_t)
elseif(HAVE___INT8_LIBZIP)
  set(ZIP_UINT8_T "unsigned __int8")
else()
  set(ZIP_UINT8_T "unsigned char")
endif()

if(HAVE_INT16_T_LIBZIP)
  set(ZIP_INT16_T int16_t)
elseif(HAVE___INT16_LIBZIP)
  set(INT16_T_LIBZIP __int16)
elseif(SHORT_LIBZIP EQUAL 2)
  set(INT16_T_LIBZIP short)
endif()

if(HAVE_UINT16_T_LIBZIP)
  set(ZIP_UINT16_T uint16_t)
elseif(HAVE___INT16_LIBZIP)
  set(UINT16_T_LIBZIP "unsigned __int16")
elseif(SHORT_LIBZIP EQUAL 2)
  set(UINT16_T_LIBZIP "unsigned short")
endif()

if(HAVE_INT32_T_LIBZIP)
  set(ZIP_INT32_T int32_t)
elseif(HAVE___INT32_LIBZIP)
  set(ZIP_INT32_T __int32)
elseif(INT_LIBZIP EQUAL 4)
  set(ZIP_INT32_T int)
elseif(LONG_LIBZIP EQUAL 4)
  set(ZIP_INT32_T long)
endif()

if(HAVE_UINT32_T_LIBZIP)
  set(ZIP_UINT32_T uint32_t)
elseif(HAVE___INT32_LIBZIP)
  set(ZIP_UINT32_T "unsigned __int32")
elseif(INT_LIBZIP EQUAL 4)
  set(ZIP_UINT32_T "unsigned int")
elseif(LONG_LIBZIP EQUAL 4)
  set(ZIP_UINT32_T "unsigned long")
endif()

if(HAVE_INT64_T_LIBZIP)
  set(ZIP_INT64_T int64_t)
elseif(HAVE___INT64_LIBZIP)
  set(ZIP_INT64_T __int64)
elseif(LONG_LIBZIP EQUAL 8)
  set(ZIP_INT64_T long)
elseif(LONG_LONG_LIBZIP EQUAL 8)
  set(ZIP_INT64_T "long long")
endif()

if(HAVE_UINT64_T_LIBZIP)
  set(ZIP_UINT64_T uint64_t)
elseif(HAVE___INT64_LIBZIP)
  set(ZIP_UINT64_T "unsigned __int64")
elseif(LONG_LIBZIP EQUAL 8)
  set(ZIP_UINT64_T "unsigned long")
elseif(LONG_LONG_LIBZIP EQUAL 8)
  set(ZIP_UINT64_T "unsigned long long")
endif()

if(HAVE_NULLABLE)
  set(ZIP_NULLABLE_DEFINES)
else()
  set(ZIP_NULLABLE_DEFINES "#define _Nullable
#define _Nonnull")
endif()

# write out config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-config.h.in ${PROJECT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake-zipconf.h.in ${PROJECT_BINARY_DIR}/zipconf.h)

set(CMAKE_C_VISIBILITY_PRESET hidden)

add_library(zip STATIC
  lib/zip_add.c
  lib/zip_add_dir.c
  lib/zip_add_entry.c
  lib/zip_algorithm_deflate.c
  lib/zip_buffer.c
  lib/zip_close.c
  lib/zip_delete.c
  lib/zip_dir_add.c
  lib/zip_dirent.c
  lib/zip_discard.c
  lib/zip_entry.c
  lib/zip_error.c
  lib/zip_error_clear.c
  lib/zip_error_get.c
  lib/zip_error_get_sys_type.c
  lib/zip_error_strerror.c
  lib/zip_error_to_str.c
  lib/zip_extra_field.c
  lib/zip_extra_field_api.c
  lib/zip_fclose.c
  lib/zip_fdopen.c
  lib/zip_file_add.c
  lib/zip_file_error_clear.c
  lib/zip_file_error_get.c
  lib/zip_file_get_comment.c
  lib/zip_file_get_external_attributes.c
  lib/zip_file_get_offset.c
  lib/zip_file_rename.c
  lib/zip_file_replace.c
  lib/zip_file_set_comment.c
  lib/zip_file_set_encryption.c
  lib/zip_file_set_external_attributes.c
  lib/zip_file_set_mtime.c
  lib/zip_file_strerror.c
  lib/zip_fopen.c
  lib/zip_fopen_encrypted.c
  lib/zip_fopen_index.c
  lib/zip_fopen_index_encrypted.c
  lib/zip_fread.c
  lib/zip_fseek.c
  lib/zip_ftell.c
  lib/zip_get_archive_comment.c
  lib/zip_get_archive_flag.c
  lib/zip_get_encryption_implementation.c
  lib/zip_get_file_comment.c
  lib/zip_get_name.c
  lib/zip_get_num_entries.c
  lib/zip_get_num_files.c
  lib/zip_hash.c
  lib/zip_io_util.c
  lib/zip_libzip_version.c
  lib/zip_memdup.c
  lib/zip_name_locate.c
  lib/zip_new.c
  lib/zip_open.c
  lib/zip_pkware.c
  lib/zip_progress.c
  lib/zip_rename.c
  lib/zip_replace.c
  lib/zip_set_archive_comment.c
  lib/zip_set_archive_flag.c
  lib/zip_set_default_password.c
  lib/zip_set_file_comment.c
  lib/zip_set_file_compression.c
  lib/zip_set_name.c
  lib/zip_source_accept_empty.c
  lib/zip_source_begin_write.c
  lib/zip_source_begin_write_cloning.c
  lib/zip_source_buffer.c
  lib/zip_source_call.c
  lib/zip_source_close.c
  lib/zip_source_commit_write.c
  lib/zip_source_compress.c
  lib/zip_source_crc.c
  lib/zip_source_error.c
  lib/zip_source_file_common.c
  lib/zip_source_file_stdio.c
  lib/zip_source_free.c
  lib/zip_source_function.c
  lib/zip_source_get_file_attributes.c
  lib/zip_source_is_deleted.c
  lib/zip_source_layered.c
  lib/zip_source_open.c
  lib/zip_source_pass_to_lower_layer.c
  lib/zip_source_pkware_decode.c
  lib/zip_source_pkware_encode.c
  lib/zip_source_read.c
  lib/zip_source_remove.c
  lib/zip_source_rollback_write.c
  lib/zip_source_seek.c
  lib/zip_source_seek_write.c
  lib/zip_source_stat.c
  lib/zip_source_supports.c
  lib/zip_source_tell.c
  lib/zip_source_tell_write.c
  lib/zip_source_window.c
  lib/zip_source_write.c
  lib/zip_source_zip.c
  lib/zip_source_zip_new.c
  lib/zip_stat.c
  lib/zip_stat_index.c
  lib/zip_stat_init.c
  lib/zip_strerror.c
  lib/zip_string.c
  lib/zip_unchange.c
  lib/zip_unchange_all.c
  lib/zip_unchange_archive.c
  lib/zip_unchange_data.c
  lib/zip_utf-8.c
  ${CMAKE_CURRENT_BINARY_DIR}/zip_err_str.c
  )

if(WIN32)
  target_sources(zip PRIVATE
    lib/zip_source_file_win32.c
    lib/zip_source_file_win32_named.c
    lib/zip_source_file_win32_utf16.c
    lib/zip_source_file_win32_utf8.c
    lib/zip_source_file_win32_ansi.c
    lib/zip_random_win32.c
    )
else(WIN32)
  target_sources(zip PRIVATE
    lib/zip_source_file_stdio_named.c
    lib/zip_random_unix.c
    )
endif(WIN32)

target_sources(zip PRIVATE lib/zip_algorithm_zstd.c)
target_link_libraries(zip PRIVATE Zstd::Zstd)

target_link_libraries(zip PRIVATE ZLIB::ZLIB)
target_include_directories(zip
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

# create zip_err_str.c from zip.h and zipint.h
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zip_err_str.c
  COMMAND "${CMAKE_COMMAND}"
    "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DCMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/GenerateZipErrorStrings.cmake"
  DEPENDS
    ${PROJECT_SOURCE_DIR}/cmake/GenerateZipErrorStrings.cmake
    ${PROJECT_SOURCE_DIR}/lib/zip.h
    ${PROJECT_SOURCE_DIR}/lib/zipint.h
  COMMENT "Generating zip_err_str.c"
)

add_library(libzip::zip ALIAS zip)
