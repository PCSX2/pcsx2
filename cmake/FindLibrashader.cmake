# Finds librashader.
#
# This module defines:
# LIBRASHADER_FOUND
# LIBRASHADER_INCLUDE_DIR
# LIBRASHADER_LIBRARY

find_path(LIBRASHADER_INCLUDE_DIR NAMES librashader/librashader.h)

find_library(LIBRASHADER_LIBRARY_DEBUG NAMES librashaderd librashader)
find_library(LIBRASHADER_LIBRARY_RELEASE NAMES librashader)

include(SelectLibraryConfigurations)
SELECT_LIBRARY_CONFIGURATIONS(LIBRASHADER)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    Librashader DEFAULT_MSG
    LIBRASHADER_LIBRARY LIBRASHADER_INCLUDE_DIR
)

mark_as_advanced(LIBRASHADER_INCLUDE_DIR LIBRASHADER_LIBRARY)

if(LIBRASHADER_FOUND AND NOT (TARGET Librashader::Librashader))
  add_library(Librashader::Librashader UNKNOWN IMPORTED)
  set_target_properties(Librashader::Librashader
    PROPERTIES
    IMPORTED_LOCATION ${LIBRASHADER_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBRASHADER_INCLUDE_DIR})
endif()
