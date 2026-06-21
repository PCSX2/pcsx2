add_executable(pcsx2-gsrunner)

if (PACKAGE_MODE)
	install(TARGETS pcsx2-gsrunner DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
	install(TARGETS pcsx2-gsrunner DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()

target_sources(pcsx2-gsrunner PRIVATE
	Main.cpp
)

target_include_directories(pcsx2-gsrunner PRIVATE
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/pcsx2"
)

target_link_libraries(pcsx2-gsrunner PRIVATE
	PCSX2_FLAGS
	PCSX2
)

if(WAYLAND_API AND UNIX AND NOT APPLE)
	find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

	# pkg_get_variable() needs CMake 3.18; the project minimum is 3.16
	# (Ubuntu 20.04 LTS ships 3.16.3), so query pkg-config directly.
	execute_process(
		COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
		OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT WAYLAND_PROTOCOLS_DIR)
		message(FATAL_ERROR
			"wayland-protocols not found via pkg-config. "
			"Install wayland-protocols-devel (Fedora) / libwayland-dev (Debian).")
	endif()

	set(_xdg_xml "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
	set(_xdg_h "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
	set(_xdg_c "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")

	add_custom_command(OUTPUT ${_xdg_h}
		COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${_xdg_xml} ${_xdg_h}
		DEPENDS ${_xdg_xml} VERBATIM)
	add_custom_command(OUTPUT ${_xdg_c}
		COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${_xdg_xml} ${_xdg_c}
		DEPENDS ${_xdg_xml} VERBATIM)

	target_sources(pcsx2-gsrunner PRIVATE ${_xdg_h} ${_xdg_c})
	target_include_directories(pcsx2-gsrunner PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
	# Generated C file; skip the C++ PCH which #includes <cstdint>.
	set_source_files_properties(${_xdg_c} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

	pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
	target_link_libraries(pcsx2-gsrunner PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
	target_include_directories(pcsx2-gsrunner PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
endif()
