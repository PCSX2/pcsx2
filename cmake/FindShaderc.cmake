# - Try to find SHADERC
# Once done this will define
#  SHADERC_FOUND - System has SHADERC
#  SHADERC_INCLUDE_DIRS - The SHADERC include directories
#  SHADERC_LIBRARIES - The libraries needed to use SHADERC

FIND_PATH(
    SHADERC_INCLUDE_DIR shaderc/shaderc.h
    HINTS /usr/include /usr/local/include
    ${SHADERC_PATH_INCLUDES}
)

FIND_LIBRARY(
    SHADERC_LIBRARY
    NAMES shaderc_shared
    PATHS ${ADDITIONAL_LIBRARY_PATHS} ${SHADERC_PATH_LIB}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Shaderc DEFAULT_MSG
                                  SHADERC_LIBRARY SHADERC_INCLUDE_DIR)

if(SHADERC_FOUND)
    add_library(Shaderc::shaderc_shared UNKNOWN IMPORTED)
    set_target_properties(Shaderc::shaderc_shared PROPERTIES
        IMPORTED_LOCATION ${SHADERC_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SHADERC_INCLUDE_DIR}
        INTERFACE_COMPILE_DEFINITIONS "SHADERC_SHAREDLIB"
    )
endif()

mark_as_advanced(SHADERC_INCLUDE_DIR SHADERC_LIBRARY)
