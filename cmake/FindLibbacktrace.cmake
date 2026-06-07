# - Try to find libbacktrace
# Once done this will define
#  LIBBACKTRACE_FOUND - System has libbacktrace
#  LIBBACKTRACE_INCLUDE_DIRS - The libbacktrace include directories
#  LIBBACKTRACE_LIBRARIES - The libraries needed to use libbacktrace

FIND_PATH(
    LIBBACKTRACE_INCLUDE_DIR backtrace.h
    HINTS /usr/include /usr/local/include
    ${LIBBACKTRACE_PATH_INCLUDES}
)

FIND_LIBRARY(
    LIBBACKTRACE_LIBRARY
    NAMES backtrace
    PATHS ${ADDITIONAL_LIBRARY_PATHS} ${LIBBACKTRACE_PATH_LIB}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libbacktrace DEFAULT_MSG
                                  LIBBACKTRACE_LIBRARY LIBBACKTRACE_INCLUDE_DIR)

if(LIBBACKTRACE_FOUND)
    add_library(libbacktrace::libbacktrace UNKNOWN IMPORTED)
    set_target_properties(libbacktrace::libbacktrace PROPERTIES
        IMPORTED_LOCATION ${LIBBACKTRACE_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${LIBBACKTRACE_INCLUDE_DIR}
    )
endif()

mark_as_advanced(LIBBACKTRACE_INCLUDE_DIR LIBBACKTRACE_LIBRARY)
