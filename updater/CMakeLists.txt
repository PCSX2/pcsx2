add_executable(updater
	SZErrors.h
	Updater.cpp
	Updater.h
	UpdaterExtractor.h
)

target_link_libraries(updater PRIVATE common fmt::fmt)
target_include_directories(updater PRIVATE .)

if(WIN32)
	target_link_libraries(updater PRIVATE
		LZMA::LZMA
		Comctl32.lib
	)
	target_sources(updater PRIVATE
		Windows/WindowsUpdater.cpp
		Windows/resource.h
		Windows/updater.rc
		Windows/updater.manifest
	)
	set_target_properties(updater PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
