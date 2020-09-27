# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output dev9null-0.5.0)

set(dev9nullFinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
	)

# dev9null sources
set(dev9nullSources
	DEV9.cpp)

# dev9null headers
set(dev9nullHeaders
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
	${wxWidgets_LIBRARIES}
)

if(BUILTIN_DEV9)
    add_pcsx2_lib(${Output} "${dev9nullFinalSources}" "${dev9nullFinalLibs}" "${dev9nullFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${dev9nullFinalSources}" "${dev9nullFinalLibs}" "${dev9nullFinalFlags}")
endif()

target_compile_features(${Output} PRIVATE cxx_std_17)