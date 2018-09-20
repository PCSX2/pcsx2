# Path to this file.
set(GCR_CMAKE_MACRO_DIR ${CMAKE_CURRENT_LIST_DIR})

# Finds the glib-compile-resources executable.
find_program(GLIB_COMPILE_RESOURCES_EXECUTABLE glib-compile-resources)
mark_as_advanced(GLIB_COMPILE_RESOURCES_EXECUTABLE)

# Include the cmake files containing the functions.
include(${GCR_CMAKE_MACRO_DIR}/CompileGResources.cmake)
include(${GCR_CMAKE_MACRO_DIR}/GenerateGXML.cmake)

