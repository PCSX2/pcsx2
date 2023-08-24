add_library(WinPixEventRuntime::WinPixEventRuntime UNKNOWN IMPORTED GLOBAL)
set_target_properties(WinPixEventRuntime::WinPixEventRuntime PROPERTIES
	IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/WinPixEventRuntime.lib"
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
