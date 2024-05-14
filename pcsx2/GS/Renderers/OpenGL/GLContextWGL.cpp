// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextWGL.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/ScopedGuard.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wmicrosoft-cast"
#endif

static void* GetProcAddressCallback(const char* name)
{
	void* addr = wglGetProcAddress(name);
	if (addr)
		return addr;

	// try opengl32.dll
	return ::GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}

static bool ReloadWGL(HDC dc)
{
	if (!gladLoadWGL(dc, [](const char* name) { return (GLADapiproc)wglGetProcAddress(name); }))
	{
		Console.Error("Loading GLAD WGL functions failed");
		return false;
	}

	return true;
}

GLContextWGL::GLContextWGL(const WindowInfo& wi)
	: GLContext(wi)
{
}

GLContextWGL::~GLContextWGL()
{
	if (wglGetCurrentContext() == m_rc)
		wglMakeCurrent(m_dc, nullptr);

	if (m_rc)
		wglDeleteContext(m_rc);

	ReleaseDC();
}

std::unique_ptr<GLContext> GLContextWGL::Create(const WindowInfo& wi, std::span<const Version> versions_to_try,
	Error* error)
{
	std::unique_ptr<GLContextWGL> context = std::make_unique<GLContextWGL>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	return context;
}

bool GLContextWGL::Initialize(std::span<const Version> versions_to_try, Error* error)
{
	if (m_wi.type == WindowInfo::Type::Win32)
	{
		if (!InitializeDC(error))
			return false;
	}
	else
	{
		if (!CreatePBuffer(error))
			return false;
	}

	// Everything including core/ES requires a dummy profile to load the WGL extensions.
	if (!CreateAnyContext(nullptr, true, error))
		return false;

	for (const Version& cv : versions_to_try)
	{
		if (CreateVersionContext(cv, nullptr, true, error))
		{
			m_version = cv;
			return true;
		}
	}

	Error::SetStringView(error, "Failed to create any contexts.");
	return false;
}

void* GLContextWGL::GetProcAddress(const char* name)
{
	return GetProcAddressCallback(name);
}

bool GLContextWGL::ChangeSurface(const WindowInfo& new_wi)
{
	const bool was_current = (wglGetCurrentContext() == m_rc);
	Error error;

	ReleaseDC();

	m_wi = new_wi;
	if (!InitializeDC(&error))
	{
		Console.ErrorFmt("Failed to change surface: {}", error.GetDescription());
		return false;
	}

	if (was_current && !wglMakeCurrent(m_dc, m_rc))
	{
		error.SetWin32(GetLastError());
		Console.ErrorFmt("Failed to make context current again after surface change: {}", error.GetDescription());
		return false;
	}

	return true;
}

void GLContextWGL::ResizeSurface(u32 new_surface_width /*= 0*/, u32 new_surface_height /*= 0*/)
{
	RECT client_rc = {};
	GetClientRect(GetHWND(), &client_rc);
	m_wi.surface_width = static_cast<u32>(client_rc.right - client_rc.left);
	m_wi.surface_height = static_cast<u32>(client_rc.bottom - client_rc.top);
}

bool GLContextWGL::SwapBuffers()
{
	return ::SwapBuffers(m_dc);
}

bool GLContextWGL::IsCurrent()
{
	return (m_rc && wglGetCurrentContext() == m_rc);
}

bool GLContextWGL::MakeCurrent()
{
	if (!wglMakeCurrent(m_dc, m_rc))
	{
		Console.ErrorFmt("wglMakeCurrent() failed: {}", GetLastError());
		return false;
	}

	return true;
}

bool GLContextWGL::DoneCurrent()
{
	return wglMakeCurrent(m_dc, nullptr);
}

bool GLContextWGL::SupportsNegativeSwapInterval() const
{
	return GLAD_WGL_EXT_swap_control && GLAD_WGL_EXT_swap_control_tear;
}

bool GLContextWGL::SetSwapInterval(s32 interval)
{
	if (!GLAD_WGL_EXT_swap_control)
		return false;

	return wglSwapIntervalEXT(interval);
}

