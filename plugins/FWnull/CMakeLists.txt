# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output FWnull-0.7.0)

set(FWnullFinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
	)

# FWnull sources
set(FWnullSources
	FW.cpp)

# FWnull headers
set(FWnullHeaders
	FW.h)

# FWnull Linux sources
set(FWnullLinuxSources
)

# FWnull Linux headers
set(FWnullLinuxHeaders
)

# FWnull Windows sources
set(FWnullWindowsSources
	Windows/FireWireNull.def
)

# FWnull Windows headers
set(FWnullWindowsHeaders
)

set(FWnullFinalSources
	${FWnullSources}
	${FWnullHeaders}
	${FWnullLinuxSources}
	${FWnullLinuxHeaders}
)

set(FWnullFinalLibs
	${wxWidgets_LIBRARIES}
)

if(BUILTIN_FW)
    add_pcsx2_lib(${Output} "${FWnullFinalSources}" "${FWnullFinalLibs}" "${FWnullFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${FWnullFinalSources}" "${FWnullFinalLibs}" "${FWnullFinalFlags}")
endif()
