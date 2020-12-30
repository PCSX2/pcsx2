/*
 *	Copyright (C) 2007-2012 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSWndEGL.h"

#if defined(__unix__)

// static method
int GSWndEGL::SelectPlatform(void *display_handle)
{
	// Check the supported extension
	const char *client_extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (!client_extensions) {
		fprintf(stderr, "EGL: Client extension not supported\n");
		return 0;
	}
	fprintf(stdout, "EGL: Supported extensions: %s\n", client_extensions);

	// Check platform extensions are supported (Note: there are core in 1.5)
	if (!strstr(client_extensions, "EGL_EXT_platform_base")) {
		fprintf(stderr, "EGL: Dynamic platform selection isn't supported\n");
		return 0;
	}

	// Inspect the provided display handle to check if it is Wayland or X11.
	// The Wayland GS display handle happens to set pDsp[1] to NULL, so check it.
	bool is_wayland_display = display_handle == nullptr
		|| ((void **)display_handle)[1] == nullptr;

	// Finally we can select the platform
#if GS_EGL_WL
	if (strstr(client_extensions, "EGL_EXT_platform_wayland") && is_wayland_display) {
		fprintf(stdout, "EGL: select Wayland platform\n");
		return EGL_PLATFORM_WAYLAND_KHR;
	}
#endif
#if GS_EGL_X11
	if (strstr(client_extensions, "EGL_EXT_platform_x11")) {
		fprintf(stdout, "EGL: select X11 platform\n");
		return EGL_PLATFORM_X11_KHR;
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
	EGLint NullContextAttribs[] = { EGL_NONE };
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
	if ( numConfigs == 0 )
	{
		fprintf(stderr,"EGL: Failed to get a frame buffer config! (0x%x)\n", eglGetError() );
		throw GSDXRecoverableError();
	}

	m_eglSurface = eglCreatePlatformWindowSurface(m_eglDisplay, eglConfig, m_native_window, nullptr);
	if ( m_eglSurface == EGL_NO_SURFACE )
	{
		fprintf(stderr,"EGL: Failed to get a window surface\n");
		throw GSDXRecoverableError();
	}

	m_eglContext = eglCreateContext(m_eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
	EGLint status = eglGetError();
	if (status == EGL_BAD_ATTRIBUTE || status == EGL_BAD_MATCH) {
		// Radeon/Gallium don't support advance attribute. Fallback to random value
		// Note: Intel gives an EGL_BAD_MATCH. I don't know why but let's by stubborn and retry.
		fprintf(stderr, "EGL: warning your driver doesn't support advance openGL context attributes\n");
		m_eglContext = eglCreateContext(m_eglDisplay, eglConfig, EGL_NO_CONTEXT, NullContextAttribs);
		status = eglGetError();
	}
	if ( m_eglContext == EGL_NO_CONTEXT )
	{
		fprintf(stderr,"EGL: Failed to create the context\n");
		fprintf(stderr,"EGL STATUS: %x\n", status);
		throw GSDXRecoverableError();
	}

	if ( !eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext) )
	{
		throw GSDXRecoverableError();
	}
}

void GSWndEGL::AttachContext()
{
	if (!IsContextAttached()) {
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
	if (IsContextAttached()) {
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
	if (api != EGL_OPENGL_API) {
		fprintf(stderr,"EGL: Failed to bind the OpenGL API got 0x%x instead\n", api);
		throw GSDXRecoverableError();
	}
}

bool GSWndEGL::Attach(void* handle, bool managed)
{
	m_managed = managed;

	AttachNativeWindow(handle, &m_native_display, &m_native_window);
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
	if(w <= 0 || h <= 0) {
		w = theApp.GetConfigI("ModeWidth");
		h = theApp.GetConfigI("ModeHeight");
	}

	m_managed = true;

	m_native_display = CreateNativeDisplay();
	OpenEGLDisplay();

	m_native_window = CreateNativeWindow(w, h);
	FullContextInit();

	return true;
}

void* GSWndEGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)eglGetProcAddress(name);
	if (ptr == nullptr) {
		if (theApp.GetConfigB("debug_opengl"))
			fprintf(stderr, "Failed to find %s\n", name);

		if (!opt)
			throw GSDXRecoverableError();
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
	// Create an EGL display from the native display
	m_eglDisplay = eglGetPlatformDisplay(m_platform, m_native_display, nullptr);
	if (m_eglDisplay == EGL_NO_DISPLAY) {
		fprintf(stderr,"EGL: Failed to open a display! (0x%x)\n", eglGetError() );
		throw GSDXRecoverableError();
	}

	if (!eglInitialize(m_eglDisplay, nullptr, nullptr)) {
		fprintf(stderr,"EGL: Failed to initialize the display! (0x%x)\n", eglGetError() );
		throw GSDXRecoverableError();
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

void *GSWndEGL_X11::CreateNativeDisplay()
{
	if (m_NativeDisplay == nullptr)
		m_NativeDisplay = XOpenDisplay(nullptr);

	return (void*)m_NativeDisplay;
}

void *GSWndEGL_X11::CreateNativeWindow(int w, int h)
{
	const int depth = 0, x = 0, y = 0, border_width = 1;
#if 0
	// Old X code in case future code will still require it
	m_NativeWindow = XCreateSimpleWindow(m_NativeDisplay, DefaultRootWindow(m_NativeDisplay), 0, 0, w, h, 0, 0, 0);
	XMapWindow (m_NativeDisplay, m_NativeWindow);
#endif

	if (m_NativeDisplay == nullptr) {
		fprintf(stderr, "EGL X11: display wasn't created before the window\n");
		throw GSDXRecoverableError();
	}

	xcb_connection_t *c = XGetXCBConnection(m_NativeDisplay);

	const xcb_setup_t *setup = xcb_get_setup(c);

	xcb_screen_t *screen = (xcb_setup_roots_iterator (setup)).data;

	m_NativeWindow = xcb_generate_id(c);

	if (m_NativeWindow == 0) {
		fprintf(stderr, "EGL X11: failed to create the native window\n");
		throw GSDXRecoverableError();
	}

	xcb_create_window (c, depth, m_NativeWindow, screen->root, x, y, w, h,
			border_width, InputOutput, screen->root_visual, 0, nullptr);

	xcb_map_window (c, m_NativeWindow);

	xcb_flush(c);

	return (void*)&m_NativeWindow;
}

void GSWndEGL_X11::AttachNativeWindow(void *handle, void **out_native_display, void **out_native_window)
{
	uptr *window_ptr = (uptr*)handle + 1;
	m_NativeWindow = (Window)*window_ptr;

	// We only need a native display when we manage the window ourself.
	// By default, EGL will create its own native display. This way the driver knows
	// that display will be thread safe and so it can enable multithread optimization.
	m_NativeDisplay = nullptr;

	*out_native_display = m_NativeDisplay;
	*out_native_window = window_ptr;
}

void GSWndEGL_X11::DestroyNativeResources()
{
	if (m_NativeDisplay) {
		XCloseDisplay(m_NativeDisplay);
		m_NativeDisplay = nullptr;
	}
}

bool GSWndEGL_X11::SetWindowText(const char* title)
{
	if (!m_managed) return true;

	xcb_connection_t *c = XGetXCBConnection(m_NativeDisplay);

	xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_NativeWindow,
			XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
			strlen (title), title);

	return true;
}

#endif

//////////////////////////////////////////////////////////////////////
// Wayland platform
//////////////////////////////////////////////////////////////////////
#if GS_EGL_WL

// wl_registry listener
static void gs_wl_registry_add_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	((GSWndEGL_WL *)data)->RegistryAddGlobal(registry, name, interface, version);
}

static void gs_wl_registry_remove_global(void *data, wl_registry *registry, uint32_t name)
{
	((GSWndEGL_WL *)data)->RegistryRemoveGlobal(registry, name);
}

static const wl_registry_listener gs_wl_registry_listener = {
	gs_wl_registry_add_global,
	gs_wl_registry_remove_global,
};

// xdg_wm_base listener
static void gs_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const xdg_wm_base_listener gs_xdg_wm_base_listener = {
	gs_xdg_wm_base_ping,
};

// xdg_surface listener
static void gs_xdg_surface_configure(void *data, xdg_surface *xdg_surface, uint32_t serial)
{
	((GSWndEGL_WL *)data)->XDGSurfaceConfigure(xdg_surface, serial);
}

static const xdg_surface_listener gs_wl_xdg_surface_listener = {
	gs_xdg_surface_configure,
};

// xdg_toplevel listener

static void gs_xdg_toplevel_configure(void *data, xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states)
{
	((GSWndEGL_WL *)data)->XDGToplevelConfigure(xdg_toplevel, width, height, states);
}

static void gs_xdg_toplevel_close(void *data, xdg_toplevel *xdg_toplevel)
{
	((GSWndEGL_WL *)data)->XDGToplevelClose(xdg_toplevel);
}

static const xdg_toplevel_listener gs_wl_xdg_toplevel_listener = {
	gs_xdg_toplevel_configure,
	gs_xdg_toplevel_close,
};

GSWndEGL_WL::GSWndEGL_WL()
	: GSWndEGL(EGL_PLATFORM_WAYLAND_KHR), m_NativeDisplay(nullptr), m_NativeWindow(nullptr),
	  m_wl_registry(nullptr), m_wl_compositor(nullptr), m_wl_subcompositor(nullptr), m_xdg_wm_base(nullptr),
	  m_wl_surface(nullptr), m_wl_subsurface(nullptr), m_xdg_surface(nullptr), m_xdg_toplevel(nullptr)
{
}

void GSWndEGL_WL::RegistryAddGlobal(wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		m_wl_compositor = (wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	}
	if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		m_wl_subcompositor = (wl_subcompositor *)wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
	}
	if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		m_xdg_wm_base = (xdg_wm_base *)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(m_xdg_wm_base, &gs_xdg_wm_base_listener, this);
	}
}

void GSWndEGL_WL::RegistryRemoveGlobal(wl_registry *registry, uint32_t name)
{
}

void GSWndEGL_WL::XDGSurfaceConfigure(xdg_surface *xdg_surface, uint32_t serial)
{
	xdg_surface_ack_configure(xdg_surface, serial);
}

void GSWndEGL_WL::XDGToplevelConfigure(xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, wl_array *states)
{
	// Configure and Close events will only be sent to us when we manage our own window.
	// We don't have our own event loop anyway, so ignore configure and close events for now.
}

void GSWndEGL_WL::XDGToplevelClose(xdg_toplevel *xdg_toplevel)
{
}

void *GSWndEGL_WL::CreateNativeDisplay()
{
	if (m_NativeDisplay == nullptr)
		m_NativeDisplay = wl_display_connect(NULL);

	if (m_wl_registry == nullptr) {
		m_wl_registry = wl_display_get_registry(m_NativeDisplay);
		wl_registry_add_listener(m_wl_registry, &gs_wl_registry_listener, this);
	}

	wl_display_roundtrip(m_NativeDisplay);

	return (void*)m_NativeDisplay;
}

void *GSWndEGL_WL::CreateNativeWindow(int w, int h)
{
	if (m_NativeDisplay == nullptr) {
		fprintf(stderr, "EGL Wayland: display wasn't created before the window\n");
		throw GSDXRecoverableError();
	}

	if (m_wl_compositor == nullptr) {
		fprintf(stderr, "EGL Wayland: compositor global required but not present\n");
		throw GSDXRecoverableError();
	}

	m_wl_surface = wl_compositor_create_surface(m_wl_compositor);
	if (m_wl_surface == nullptr) {
		fprintf(stderr, "EGL Wayland: wl_compositor_create_surface failed\n");
		throw GSDXRecoverableError();
	}

	m_NativeWindow = wl_egl_window_create(m_wl_surface, w, h);
	if (m_NativeWindow == nullptr) {
		fprintf(stderr, "EGL Wayland: wl_egl_window_create failed\n");
		throw GSDXRecoverableError();
	}

	if (m_xdg_wm_base == nullptr) {
		fprintf(stderr, "EGL Wayland: xdg_wm_base global required but not present\n");
		throw GSDXRecoverableError();
	}

	m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_wl_surface);
	if (m_xdg_surface == nullptr) {
		fprintf(stderr, "EGL Wayland: xdg_wm_base_get_xdg_surface failed\n");
		throw GSDXRecoverableError();
	}

	xdg_surface_add_listener(m_xdg_surface, &gs_wl_xdg_surface_listener, this);

	m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
	if (m_xdg_toplevel == nullptr) {
		fprintf(stderr, "EGL Wayland: xdg_surface_get_toplevel failed\n");
		throw GSDXRecoverableError();
	}

	xdg_toplevel_add_listener(m_xdg_toplevel, &gs_wl_xdg_toplevel_listener, this);

	wl_surface_commit(m_wl_surface);

	wl_display_flush(m_NativeDisplay);
	wl_display_roundtrip(m_NativeDisplay);

	return m_NativeWindow;
}

void GSWndEGL_WL::AttachNativeWindow(void *handle, void **out_native_display, void **out_native_window)
{
	// the caller provides us with a display and surface through pDsp.
	// we are allowed to render whatever we want to the given surface.
	PluginDisplayPropertiesWayland *props = *(PluginDisplayPropertiesWayland **)handle;
	// we don't store the display or surface because we don't own them.

	*out_native_display = props->display;
	*out_native_window = props->egl_window;
}

void GSWndEGL_WL::DestroyNativeResources()
{
	if (m_NativeWindow) {
		wl_egl_window_destroy(m_NativeWindow);
		m_NativeWindow = nullptr;
	}

	if (m_xdg_toplevel) {
		xdg_toplevel_destroy(m_xdg_toplevel);
		m_xdg_toplevel = nullptr;
	}

	if (m_xdg_surface) {
		xdg_surface_destroy(m_xdg_surface);
		m_xdg_surface = nullptr;
	}

	if (m_wl_surface) {
		wl_surface_destroy(m_wl_surface);
		m_wl_surface = nullptr;
	}

	if (m_xdg_wm_base) {
		xdg_wm_base_destroy(m_xdg_wm_base);
		m_xdg_wm_base = nullptr;
	}

	if (m_wl_compositor) {
		wl_compositor_destroy(m_wl_compositor);
		m_wl_compositor = nullptr;
	}

	if (m_wl_registry) {
		wl_registry_destroy(m_wl_registry);
		m_wl_registry = nullptr;
	}

	if (m_NativeDisplay) {
		wl_display_flush(m_NativeDisplay);
		wl_display_disconnect(m_NativeDisplay);
		m_NativeDisplay = nullptr;
	}
}

bool GSWndEGL_WL::SetWindowText(const char* title)
{
	if (m_managed) {
		// we created our own window and thus should have a toplevel
		if (m_xdg_toplevel == nullptr) return false;

		xdg_toplevel_set_title(m_xdg_toplevel, title);
		return true;
	}
	else
	{
		// we did not create our own window so we can't fulfill this request
		return true;
	}
}

#endif

#endif
