// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGL.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/DynamicLibrary.h"
#include "common/Error.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <optional>
#include <vector>

static DynamicLibrary s_egl_library;
static std::atomic_uint32_t s_egl_refcount = 0;

static bool LoadEGL()
{
	// We're not going to be calling this from multiple threads concurrently.
	// So, not wrapping this in a mutex should be fine.
	if (s_egl_refcount.fetch_add(1, std::memory_order_acq_rel) == 0)
	{
		pxAssert(!s_egl_library.IsOpen());

		std::string egl_libname = DynamicLibrary::GetVersionedFilename("libEGL");
		Console.WriteLnFmt("Loading EGL from {}...", egl_libname);

		Error error;
		if (!s_egl_library.Open(egl_libname.c_str(), &error))
		{
			// Try versioned.
			egl_libname = DynamicLibrary::GetVersionedFilename("libEGL", 1);
			Console.WriteLnFmt("Loading EGL from {}...", egl_libname);
			if (!s_egl_library.Open(egl_libname.c_str(), &error))
				Console.ErrorFmt("Failed to load EGL: {}", error.GetDescription());
		}
	}

	return s_egl_library.IsOpen();
}

static void UnloadEGL()
{
	pxAssert(s_egl_refcount.load(std::memory_order_acquire) > 0);
	if (s_egl_refcount.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		Console.WriteLn("Unloading EGL.");
		s_egl_library.Close();
	}
}

static bool LoadGLADEGL(EGLDisplay display, Error* error)
{
	const int version =
		gladLoadEGL(display, [](const char* name) { return (GLADapiproc)s_egl_library.GetSymbolAddress(name); });
	if (version == 0)
	{
		Error::SetStringView(error, "Loading GLAD EGL functions failed");
		return false;
	}

	Console.WriteLnFmt("GLAD EGL Version: {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
	return true;
}

GLContextEGL::GLContextEGL(const WindowInfo& wi)
	: GLContext(wi)
{
	LoadEGL();
}

GLContextEGL::~GLContextEGL()
{
	DestroySurface();
	DestroyContext();
	UnloadEGL();
}

std::unique_ptr<GLContext> GLContextEGL::Create(const WindowInfo& wi, std::span<const Version> versions_to_try,
	Error* error)
{
	std::unique_ptr<GLContextEGL> context = std::make_unique<GLContextEGL>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	return context;
}

bool GLContextEGL::Initialize(std::span<const Version> versions_to_try, Error* error)
{
	if (!LoadGLADEGL(EGL_NO_DISPLAY, error))
		return false;

	m_display = GetPlatformDisplay(error);
	if (m_display == EGL_NO_DISPLAY)
		return false;

	int egl_major, egl_minor;
	if (!eglInitialize(m_display, &egl_major, &egl_minor))
	{
		const int gerror = static_cast<int>(eglGetError());
		Error::SetStringFmt(error, "eglInitialize() failed: {} (0x{:X})", gerror, gerror);
		return false;
	}

	Console.WriteLnFmt("eglInitialize() version: {}.{}", egl_major, egl_minor);

	// Re-initialize EGL/GLAD.
	if (!LoadGLADEGL(m_display, error))
		return false;

	if (!GLAD_EGL_KHR_surfaceless_context)
		Console.Warning("EGL implementation does not support surfaceless contexts, emulating with pbuffers");

	for (const Version& cv : versions_to_try)
	{
		if (CreateContextAndSurface(cv, nullptr, true))
			return true;
	}

	Error::SetStringView(error, "Failed to create any context versions");
	return false;
}

EGLDisplay GLContextEGL::GetPlatformDisplay(Error* error)
{
	EGLDisplay dpy = TryGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, "EGL_MESA_platform_surfaceless");
	if (dpy == EGL_NO_DISPLAY)
		dpy = GetFallbackDisplay(error);

	return dpy;
}

EGLSurface GLContextEGL::CreatePlatformSurface(EGLConfig config, void* win, Error* error)
{
	EGLSurface surface = TryCreatePlatformSurface(config, win, error);
	if (!surface)
		surface = CreateFallbackSurface(config, win, error);
	return surface;
}

