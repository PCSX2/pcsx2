add_executable(updater
	Updater.cpp
	Updater.h
)

target_link_libraries(updater PRIVATE common fmt::fmt)

if(WIN32)
	target_link_libraries(updater PRIVATE
		LZMA::LZMA
	)
	target_sources(updater PRIVATE
		Win32Update.cpp
	)
endif()
