# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# plugin name
set(Output PADnull)

set(CommonFlags
    -fvisibility=hidden
    -Wall
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
	# add defines
    set(PadNullFinalFlags 
    	${CommonFlags} -g
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
	# add defines
	set(PadNullFinalFlags 
		${CommonFlags} ${OptimizationFlags}
	)
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
	# add defines
	set(PadNullFinalFlags 
		${CommonFlags} ${OptimizationFlags}
	)
endif(CMAKE_BUILD_TYPE STREQUAL Release)

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

# PadNull Windows sources
set(PadNullWindowsSources
	Windows/Config.cpp
	Windows/PadNull.rc
	Windows/PadWin.cpp
	Windows/Win32.cpp)

# PadNull Windows headers
set(PadNullWindowsHeaders
	Windows/PadWin.h
	Windows/resource.h)

# add additional include directories
include_directories(Linux)

set(PadNullFinalSources 
	${PadNullSources}
	${PadNullHeaders}
	${PadNullLinuxSources}
	${PadNullLinuxHeaders}
)

set(PadNullFinalLibs 
	${GTK2_LIBRARIES}
	${X11_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${PadNullFinalSources}" "${PadNullFinalLibs}" "${PadNullFinalFlags}")