EGLDisplay GLContextEGL::TryGetPlatformDisplay(EGLenum platform, const char* platform_ext)
{
	const char* extensions_str = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (!extensions_str)
	{
		Console.WriteLn("No extensions supported.");
		return EGL_NO_DISPLAY;
	}

	EGLDisplay dpy = EGL_NO_DISPLAY;
	if (platform_ext && std::strstr(extensions_str, platform_ext))
	{
		Console.WriteLnFmt("Using EGL platform {}.", platform_ext);

		PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display_ext =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
		if (get_platform_display_ext)
		{
			dpy = get_platform_display_ext(platform, m_wi.display_connection, nullptr);
			m_use_ext_platform_base = (dpy != EGL_NO_DISPLAY);
			if (!m_use_ext_platform_base)
			{
				const EGLint err = eglGetError();
				Console.ErrorFmt("eglGetPlatformDisplayEXT() failed: {} (0x{:X})", err, err);
			}
		}
		else
		{
			Console.Warning("eglGetPlatformDisplayEXT() was not found");
		}
	}
	else
	{
		Console.WarningFmt("{} is not supported.", platform_ext);
	}

	return dpy;
}

EGLSurface GLContextEGL::TryCreatePlatformSurface(EGLConfig config, void* win, Error* error)
{
	EGLSurface surface = EGL_NO_SURFACE;
	if (m_use_ext_platform_base)
	{
		PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC create_platform_window_surface_ext =
			(PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
		if (create_platform_window_surface_ext)
		{
			surface = create_platform_window_surface_ext(m_display, config, win, nullptr);
			if (surface == EGL_NO_SURFACE)
			{
				const EGLint err = eglGetError();
				Error::SetStringFmt(error, "eglCreatePlatformWindowSurfaceEXT() failed: {} (0x{:X})", err, err);
			}
		}
		else
		{
			Console.Error("eglCreatePlatformWindowSurfaceEXT() not found");
		}
	}

	return surface;
}

EGLDisplay GLContextEGL::GetFallbackDisplay(Error* error)
{
	Console.Warning("Using fallback eglGetDisplay() path.");

	EGLDisplay dpy = eglGetDisplay(m_wi.display_connection);
	if (dpy == EGL_NO_DISPLAY)
	{
		const EGLint err = eglGetError();
		Error::SetStringFmt(error, "eglGetDisplay() failed: {} (0x{:X})", err, err);
	}

	return dpy;
}

EGLSurface GLContextEGL::CreateFallbackSurface(EGLConfig config, void* win, Error* error)
{
	Console.Warning("Using fallback eglCreateWindowSurface() path.");

	EGLSurface surface = eglCreateWindowSurface(m_display, config, (EGLNativeWindowType)win, nullptr);
	if (surface == EGL_NO_SURFACE)
	{
		const EGLint err = eglGetError();
		Error::SetStringFmt(error, "eglCreateWindowSurface() failed: {} (0x{:X})", err, err);
	}

	return surface;
}

void* GLContextEGL::GetProcAddress(const char* name)
{
	return reinterpret_cast<void*>(eglGetProcAddress(name));
}

bool GLContextEGL::ChangeSurface(const WindowInfo& new_wi)
{
	const bool was_current = (eglGetCurrentContext() == m_context);
	if (was_current)
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (m_surface != EGL_NO_SURFACE)
	{
		eglDestroySurface(m_display, m_surface);
		m_surface = EGL_NO_SURFACE;
	}

	m_wi = new_wi;
	if (!CreateSurface())
		return false;

	if (was_current && !eglMakeCurrent(m_display, m_surface, m_surface, m_context))
	{
		Console.Error("Failed to make context current again after surface change");
		return false;
	}

	return true;
}

void GLContextEGL::ResizeSurface(u32 new_surface_width /*= 0*/, u32 new_surface_height /*= 0*/)
{
	if (new_surface_width == 0 && new_surface_height == 0)
	{
		EGLint surface_width, surface_height;
		if (eglQuerySurface(m_display, m_surface, EGL_WIDTH, &surface_width) &&
			eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &surface_height))
		{
			m_wi.surface_width = static_cast<u32>(surface_width);
			m_wi.surface_height = static_cast<u32>(surface_height);
			return;
		}
		else
		{
			Console.ErrorFmt("eglQuerySurface() failed: 0x{:x}", eglGetError());
		}
	}

	m_wi.surface_width = new_surface_width;
	m_wi.surface_height = new_surface_height;
}

