# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

set(USBnullSources
	Config.cpp
	USB.cpp)

# USBnull headers
set(USBnullHeaders
	Config.h
	USB.h)

add_pcsx2_plugin(USBnull "${USBnullSources} ${USBnullHeaders}" "${GTK2_LIBRARIES}")
