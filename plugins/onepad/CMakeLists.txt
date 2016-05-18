# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

set(compiled_images "${CMAKE_BINARY_DIR}/plugins/onepad/ImgHeader")
set(image_sources "${CMAKE_SOURCE_DIR}/plugins/onepad/Img")
set(linux_sources "Linux")
set(linux_headers "Linux")


set(onepadGuiResources
    ${compiled_images}/analog.h
    ${compiled_images}/circle.h
    ${compiled_images}/cross.h
    ${compiled_images}/dp_bottom.h
    ${compiled_images}/dp_left.h
    ${compiled_images}/dp_right.h
    ${compiled_images}/dp_up.h
    ${compiled_images}/dualshock2.h
    ${compiled_images}/joystick_cursor.h
    ${compiled_images}/l1.h
    ${compiled_images}/r1.h
    ${compiled_images}/l2.h
    ${compiled_images}/r2.h
    ${compiled_images}/l3.h
    ${compiled_images}/r3.h
    ${compiled_images}/select.h
    ${compiled_images}/square.h
    ${compiled_images}/start.h
    ${compiled_images}/triangle.h
    ${compiled_images}/arrow_up.h
    ${compiled_images}/arrow_bottom.h
    ${compiled_images}/arrow_left.h
    ${compiled_images}/arrow_right.h
    )

# plugin name
set(Output onepad-1.2.0)
set(onepadFinalFlags "")

# onepad sources
set(onepadSources
	controller.cpp
	GamePad.cpp
	SDL/joystick.cpp
	keyboard.cpp
	KeyStatus.cpp
	onepad.cpp)

# onepad headers
set(onepadHeaders
	bitwise.h
	controller.h
	GamePad.h
	SDL/joystick.h
	keyboard.h
	KeyStatus.h
	onepad.h)

# onepad Linux sources
set(onepadLinuxSources
	${linux_sources}/ini.cpp
	${linux_sources}/dialog.cpp
	${linux_sources}/linux.cpp
	${linux_sources}/opPanel.cpp
	${linux_sources}/GamepadConfiguration.cpp
	${linux_headers}/JoystickConfiguration.cpp)

# onepad Linux headers
set(onepadLinuxHeaders
	${linux_headers}/linux.h
	${linux_headers}/opPanel.h
	${linux_headers}/GamepadConfiguration.h
	${linux_headers}/JoystickConfiguration.h)

# onepad Windows sources
set(onepadWindowsSources
	)

# onepad Windows headers
set(onepadWindowsHeaders
	)

if (SDL2_API)
	set(onepadFinalLibs
		${SDL2_LIBRARIES}
	)
	add_definitions(-DSDL_BUILD)
else()
	set(onepadFinalLibs
		${SDL_LIBRARY}
	)
	add_definitions(-DSDL_BUILD)
endif()

set(onepadFinalLibs
	${onepadFinalLibs}
	${wxWidgets_LIBRARIES}
	${GTK2_LIBRARIES}
	${X11_LIBRARIES}
)

set(onepadFinalSources
	${onepadSources}
	${onepadHeaders}
	${onepadLinuxSources}
	${onepadLinuxHeaders}
	${onepadGuiResources}
)

include_directories(
    ${CMAKE_BINARY_DIR}/plugins/onepad/
)

### Generate the resources files
file(MAKE_DIRECTORY ${compiled_images})

foreach(result_file IN ITEMS
        analog circle cross dp_bottom dp_left dp_right dp_up dualshock2
        joystick_cursor l1 r1 l2 r2 l3 r3 select square start triangle arrow_up
        arrow_bottom arrow_left arrow_right)
    add_custom_command(
        OUTPUT  "${compiled_images}/${result_file}.h"
        COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl "${image_sources}/${result_file}.png" "${compiled_images}/${result_file}" )
endforeach()

if(BUILTIN_PAD)
    add_pcsx2_lib(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
endif()