bool GLContextEGL::SwapBuffers()
{
	return eglSwapBuffers(m_display, m_surface);
}

bool GLContextEGL::IsCurrent()
{
	return m_context && eglGetCurrentContext() == m_context;
}

bool GLContextEGL::MakeCurrent()
{
	if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context))
	{
		Console.ErrorFmt("eglMakeCurrent() failed: 0x{:x}", eglGetError());
		return false;
	}

	return true;
}

bool GLContextEGL::DoneCurrent()
{
	return eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool GLContextEGL::SupportsNegativeSwapInterval() const
{
	return m_supports_negative_swap_interval;
}

bool GLContextEGL::SetSwapInterval(s32 interval)
{
	return eglSwapInterval(m_display, interval);
}

std::unique_ptr<GLContext> GLContextEGL::CreateSharedContext(const WindowInfo& wi, Error* error)
{
	std::unique_ptr<GLContextEGL> context = std::make_unique<GLContextEGL>(wi);
	context->m_display = m_display;

	if (!context->CreateContextAndSurface(m_version, m_context, false))
	{
		Error::SetStringView(error, "Failed to create context/surface");
		return nullptr;
	}

	return context;
}

bool GLContextEGL::CreateSurface()
{
	if (m_wi.type == WindowInfo::Type::Surfaceless)
	{
		if (GLAD_EGL_KHR_surfaceless_context)
			return true;
		else
			return CreatePBufferSurface();
	}

	Error error;
	m_surface = CreatePlatformSurface(m_config, m_wi.window_handle, &error);
	if (m_surface == EGL_NO_SURFACE)
	{
		Console.ErrorFmt("Failed to create platform surface: {}", error.GetDescription());
		return false;
	}

	// Some implementations may require the size to be queried at runtime.
	EGLint surface_width, surface_height;
	if (eglQuerySurface(m_display, m_surface, EGL_WIDTH, &surface_width) &&
		eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &surface_height))
	{
		m_wi.surface_width = static_cast<u32>(surface_width);
		m_wi.surface_height = static_cast<u32>(surface_height);
	}
	else
	{
		Console.ErrorFmt("eglQuerySurface() failed: 0x{:x}", eglGetError());
	}

	return true;
}

bool GLContextEGL::CreatePBufferSurface()
{
	const u32 width = std::max<u32>(m_wi.surface_width, 1);
	const u32 height = std::max<u32>(m_wi.surface_height, 1);

	// TODO: Format
	EGLint attrib_list[] = {
		EGL_WIDTH,
		static_cast<EGLint>(width),
		EGL_HEIGHT,
		static_cast<EGLint>(height),
		EGL_NONE,
	};

	m_surface = eglCreatePbufferSurface(m_display, m_config, attrib_list);
	if (!m_surface)
	{
		Console.Error("eglCreatePbufferSurface() failed: 0x{:x}", eglGetError());
		return false;
	}

	DevCon.WriteLnFmt("Created {}x{} pbuffer surface", width, height);
	return true;
}

bool GLContextEGL::CheckConfigSurfaceFormat(EGLConfig config)
{
	int red_size, green_size, blue_size, alpha_size;
	if (!eglGetConfigAttrib(m_display, config, EGL_RED_SIZE, &red_size) ||
		!eglGetConfigAttrib(m_display, config, EGL_GREEN_SIZE, &green_size) ||
		!eglGetConfigAttrib(m_display, config, EGL_BLUE_SIZE, &blue_size) ||
		!eglGetConfigAttrib(m_display, config, EGL_ALPHA_SIZE, &alpha_size))
	{
		return false;
	}

	return (red_size == 8 && green_size == 8 && blue_size == 8);
}

void GLContextEGL::DestroyContext()
{
	if (eglGetCurrentContext() == m_context)
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (m_context != EGL_NO_CONTEXT)
	{
		eglDestroyContext(m_display, m_context);
		m_context = EGL_NO_CONTEXT;
	}
}

