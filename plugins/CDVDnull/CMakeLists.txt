# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output CDVDnull)
set(CDVDnullFinalFlags "")

# CDVDnull sources
set(CDVDnullSources
	CDVD.cpp)

# CDVDnull headers
set(CDVDnullHeaders
	CDVD.h)

# CDVDnull Windows sources
set(CDVDnullWindowsSources
	Windows/CDVDnull.def
	Windows/plugin.def)

# CDVDnull Windows headers
set(CDVDnullWindowsHeaders
	)

set(CDVDnullFinalSources
	${CDVDnullSources}
	${CDVDnullHeaders}
)

set(CDVDnullFinalLibs
	${GTK2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${CDVDnullFinalSources}" "${CDVDnullFinalLibs}" "${CDVDnullFinalFlags}")
