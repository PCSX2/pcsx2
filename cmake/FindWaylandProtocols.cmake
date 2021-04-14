# - Try to find wayland-protocols
# Once done this will define
#
# WAYLAND_PROTOCOLS_FOUND - system has wayland-protocols
# WAYLAND_PROTOCOLS_DIR - the directory containing Wayland protocol definitions

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} wayland-protocols --variable=pkgdatadir
    OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# handle the QUIETLY and REQUIRED arguments and set WAYLAND_PROTOCOLS_FOUND to TRUE if
# all listed variables are TRUE
include( "FindPackageHandleStandardArgs" )
find_package_handle_standard_args(wayland-protocols DEFAULT_MSG WAYLAND_PROTOCOLS_DIR)
mark_as_advanced(WAYLAND_PROTOCOLS_DIR)
