add_library(WinPixEventRuntime::WinPixEventRuntime UNKNOWN IMPORTED GLOBAL)

if(ARCH_ARM64)
  set_target_properties(WinPixEventRuntime::WinPixEventRuntime PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libarm64/WinPixEventRuntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
else()
  set_target_properties(WinPixEventRuntime::WinPixEventRuntime PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/WinPixEventRuntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
endif()
