add_library(glad
	include/glad/gl.h
	include/KHR/khrplatform.h
	src/gl.c
)

target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(glad PRIVATE Threads::Threads "${CMAKE_DL_LIBS}")

if(WIN32)
	target_sources(glad PRIVATE
		include/glad/wgl.h
		src/wgl.c
	)
else()
	target_sources(glad PRIVATE
		include/EGL/eglplatform.h
		include/glad/egl.h
		src/egl.c
	)
endif()
