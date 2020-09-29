# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output USBnull-0.7.0)

set(USBnullFinalFlags
-fvisibility=hidden
-Wall
-Wno-parentheses
	)

# USBnull sources
set(USBnullSources
	USB.cpp)

# USBnull headers
set(USBnullHeaders
	USB.h)

# USBnull Linux sources
set(USBnullLinuxSources
)

# USBnull Linux headers
set(USBnullLinuxHeaders
)

# USBnull Windows sources
set(USBnullWindowsSources
	Windows/USBnull.def
)

# USBnull Windows headers
set(USBnullWindowsHeaders
)

set(USBnullFinalSources
	${USBnullSources}
	${USBnullHeaders}
	${USBnullLinuxSources}
	${USBnullLinuxHeaders}
)

set(USBnullFinalLibs
	${GTK2_LIBRARIES}
	${wxWidgets_LIBRARIES}
)

if(BUILTIN_USB)
    add_pcsx2_lib(${Output} "${USBnullFinalSources}" "${USBnullFinalLibs}" "${USBnullFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${USBnullFinalSources}" "${USBnullFinalLibs}" "${USBnullFinalFlags}")
endif()

target_compile_features(${Output} PRIVATE cxx_std_17)