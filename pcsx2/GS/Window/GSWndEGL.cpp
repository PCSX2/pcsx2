/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "GSWndEGL.h"

#if defined(__unix__)

// static method
int GSWndEGL::SelectPlatform()
{
	// Check the supported extension
	const char* client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (!client_extensions)
	{
		fprintf(stderr, "EGL: Client extension not supported\n");
		return 0;
	}
	fprintf(stdout, "EGL: Supported extensions: %s\n", client_extensions);

	// Check platform extensions are supported (Note: there are core in 1.5)
	if (!strstr(client_extensions, "EGL_EXT_platform_base"))
	{
		fprintf(stderr, "EGL: Dynamic platform selection isn't supported\n");
		return 0;
	}

	// Finally we can select the platform
#if GS_EGL_X11
	if (strstr(client_extensions, "EGL_EXT_platform_x11"))
	{
		fprintf(stdout, "EGL: select X11 platform\n");
		return EGL_PLATFORM_X11_KHR;
	}
#endif
#if GS_EGL_WL
	if (strstr(client_extensions, "EGL_EXT_platform_wayland"))
	{
		fprintf(stdout, "EGL: select Wayland platform\n");
		return EGL_PLATFORM_WAYLAND_KHR;
	}
#endif

	fprintf(stderr, "EGL: no compatible platform found\n");

	return 0;
}


GSWndEGL::GSWndEGL(int platform)
	: m_native_window(nullptr), m_platform(platform)
{
}

void GSWndEGL::CreateContext(int major, int minor)
{
	EGLConfig eglConfig;
	EGLint numConfigs = 0;
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_MAJOR_VERSION_KHR, major,
		EGL_CONTEXT_MINOR_VERSION_KHR, minor,
#ifdef ENABLE_OGL_DEBUG
		EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
#else
		// Open Source isn't happy with an unsupported flags...
		//EGL_CONTEXT_FLAGS_KHR, GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR,
#endif
		EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
		EGL_NONE
	};
	EGLint NullContextAttribs[] = {EGL_NONE};
	EGLint attrList[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	BindAPI();

	eglChooseConfig(m_eglDisplay, attrList, &eglConfig, 1, &numConfigs);
	if (numConfigs == 0)
	{
		fprintf(stderr, "EGL: Failed to get a frame buffer config! (0x%x)\n", eglGetError());
		throw GSRecoverableError();
	}

	m_eglSurface = eglCreatePlatformWindowSurface(m_eglDisplay, eglConfig, m_native_window, nullptr);
	if (m_eglSurface == EGL_NO_SURFACE)
	{
		fprintf(stderr, "EGL: Failed to get a window surface\n");
		throw GSRecoverableError();
	}

	m_eglContext = eglCreateContext(m_eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
	EGLint status = eglGetError();
	if (status == EGL_BAD_ATTRIBUTE || status == EGL_BAD_MATCH)
	{
		// Radeon/Gallium don't support advance attribute. Fallback to random value
		// Note: Intel gives an EGL_BAD_MATCH. I don't know why but let's by stubborn and retry.
		fprintf(stderr, "EGL: warning your driver doesn't support advance openGL context attributes\n");
		m_eglContext = eglCreateContext(m_eglDisplay, eglConfig, EGL_NO_CONTEXT, NullContextAttribs);
		status = eglGetError();
	}
	if (m_eglContext == EGL_NO_CONTEXT)
	{
		fprintf(stderr, "EGL: Failed to create the context\n");
		fprintf(stderr, "EGL STATUS: %x\n", status);
		throw GSRecoverableError();
	}

	if (!eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext))
	{
		throw GSRecoverableError();
	}
}

void GSWndEGL::AttachContext()
{
	if (!IsContextAttached())
	{
		// The setting of the API is local to a thread. This function
		// can be called from 2 threads.
		BindAPI();

		//fprintf(stderr, "Attach the context\n");
		eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
		m_ctx_attached = true;
	}
}

void GSWndEGL::DetachContext()
{
	if (IsContextAttached())
	{
		//fprintf(stderr, "Detach the context\n");
		eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		m_ctx_attached = false;
	}
}

void GSWndEGL::PopulateWndGlFunction()
{
}

void GSWndEGL::BindAPI()
{
	eglBindAPI(EGL_OPENGL_API);
	EGLenum api = eglQueryAPI();
	if (api != EGL_OPENGL_API)
	{
		fprintf(stderr, "EGL: Failed to bind the OpenGL API got 0x%x instead\n", api);
		throw GSRecoverableError();
	}
}

bool GSWndEGL::Attach(void* handle, bool managed)
{
	m_managed = managed;

	m_native_window = AttachNativeWindow(handle);

	OpenEGLDisplay();

	FullContextInit();

	return true;
}

void GSWndEGL::Detach()
{
	// Actually the destructor is not called when there is only a GSclose/GSshutdown
	// The window still need to be closed
	DetachContext();
	eglDestroyContext(m_eglDisplay, m_eglContext);
	m_eglContext = nullptr;

	eglDestroySurface(m_eglDisplay, m_eglSurface);
	m_eglSurface = nullptr;

	CloseEGLDisplay();

	DestroyNativeResources();
}

bool GSWndEGL::Create(const std::string& title, int w, int h)
{
	if (w <= 0 || h <= 0)
	{
		w = theApp.GetConfigI("ModeWidth");
		h = theApp.GetConfigI("ModeHeight");
	}

	m_managed = true;

	OpenEGLDisplay();

	m_native_window = CreateNativeWindow(w, h);

	FullContextInit();

	return true;
}

void* GSWndEGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)eglGetProcAddress(name);
	if (ptr == nullptr)
	{
		if (theApp.GetConfigB("debug_opengl"))
			fprintf(stderr, "Failed to find %s\n", name);

		if (!opt)
			throw GSRecoverableError();
	}
	return ptr;
}

GSVector4i GSWndEGL::GetClientRect()
{
	EGLint h = 0;
	EGLint w = 0;
	eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &h);
	eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &w);

	return GSVector4i(0, 0, w, h);
}

void GSWndEGL::SetSwapInterval()
{
	// 0 -> disable vsync
	// n -> wait n frame
	eglSwapInterval(m_eglDisplay, m_vsync);
}

void GSWndEGL::Flip()
{
	if (m_vsync_change_requested.exchange(false))
		SetSwapInterval();

	eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

void GSWndEGL::CloseEGLDisplay()
{
	eglReleaseThread();
	eglTerminate(m_eglDisplay);
}

void GSWndEGL::OpenEGLDisplay()
{
	// We only need a native display when we manage the window ourself.
	// By default, EGL will create its own native display. This way the driver knows
	// that display will be thread safe and so it can enable multithread optimization.
	void* native_display = (m_managed) ? CreateNativeDisplay() : nullptr;

	// Create an EGL display from the native display
	m_eglDisplay = eglGetPlatformDisplay(m_platform, native_display, nullptr);
	if (m_eglDisplay == EGL_NO_DISPLAY)
	{
		fprintf(stderr, "EGL: Failed to open a display! (0x%x)\n", eglGetError());
		throw GSRecoverableError();
	}

	if (!eglInitialize(m_eglDisplay, nullptr, nullptr))
	{
		fprintf(stderr, "EGL: Failed to initialize the display! (0x%x)\n", eglGetError());
		throw GSRecoverableError();
	}
}

//////////////////////////////////////////////////////////////////////
// X11 platform
//////////////////////////////////////////////////////////////////////
#if GS_EGL_X11

GSWndEGL_X11::GSWndEGL_X11()
	: GSWndEGL(EGL_PLATFORM_X11_KHR), m_NativeDisplay(nullptr), m_NativeWindow(0)
{
}

void* GSWndEGL_X11::CreateNativeDisplay()
{
	if (m_NativeDisplay == nullptr)
		m_NativeDisplay = XOpenDisplay(nullptr);

	return (void*)m_NativeDisplay;
}

void* GSWndEGL_X11::CreateNativeWindow(int w, int h)
{
	const int depth = 0, x = 0, y = 0, border_width = 1;
#if 0
	// Old X code in case future code will still require it
	m_NativeWindow = XCreateSimpleWindow(m_NativeDisplay, DefaultRootWindow(m_NativeDisplay), 0, 0, w, h, 0, 0, 0);
	XMapWindow (m_NativeDisplay, m_NativeWindow);
#endif

	if (m_NativeDisplay == nullptr)
	{
		fprintf(stderr, "EGL X11: display wasn't created before the window\n");
		throw GSRecoverableError();
	}

	xcb_connection_t* c = XGetXCBConnection(m_NativeDisplay);

	const xcb_setup_t* setup = xcb_get_setup(c);

	xcb_screen_t* screen = (xcb_setup_roots_iterator(setup)).data;

	m_NativeWindow = xcb_generate_id(c);

	if (m_NativeWindow == 0)
	{
		fprintf(stderr, "EGL X11: failed to create the native window\n");
		throw GSRecoverableError();
	}

	xcb_create_window(c, depth, m_NativeWindow, screen->root, x, y, w, h,
		border_width, InputOutput, screen->root_visual, 0, nullptr);

	xcb_map_window(c, m_NativeWindow);

	xcb_flush(c);

	return (void*)&m_NativeWindow;
}

void* GSWndEGL_X11::AttachNativeWindow(void* handle)
{
	m_NativeWindow = *(Window*)handle;
	return handle;
}

void GSWndEGL_X11::DestroyNativeResources()
{
	if (m_NativeDisplay)
	{
		XCloseDisplay(m_NativeDisplay);
		m_NativeDisplay = nullptr;
	}
}

bool GSWndEGL_X11::SetWindowText(const char* title)
{
	if (!m_managed)
		return true;

	xcb_connection_t* c = XGetXCBConnection(m_NativeDisplay);

	xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_NativeWindow,
		XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
		strlen(title), title);

	return true;
}

#endif

//////////////////////////////////////////////////////////////////////
// Wayland platform (just a place holder)
//////////////////////////////////////////////////////////////////////
#if GS_EGL_WL

GSWndEGL_WL::GSWndEGL_WL()
	: GSWndEGL(EGL_PLATFORM_WAYLAND_KHR), m_NativeDisplay(nullptr), m_NativeWindow(nullptr)
{
}

void* GSWndEGL_WL::CreateNativeDisplay()
{
	if (m_NativeDisplay == nullptr)
		m_NativeDisplay = wl_display_connect(NULL);

	return (void*)m_NativeDisplay;
}

void* GSWndEGL_WL::CreateNativeWindow(int w, int h)
{
	return nullptr;
}

void* GSWndEGL_WL::AttachNativeWindow(void* handle)
{
	m_NativeWindow = (wl_egl_window*)handle;
	return handle;
}

void GSWndEGL_WL::DestroyNativeResources()
{
	if (m_NativeDisplay)
	{
		wl_display_disconnect(m_NativeDisplay);
		m_NativeDisplay = nullptr;
	}
}

bool GSWndEGL_WL::SetWindowText(const char* title)
{
	return true;
}

#endif

#endif
