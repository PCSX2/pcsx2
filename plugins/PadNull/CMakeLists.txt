# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# PadNull sources
set(PadNullSources
	Pad.cpp)

# PadNull headers
set(PadNullHeaders
	Pad.h)

# PadNull Linux sources
set(PadNullLinuxSources
	Linux/Config.cpp
	Linux/PadLinux.cpp)

# PadNull Linux headers
set(PadNullLinuxHeaders
	Linux/Config.h
	Linux/PadLinux.h)

include_directories(Linux)

add_pcsx2_plugin(PadNull "${PadNullSources} ${PadNullHeaders} ${PadNullLinuxSources} ${PadNullLinuxHeaders}" "${GTK2_LIBRARIES} ${X11_LIBRARIES}")
