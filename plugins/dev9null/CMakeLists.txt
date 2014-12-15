# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output dev9null-0.5.0)

set(OptimizationFlags
    -O2
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(dev9nullFinalFlags "")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(dev9nullFinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(dev9nullFinalFlags ${OptimizationFlags})

endif()

# dev9null sources
set(dev9nullSources
	Config.cpp
	DEV9.cpp)

# dev9null headers
set(dev9nullHeaders
	Config.h
	DEV9.h)

# dev9null Linux sources
set(dev9nullLinuxSources
)

# dev9null Linux headers
set(dev9nullLinuxHeaders
)

# dev9null Windows sources
set(dev9nullWindowsSources
	Windows/dev9null.def)

# dev9null Windows headers
set(dev9nullWindowsHeaders
	)

set(dev9nullFinalSources
	${dev9nullSources}
	${dev9nullHeaders}
	${dev9nullLinuxSources}
	${dev9nullLinuxHeaders}
)

set(dev9nullFinalLibs
	${GTK2_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${dev9nullFinalSources}" "${dev9nullFinalLibs}" "${dev9nullFinalFlags}")
