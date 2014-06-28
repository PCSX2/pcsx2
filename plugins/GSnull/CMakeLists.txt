# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# GSnull sources
set(GSnullSources
	GifTransfer.cpp
	GS.cpp
	GSLog.cpp
	Registers.cpp)

# GSnull Linux sources
set(GSnullLinuxSources
	Linux/Config.cpp
	Linux/GSLinux.cpp
	Linux/Linux.cpp)

# GSnull null sources
set(GSnullnullSources
	null/GSnull.cpp)

set(GSnullFinalSources
	${GSnullSources} 
	${GSnullnullSources} 
	${GSnullLinuxSources})

add_pcsx2_plugin(GSnull "${GSnullFinalSources}" "${CMAKE_THREAD_LIBS_INIT} ${GTK2_LIBRARIES}")