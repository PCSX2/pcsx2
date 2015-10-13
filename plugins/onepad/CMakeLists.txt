# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output onepad-1.1.0)
set(onepadFinalFlags "")
# onepad sources
set(onepadSources
	controller.cpp
	SDL/joystick.cpp
    GamePad.cpp
	keyboard.cpp
    KeyStatus.cpp
	onepad.cpp)

# onepad headers
set(onepadHeaders
	bitwise.h
	controller.h
	SDL/joystick.h
    GamePad.h
	keyboard.h
    KeyStatus.h
	onepad.h)

# onepad Linux sources
set(onepadLinuxSources
	Linux/ini.cpp
	Linux/dialog.cpp
	Linux/linux.cpp)

# onepad Linux headers
set(onepadLinuxHeaders
	Linux/linux.h)

if (SDL2_API)
	set(onepadFinalLibs
		${SDL2_LIBRARIES}
        )
    add_definitions(-DSDL_BUILD -DSDL2_BUILD)
else()
	set(onepadFinalLibs
		${SDL_LIBRARY}
        )
    add_definitions(-DSDL_BUILD -DSDL2_BUILD)
endif()

set(onepadFinalLibs
	${onepadFinalLibs}
	${GTK2_LIBRARIES}
	${X11_LIBRARIES}
)

set(onepadFinalSources
	${onepadSources}
	${onepadHeaders}
	${onepadLinuxSources}
	${onepadLinuxHeaders}
)

add_pcsx2_plugin(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
