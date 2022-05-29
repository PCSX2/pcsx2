set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(LIBZIP_DO_INSTALL OFF CACHE BOOL "")

set(ENABLE_COMMONCRYPTO OFF CACHE BOOL "")
set(ENABLE_GNUTLS OFF CACHE BOOL "")
set(ENABLE_MBEDTLS OFF CACHE BOOL "")
set(ENABLE_WINDOWS_CRYPTO OFF CACHE BOOL "")
set(ENABLE_OPENSSL OFF CACHE BOOL "")
set(ENABLE_BZIP2 OFF CACHE BOOL "")
set(ENABLE_LZMA OFF CACHE BOOL "")
set(ENABLE_ZSTD ON CACHE BOOL "")

set(BUILD_TOOLS OFF CACHE BOOL "")
set(BUILD_REGRESS OFF CACHE BOOL "")
set(BUILD_EXAMPLES OFF CACHE BOOL "")
set(BUILD_DOC OFF CACHE BOOL "")

if(WIN32)
	# Needed otherwise find_package() will look in the system.
	set(ZLIB_FOUND TRUE)
	set(ZLIB_LIBRARY "$<TARGET_FILE:pcsx2-zlib>")
	set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib")
endif()

# Fill in the location of zstd, so find_package() doesn't try to use
# a potentially-old system version. It also picks up Zstd::Zstd.
set(Zstd_FOUND TRUE)
set(Zstd_LIBRARY "$<TARGET_FILE:pcsx2-zstd>")
set(Zstd_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zstd/zstd/lib")

add_subdirectory(libzip EXCLUDE_FROM_ALL)
