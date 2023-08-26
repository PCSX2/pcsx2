add_library(jpgd
	jpgd.cpp
	jpgd.h
	jpgd_idct.h
	jpge.cpp
	jpge.h
)

target_include_directories(jpgd PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(TARGET jpgd PROPERTY CXX_STANDARD 17)
set_property(TARGET jpgd PROPERTY CXX_STANDARD_REQUIRED ON)
disable_compiler_warnings_for_target(jpgd)

