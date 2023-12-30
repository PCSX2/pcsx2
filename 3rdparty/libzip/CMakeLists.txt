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

# Replaced these dynamic checks with static, otherwise XCode is slow to configure.
set(HAVE__CLOSE ${MSVC})
set(HAVE__DUP ${MSVC})
set(HAVE__FDOPEN ${MSVC})
set(HAVE__FILENO ${MSVC})
set(HAVE__SETMODE ${MSVC})
set(HAVE__SNPRINTF ${MSVC})
set(HAVE__SNPRINTF_S ${MSVC})
set(HAVE__SNWPRINTF_S ${MSVC})
set(HAVE__STRDUP ${MSVC})
set(HAVE__STRICMP ${MSVC})
set(HAVE__STRTOI64 ${MSVC})
set(HAVE__STRTOUI64 ${MSVC})
set(HAVE__UNLINK ${MSVC})
set(HAVE_ARC4RANDOM ${APPLE})
set(HAVE_CLONEFILE ${APPLE})
set(HAVE_EXPLICIT_BZERO ${LINUX})
set(HAVE_EXPLICIT_MEMSET ${LINUX})
set(HAVE_FCHMOD ${UNIX})
set(HAVE_FILENO TRUE)
set(HAVE_FSEEKO ${UNIX})
set(HAVE_FTELLO ${UNIX})
set(HAVE_GETPROGNAME ${APPLE})
set(HAVE_LOCALTIME_R ${UNIX})
set(HAVE_LOCALTIME_S ${MSVC})
set(HAVE_MEMCPY_S ${MSVC})
set(HAVE_RANDOM ${UNIX})
set(HAVE_SETMODE ${APPLE})
set(HAVE_SNPRINTF TRUE)
set(HAVE_SNPRINTF_S FALSE)
set(HAVE_STRCASECMP ${UNIX})
set(HAVE_STRDUP TRUE)
set(HAVE_STRERROR_S ${MSVC})
set(HAVE_STRERRORLEN_S FALSE)
set(HAVE_STRICMP ${MSVC})
set(HAVE_STRNCPY_S ${MSVC})
set(HAVE_STRTOLL TRUE)
set(HAVE_STRTOULL TRUE)

if(UNIX)
  set(HAVE_FTS_H TRUE)
  check_function_exists(fts_open HAVE_FTS_OPEN)
  if(NOT HAVE_FTS_OPEN)
    check_library_exists(fts fts_open "" HAVE_LIB_FTS)
  else(NOT HAVE_FTS_OPEN)
    set(HAVE_LIB_FTS "")
  endif(NOT HAVE_FTS_OPEN)
else()
  set(HAVE_LIB_FTS "")
endif()

if(HAVE_LIB_FTS)
  set(FTS_LIB fts)
else()
  set(FTS_LIB "")
endif()

set(HAVE_STDBOOL_H TRUE)
set(HAVE_STRINGS_H ${UNIX})
set(HAVE_UNISTD_H ${UNIX})

check_type_size("off_t" SIZEOF_OFF_T)
check_type_size("size_t" SIZEOF_SIZE_T)

if(LINUX)
  check_c_source_compiles("#include <sys/ioctl.h>
  #include <linux/fs.h>
  int main(int argc, char *argv[]) { unsigned long x = FICLONERANGE; }" HAVE_FICLONERANGE)
else()
  set(HAVE_FICLONERANGE FALSE)
endif()

check_c_source_compiles("
int foo(char * _Nullable bar);
int main(int argc, char *argv[]) { }" HAVE_NULLABLE)

set(WORDS_BIGENDIAN FALSE)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

# fixed size integral types

set(LIBZIP_TYPES_INCLUDE "#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>")

set(ZIP_INT8_T int8_t)
set(ZIP_UINT8_T uint8_t)
set(ZIP_INT16_T int16_t)
set(ZIP_UINT16_T uint16_t)
set(ZIP_INT32_T int32_t)
set(ZIP_UINT32_T uint32_t)
set(ZIP_INT64_T int64_t)
set(ZIP_UINT64_T uint64_t)

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
