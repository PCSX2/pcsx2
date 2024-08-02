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
