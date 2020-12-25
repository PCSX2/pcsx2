# - Try to find libchdr include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(CHD)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CHD_ROOT_DIR             Set this variable to the root installation of
#                            libchdr if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  CHD_FOUND                System has libchdr, include and library dirs found
#  CHD_INCLUDE_DIR          The libchdr include directories.
#  CHD_LIBRARIES            The libchdr libraries to link to.

find_path(CHD_ROOT_DIR
    NAMES src/chd.h
    HINTS 3rdparty/libchdr
)

find_path(CHD_INCLUDE_DIR
    NAMES chd.h
    HINTS ${CHD_ROOT_DIR}/src
)

set(CHD_LIBRARIES chdr-static)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CHD DEFAULT_MSG
  CHD_ROOT_DIR
)

mark_as_advanced(
    CHD_ROOT_DIR
    CHD_INCLUDE_DIR
)