std::unique_ptr<GLContext> GLContextWGL::CreateSharedContext(const WindowInfo& wi, Error* error)
{
	std::unique_ptr<GLContextWGL> context = std::make_unique<GLContextWGL>(wi);
	if (wi.type == WindowInfo::Type::Win32)
	{
		if (!context->InitializeDC(error))
			return nullptr;
	}
	else
	{
		if (!context->CreatePBuffer(error))
			return nullptr;
	}

	if (!context->CreateVersionContext(m_version, m_rc, false, error))
		return nullptr;

	context->m_version = m_version;
	return context;
}

HDC GLContextWGL::GetDCAndSetPixelFormat(HWND hwnd, Error* error)
{
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.dwLayerMask = PFD_MAIN_PLANE;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cColorBits = 24;

	HDC hDC = ::GetDC(hwnd);
	if (!hDC)
	{
		Error::SetWin32(error, "GetDC() failed: ", GetLastError());
		return {};
	}

	if (!m_pixel_format.has_value())
	{
		const int pf = ChoosePixelFormat(hDC, &pfd);
		if (pf == 0)
		{
			Error::SetWin32(error, "ChoosePixelFormat() failed: ", GetLastError());
			::ReleaseDC(hwnd, hDC);
			return {};
		}

		m_pixel_format = pf;
	}

	if (!SetPixelFormat(hDC, m_pixel_format.value(), &pfd))
	{
		Error::SetWin32(error, "SetPixelFormat() failed: ", GetLastError());
		::ReleaseDC(hwnd, hDC);
		return {};
	}

	return hDC;
}

bool GLContextWGL::InitializeDC(Error* error)
{
	if (m_wi.type == WindowInfo::Type::Win32)
	{
		m_dc = GetDCAndSetPixelFormat(GetHWND(), error);
		if (!m_dc)
			return false;

		return true;
	}
	else if (m_wi.type == WindowInfo::Type::Surfaceless)
	{
		return CreatePBuffer(error);
	}
	else
	{
		Error::SetStringFmt(error, "Unknown window info type {}", static_cast<unsigned>(m_wi.type));
		return false;
	}
}

void GLContextWGL::ReleaseDC()
{
	if (m_pbuffer)
	{
		wglReleasePbufferDCARB(m_pbuffer, m_dc);
		m_dc = {};

		wglDestroyPbufferARB(m_pbuffer);
		m_pbuffer = {};

		::ReleaseDC(m_dummy_window, m_dummy_dc);
		m_dummy_dc = {};

		DestroyWindow(m_dummy_window);
		m_dummy_window = {};
	}
	else if (m_dc)
	{
		::ReleaseDC(GetHWND(), m_dc);
		m_dc = {};
	}
}

