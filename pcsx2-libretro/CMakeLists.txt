add_library(pcsx2-libretro SHARED)

# Libretro cores are named <core>_libretro.so with no "lib" prefix.
set_target_properties(pcsx2-libretro PROPERTIES
	OUTPUT_NAME "armsx2_libretro"
	PREFIX ""
)

if (PACKAGE_MODE)
	install(TARGETS pcsx2-libretro DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
	install(TARGETS pcsx2-libretro DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()

target_sources(pcsx2-libretro PRIVATE
	Main.cpp
)

target_include_directories(pcsx2-libretro PRIVATE
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/pcsx2"
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(pcsx2-libretro PRIVATE
	PCSX2_FLAGS
	PCSX2
	libretro-headers
)

# A shared core must be fully position-independent — the pcsx2-sdl ET_EXEC
# fixed-address trick (persisted-JIT VU cache) does not apply here.
set_target_properties(pcsx2-libretro PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Only export the retro_* entry points; PCSX2's internal symbols must not
# leak into (or collide with) the frontend process.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_options(pcsx2-libretro PRIVATE
		"-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/link.T")
	set_target_properties(pcsx2-libretro PROPERTIES
		LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/link.T")
endif()
