add_library(imgui
	imgui/imconfig.h
	imgui/imgui.cpp
	imgui/imgui.h
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_internal.h
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/imstb_rectpack.h
	imgui/imstb_textedit.h
	imgui/imstb_truetype.h
)

target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/imgui" "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(USE_OPENGL)
	target_sources(imgui PRIVATE
		include/imgui_impl_opengl3.h
		src/imgui_impl_opengl3.cpp
	)
	target_link_libraries(imgui glad)
endif()

if(WIN32)
	target_sources(imgui PRIVATE
		include/imgui_impl_dx11.h
		include/imgui_impl_dx12.h
		src/imgui_impl_dx11.cpp
		src/imgui_impl_dx12.cpp
	)
endif()
