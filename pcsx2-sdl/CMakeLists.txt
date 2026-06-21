add_executable(pcsx2-sdl)

if (PACKAGE_MODE)
	install(TARGETS pcsx2-sdl DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
	install(TARGETS pcsx2-sdl DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()

target_sources(pcsx2-sdl PRIVATE
	Main.cpp
)

target_include_directories(pcsx2-sdl PRIVATE
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/pcsx2"
)

target_link_libraries(pcsx2-sdl PRIVATE
	PCSX2_FLAGS
	PCSX2
	SDL3::SDL3
)

# Deterministic process layout for the persisted-JIT VU program cache: a
# non-PIE executable (ET_EXEC) loads at its fixed link address even with
# ASLR enabled, so every libpcsx2 symbol address is run-invariant —
# paired with the image-anchored fixed-base arena reservation in
# SysMemory::AllocateVirtualMemory. This is what lets cached VU code
# reload across boots on the handheld without repatching baked
# addresses. Linux-only; libpcsx2's -fPIC objects link into an ET_EXEC
# fine.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set_target_properties(pcsx2-sdl PROPERTIES POSITION_INDEPENDENT_CODE OFF)
	target_link_options(pcsx2-sdl PRIVATE -no-pie)
endif()
