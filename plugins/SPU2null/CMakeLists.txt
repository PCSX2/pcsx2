# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output SPU2null)

set(OptimizationFlags
    -O2
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(SPU2nullFinalFlags "")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(SPU2nullFinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(SPU2nullFinalFlags ${OptimizationFlags})

endif()

# SPU2null sources
set(SPU2nullSources
	SPU2.cpp)

# SPU2null headers
set(SPU2nullHeaders
	SPU2.h)

# SPU2null Linux sources
set(SPU2nullLinuxSources
	Linux/Config.cpp)

# SPU2null Linux headers
set(SPU2nullLinuxHeaders
	Linux/Config.h)

# SPU2null Windows sources
set(SPU2nullWindowsSources
	Windows/Config.cpp
	Windows/SPU2null.def
	Windows/SPU2null.rc
	Windows/Win32.cpp)

# SPU2null Windows headers
set(SPU2nullWindowsHeaders
	Windows/resource.h)

set(SPU2nullFinalSources
	${SPU2nullSources}
	${SPU2nullHeaders}
	${SPU2nullLinuxSources}
	${SPU2nullLinuxHeaders}
)

set(SPU2nullFinalLibs
	${GTK2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${SPU2nullFinalSources}" "${SPU2nullFinalLibs}" "${SPU2nullFinalFlags}")
