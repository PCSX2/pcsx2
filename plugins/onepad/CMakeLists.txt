# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# onepad sources
set(onepadSources
	controller.cpp
	joystick.cpp
	keyboard.cpp
    KeyStatus.cpp
	onepad.cpp)

# onepad Linux sources
set(onepadLinuxSources
	Linux/ini.cpp
	Linux/dialog.cpp
	Linux/linux.cpp)

set(onepadFinalSources
${onepadSources} 
${onepadLinuxSources}
	)
set(onepadFinalLibs
${SDL_LIBRARY} 
${GTK2_LIBRARIES}
	)

add_pcsx2_plugin(onepad "${onepadFinalSources}" "${onepadFinalLibs}")

