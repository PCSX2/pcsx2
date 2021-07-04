# Once done, this will define
#
# LIBC_FOUND - system has libc 
# LIBC_LIBRARIES - link these to use libc

if(LIBC_LIBRARIES)
	set(LIBC_FIND_QUIETLY TRUE)
endif(LIBC_LIBRARIES)

find_library(libm NAMES m)

# OSX doesn't have rt. On Linux timer and aio dependency.
if(APPLE)
	find_library(libdl NAMES dl)
	set(LIBC_LIBRARIES ${librt} ${libdl} ${libm})    
elseif(Linux)
	find_library(libdl NAMES dl)
	find_library(librt NAMES rt)
	set(LIBC_LIBRARIES ${librt} ${libdl} ${libm})
else()
	# FreeBSD doesn't have libdl
	find_library(librt NAMES rt)
	set(LIBC_LIBRARIES ${librt} ${libm})
endif()

# handle the QUIETLY and REQUIRED arguments and set LIBC_FOUND to TRUE if 
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libc DEFAULT_MSG LIBC_LIBRARIES)

mark_as_advanced(LIBC_LIBRARIES)

