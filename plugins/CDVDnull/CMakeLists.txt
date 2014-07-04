# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# CDVDnull sources
set(CDVDnullSources
	CDVD.cpp)

add_pcsx2_plugin(CDVDnull "${CDVDnullSources}" "${CMAKE_THREAD_LIBS_INIT} ${GTK2_LIBRARIES}")