bool GLContextWGL::CreatePBuffer(Error* error)
{
	static bool window_class_registered = false;
	static const wchar_t* window_class_name = L"ContextWGLPBuffer";

	if (!window_class_registered)
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = 0;
		wc.lpfnWndProc = DefWindowProcW;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.hIcon = NULL;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = window_class_name;
		wc.hIconSm = NULL;

		if (!RegisterClassExW(&wc))
		{
			Error::SetStringView(error, "(ContextWGL::CreatePBuffer) RegisterClassExW() failed");
			return false;
		}

		window_class_registered = true;
	}

	HWND hwnd = CreateWindowExW(0, window_class_name, window_class_name, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
	if (!hwnd)
	{
		Error::SetStringView(error, "(ContextWGL::CreatePBuffer) CreateWindowEx() failed");
		return false;
	}

	ScopedGuard hwnd_guard([hwnd]() { DestroyWindow(hwnd); });

	HDC hdc = GetDCAndSetPixelFormat(hwnd, error);
	if (!hdc)
		return false;

	ScopedGuard hdc_guard([hdc, hwnd]() { ::ReleaseDC(hwnd, hdc); });

	static constexpr const int pb_attribs[] = {0, 0};

	HGLRC temp_rc = nullptr;
	ScopedGuard temp_rc_guard([&temp_rc, hdc]() {
		if (temp_rc)
		{
			wglMakeCurrent(hdc, nullptr);
			wglDeleteContext(temp_rc);
		}
	});

	if (!GLAD_WGL_ARB_pbuffer)
	{
		// we're probably running completely surfaceless... need a temporary context.
		temp_rc = wglCreateContext(hdc);
		if (!temp_rc || !wglMakeCurrent(hdc, temp_rc))
		{
			Error::SetStringView(error, "Failed to create temporary context to load WGL for pbuffer.");
			return false;
		}

		if (!ReloadWGL(hdc) || !GLAD_WGL_ARB_pbuffer)
		{
			Error::SetStringView(error, "Missing WGL_ARB_pbuffer");
			return false;
		}
	}

	pxAssertRel(m_pixel_format.has_value(), "Has pixel format for pbuffer");
	HPBUFFERARB pbuffer = wglCreatePbufferARB(hdc, m_pixel_format.value(), 1, 1, pb_attribs);
	if (!pbuffer)
	{
		Error::SetStringView(error, "(ContextWGL::CreatePBuffer) wglCreatePbufferARB() failed");
		return false;
	}

	ScopedGuard pbuffer_guard([pbuffer]() { wglDestroyPbufferARB(pbuffer); });

	m_dc = wglGetPbufferDCARB(pbuffer);
	if (!m_dc)
	{
		Error::SetStringView(error, "(ContextWGL::CreatePbuffer) wglGetPbufferDCARB() failed");
		return false;
	}

	m_dummy_window = hwnd;
	m_dummy_dc = hdc;
	m_pbuffer = pbuffer;

	temp_rc_guard.Run();
	pbuffer_guard.Cancel();
	hdc_guard.Cancel();
	hwnd_guard.Cancel();
	return true;
}

bool GLContextWGL::CreateAnyContext(HGLRC share_context, bool make_current, Error* error)
{
	m_rc = wglCreateContext(m_dc);
	if (!m_rc)
	{
		Error::SetWin32(error, "wglCreateContext() failed: ", GetLastError());
		return false;
	}

	if (make_current)
	{
		if (!wglMakeCurrent(m_dc, m_rc))
		{
			Error::SetWin32(error, "wglMakeCurrent() failed: ", GetLastError());
			return false;
		}

		// re-init glad-wgl
		if (!gladLoadWGL(m_dc, [](const char* name) { return (GLADapiproc)wglGetProcAddress(name); }))
		{
			Error::SetStringView(error, "Loading GLAD WGL functions failed");
			return false;
		}
	}

	if (share_context && !wglShareLists(share_context, m_rc))
	{
		Error::SetWin32(error, "wglShareLists() failed: ", GetLastError());
		return false;
	}

	return true;
}

bool GLContextWGL::CreateVersionContext(const Version& version, HGLRC share_context, bool make_current,
	Error* error)
{
	// we need create context attribs
	if (!GLAD_WGL_ARB_create_context)
	{
		Error::SetStringView(error, "Missing GLAD_WGL_ARB_create_context.");
		return false;
	}

	const int attribs[] = {WGL_CONTEXT_PROFILE_MASK_ARB,
		WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		WGL_CONTEXT_MAJOR_VERSION_ARB,
		version.major_version,
		WGL_CONTEXT_MINOR_VERSION_ARB,
		version.minor_version,
#ifdef _DEBUG
		WGL_CONTEXT_FLAGS_ARB,
		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
#else
		WGL_CONTEXT_FLAGS_ARB,
		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
		0,
		0};

	HGLRC new_rc = wglCreateContextAttribsARB(m_dc, share_context, attribs);
	if (!new_rc)
		return false;

	// destroy and swap contexts
	if (m_rc)
	{
		if (!wglMakeCurrent(m_dc, make_current ? new_rc : nullptr))
		{
			Error::SetWin32(error, "wglMakeCurrent() failed: ", GetLastError());
			wglDeleteContext(new_rc);
			return false;
		}

		// re-init glad-wgl
		if (make_current && !ReloadWGL(m_dc))
			return false;

		wglDeleteContext(m_rc);
	}

	m_rc = new_rc;
	return true;
}
