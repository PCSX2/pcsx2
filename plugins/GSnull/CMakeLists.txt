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

# GSnull headers
set(GSnullHeaders
	GifTransfer.h
	GS.h
	Registers.h)

# GSnull Linux sources
set(GSnullLinuxSources
	Linux/Config.cpp
	Linux/GSLinux.cpp
	Linux/Linux.cpp)

# GSnull Linux headers
set(GSnullLinuxHeaders
	Linux/Config.h
	Linux/GSLinux.h
	Linux/Linux.h)

# GSnull null sources
set(GSnullnullSources
	null/GSnull.cpp)

# GSnull null headers
set(GSnullnullHeaders
	null/GSnull.h)

add_pcsx2_plugin(GSnull "${GSnullSources} ${GSnullHeaders} ${GSnullnullSources} ${GSnullnullHeaders} ${GSnullLinuxSources} ${GSnullLinuxHeaders}" "${GTK2_LIBRARIES} ${X11_LIBRARIES}")