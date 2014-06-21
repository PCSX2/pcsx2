# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# dev9null sources
set(dev9nullSources
	Config.cpp
	DEV9.cpp)

# dev9null headers
set(dev9nullHeaders
	Config.h
	DEV9.h)

add_pcsx2_plugin(dev9null "${dev9nullSources} ${dev9nullHeaders}" "${GTK2_LIBRARIES}")