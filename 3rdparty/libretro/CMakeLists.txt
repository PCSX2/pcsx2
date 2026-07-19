# libretro public API headers (libretro.h + the Vulkan HW-render interface).
# Header-only: consumed by the pcsx2-libretro frontend and by VKLibretro.cpp
# in the GS backend, which implements the Vulkan context-negotiation wraps.
add_library(libretro-headers INTERFACE)
target_include_directories(libretro-headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
