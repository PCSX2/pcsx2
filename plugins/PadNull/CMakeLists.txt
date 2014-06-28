# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# PadNull sources
set(PadNullSources
	Pad.cpp)

# PadNull Linux sources
set(PadNullLinuxSources
	Linux/Config.cpp
	Linux/PadLinux.cpp)

include_directories(Linux)

set(PadNullFinalSources
	${PadNullSources} 
	${PadNullLinuxSources}
)

add_pcsx2_plugin(PadNull "${PadNullFinalSources}" "${CMAKE_THREAD_LIBS_INIT} ${GTK2_LIBRARIES}")
