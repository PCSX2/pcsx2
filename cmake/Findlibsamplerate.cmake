# - Try to find libsamplerate
# Once done this will define
#
# LIBSAMPLERATE_FOUND - system has libsamplerate
# LIBSAMPLERATE_INCLUDE_DIRS - the libsamplerate include directory
# LIBSAMPLERATE_LIBRARIES - The libsamplerate libraries

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules (LIBSAMPLERATE samplerate)
  list(APPEND LIBSAMPLERATE_INCLUDE_DIRS ${LIBSAMPLERATE_INCLUDEDIR})
endif()

if(NOT LIBSAMPLERATE_FOUND)
  find_path( LIBSAMPLERATE_INCLUDE_DIRS "samplerate.h"
             PATH_SUFFIXES "samplerate" )
  find_library( LIBSAMPLERATE_LIBRARIES samplerate)
endif()

# handle the QUIETLY and REQUIRED arguments and set SAMPLERATE_FOUND to TRUE if
# all listed variables are TRUE
include( "FindPackageHandleStandardArgs" )
find_package_handle_standard_args(libsamplerate DEFAULT_MSG LIBSAMPLERATE_INCLUDE_DIRS LIBSAMPLERATE_LIBRARIES)

mark_as_advanced(LIBSAMPLERATE_INCLUDE_DIRS LIBSAMPLERATE_LIBRARIES)
