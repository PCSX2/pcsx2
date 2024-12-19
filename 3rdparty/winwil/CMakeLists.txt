add_library(pcsx2-wil INTERFACE)
target_include_directories(pcsx2-wil INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(WIL::WIL ALIAS pcsx2-wil)
