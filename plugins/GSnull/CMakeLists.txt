# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output GSnull)

set(GSnullFinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
	)

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

# GSnull Windows sources
set(GSnullWindowsSources
	Windows/Config.cpp
	Windows/GS.def
	Windows/GS.rc
	Windows/GSwin.cpp
	Windows/Win32.cpp)

# GSnull Windows headers
set(GSnullWindowsHeaders
	)

set(GSnullFinalSources
	${GSnullSources}
	${GSnullHeaders}
	${GSnullnullSources}
	${GSnullnullHeaders}
	${GSnullLinuxSources}
	${GSnullLinuxHeaders}
)

set(GSnullFinalLibs
	${GTK2_LIBRARIES}
	${X11_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${GSnullFinalSources}" "${GSnullFinalLibs}" "${GSnullFinalFlags}")
target_compile_features(${Output} PRIVATE cxx_std_17)