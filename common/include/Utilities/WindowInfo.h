#pragma once
#include "Pcsx2Defs.h"

// Contains the information required to create a graphics context in a window.
struct WindowInfo
{
  enum class Type
  {
    Surfaceless,
    Win32,
    X11,
    Wayland,
    MacOS
  };

  Type type = Type::Surfaceless;
  void* display_connection = nullptr;
  void* window_handle = nullptr;
  u32 surface_width = 0;
  u32 surface_height = 0;
  float surface_scale = 1.0f;
};
