add_executable(pcsx2-eerunner)

if (PACKAGE_MODE)
	install(TARGETS pcsx2-eerunner DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
	install(TARGETS pcsx2-eerunner DESTINATION ${CMAKE_SOURCE_DIR}/bin)
endif()

target_sources(pcsx2-eerunner PRIVATE
	Main.cpp
)

target_include_directories(pcsx2-eerunner PRIVATE
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/pcsx2"
	"${CMAKE_SOURCE_DIR}/tests/ctest/core/recompilers"
)

target_link_libraries(pcsx2-eerunner PRIVATE
	PCSX2_FLAGS
	PCSX2
)
