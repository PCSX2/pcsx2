#pragma once

#if defined(__unix__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
// undef Xlib macros that conflict with our symbols,
// but only after including X11 headers that use them.
#undef Status
#undef DisableScreenSaver
#elif defined(_WIN32)
#include <windows.h>
#endif

// Represents a platform-native window handle shared between GUI and components.
struct NativeWindowHandle {
	enum {
#if defined(__unix__)
		X11
#elif defined(_WIN32)
		Win32
#endif
	} kind;

	union
	{
#if defined(__unix__)
		struct {
			Display* display;
			Window window;
		} x11;
#elif defined(_WIN32)
		HWND win32;
#endif
	};
};