void GLContextEGL::DestroySurface()
{
	if (eglGetCurrentSurface(EGL_DRAW) == m_surface)
		eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (m_surface != EGL_NO_SURFACE)
	{
		eglDestroySurface(m_display, m_surface);
		m_surface = EGL_NO_SURFACE;
	}
}

bool GLContextEGL::CreateContext(const Version& version, EGLContext share_context)
{
	DevCon.WriteLnFmt("Trying GL version {}.{}", version.major_version, version.minor_version);
	const int surface_attribs[] = {
		EGL_RENDERABLE_TYPE,
		EGL_OPENGL_BIT,
		EGL_SURFACE_TYPE,
		(m_wi.type != WindowInfo::Type::Surfaceless) ? EGL_WINDOW_BIT : 0,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8, EGL_NONE, 0};

	EGLint num_configs;
	if (!eglChooseConfig(m_display, surface_attribs, nullptr, 0, &num_configs) || num_configs == 0)
	{
		Console.ErrorFmt("eglChooseConfig() failed: 0x{:x}", eglGetError());
		return false;
	}

	std::vector<EGLConfig> configs(static_cast<u32>(num_configs));
	if (!eglChooseConfig(m_display, surface_attribs, configs.data(), num_configs, &num_configs))
	{
		Console.ErrorFmt("eglChooseConfig() failed: 0x{:x}", eglGetError());
		return false;
	}
	configs.resize(static_cast<u32>(num_configs));

	std::optional<EGLConfig> config;
	for (EGLConfig check_config : configs)
	{
		if (CheckConfigSurfaceFormat(check_config))
		{
			config = check_config;
			break;
		}
	}

	if (!config.has_value())
	{
		Console.Warning("No EGL configs matched exactly, using first.");
		config = configs.front();
	}

	const int attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION,
		version.major_version,
		EGL_CONTEXT_MINOR_VERSION,
		version.minor_version,
		EGL_NONE,
		0};

	if (!eglBindAPI(EGL_OPENGL_API))
	{
		Console.ErrorFmt("eglBindAPI() failed: 0x{:x}", eglGetError());
		return false;
	}

	m_context = eglCreateContext(m_display, config.value(), share_context, attribs);
	if (!m_context)
	{
		Console.ErrorFmt("eglCreateContext() failed: 0x{:x}", eglGetError());
		return false;
	}

	Console.WriteLnFmt("Got GL version {}.{}", version.major_version, version.minor_version);

	EGLint min_swap_interval, max_swap_interval;
	m_supports_negative_swap_interval = false;
	if (eglGetConfigAttrib(m_display, config.value(), EGL_MIN_SWAP_INTERVAL, &min_swap_interval) &&
		eglGetConfigAttrib(m_display, config.value(), EGL_MAX_SWAP_INTERVAL, &max_swap_interval))
	{
		DEV_LOG("EGL_MIN_SWAP_INTERVAL = {}", min_swap_interval);
		DEV_LOG("EGL_MAX_SWAP_INTERVAL = {}", max_swap_interval);
		m_supports_negative_swap_interval = (min_swap_interval <= -1);
	}

	INFO_LOG("Negative swap interval/tear-control is {}supported", m_supports_negative_swap_interval ? "" : "NOT ");

	m_config = config.value();
	m_version = version;
	return true;
}

bool GLContextEGL::CreateContextAndSurface(const Version& version, EGLContext share_context, bool make_current)
{
	if (!CreateContext(version, share_context))
		return false;

	if (!CreateSurface())
	{
		Console.Error("Failed to create surface for context");
		eglDestroyContext(m_display, m_context);
		m_context = EGL_NO_CONTEXT;
		return false;
	}

	if (make_current && !eglMakeCurrent(m_display, m_surface, m_surface, m_context))
	{
		Console.ErrorFmt("eglMakeCurrent() failed: 0x{:x}", eglGetError());
		if (m_surface != EGL_NO_SURFACE)
		{
			eglDestroySurface(m_display, m_surface);
			m_surface = EGL_NO_SURFACE;
		}
		eglDestroyContext(m_display, m_context);
		m_context = EGL_NO_CONTEXT;
		return false;
	}

	return true;
}
