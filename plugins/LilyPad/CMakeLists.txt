# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output LilyPad-0.11.0)

set(OptimizationFlags
    -O2
    )

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(lilypadFinalFlags "-DPCSX2_DEBUG")

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
	set(lilypadFinalFlags ${OptimizationFlags})

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
	set(lilypadFinalFlags ${OptimizationFlags})

endif()

# lilypad sources
set(lilypadSources
	DeviceEnumerator.cpp
	InputManager.cpp
	Linux/Config.cpp
	Linux/ConfigHelper.cpp
    Linux/KeyboardMouse.cpp
	Linux/KeyboardQueue.cpp
    Linux/LilyPad.cpp)

# lilypad headers
set(lilypadHeaders
	)

# lilypad Linux sources
set(lilypadLinuxSources
	)

# lilypad Linux headers
set(lilypadLinuxHeaders
	)

if (SDL2_API)
	set(lilypadFinalLibs
		${SDL2_LIBRARIES}
	)
else()
	set(lilypadFinalLibs
		${SDL_LIBRARY}
	)
endif()

set(lilypadFinalLibs
	#${lilypadFinalLibs}
	#${GTK2_LIBRARIES}
	${X11_LIBRARIES}
)

set(lilypadFinalSources
	${lilypadSources}
	${lilypadHeaders}
	${lilypadLinuxSources}
	${lilypadLinuxHeaders}
)

add_pcsx2_plugin(${Output} "${lilypadFinalSources}" "${lilypadFinalLibs}" "${lilypadFinalFlags}")
