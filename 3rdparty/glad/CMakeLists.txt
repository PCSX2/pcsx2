add_library(glad
	src/glad.c
)

if(WIN32)
	target_sources(glad PRIVATE
		src/glad_wgl.c
	)
elseif(X11_API OR WAYLAND_API)
	target_sources(glad PRIVATE
		src/glad_egl.c
	)
endif()

target_include_directories(glad PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(glad INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(glad PRIVATE Threads::Threads "${CMAKE_DL_LIBS}")

if(APPLE)
	target_compile_options(glad PRIVATE -fno-common)
endif()

