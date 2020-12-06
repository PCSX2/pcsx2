# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx" AND GSdx)
	add_subdirectory(GSdx)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSnull" AND GSnull)
	add_subdirectory(GSnull)
endif()
