/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/PrecompiledHeader.h"

#include "common/Console.h"
#include "ContextWGL.h"

static void* GetProcAddressCallback(const char* name)
{
	void* addr = wglGetProcAddress(name);
	if (addr)
		return addr;

	// try opengl32.dll
	return ::GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}

namespace GL
{
	ContextWGL::ContextWGL(const WindowInfo& wi)
		: Context(wi)
	{
	}

	ContextWGL::~ContextWGL()
	{
		if (wglGetCurrentContext() == m_rc)
			wglMakeCurrent(m_dc, nullptr);

		if (m_rc)
			wglDeleteContext(m_rc);

		if (m_dc)
			ReleaseDC(GetHWND(), m_dc);
	}

	std::unique_ptr<Context> ContextWGL::Create(const WindowInfo& wi, const Version* versions_to_try,
		size_t num_versions_to_try)
	{
		std::unique_ptr<ContextWGL> context = std::make_unique<ContextWGL>(wi);
		if (!context->Initialize(versions_to_try, num_versions_to_try))
			return nullptr;

		return context;
	}

	bool ContextWGL::Initialize(const Version* versions_to_try, size_t num_versions_to_try)
	{
		if (m_wi.type == WindowInfo::Type::Win32)
		{
			if (!InitializeDC())
				return false;
		}
		else
		{
			Console.Error("PBuffer not implemented");
			return false;
		}

		// Everything including core/ES requires a dummy profile to load the WGL extensions.
		if (!CreateAnyContext(nullptr, true))
			return false;

		for (size_t i = 0; i < num_versions_to_try; i++)
		{
			const Version& cv = versions_to_try[i];
			if (cv.profile == Profile::NoProfile)
			{
				// we already have the dummy context, so just use that
				m_version = cv;
				return true;
			}
			else if (CreateVersionContext(cv, nullptr, true))
			{
				m_version = cv;
				return true;
			}
		}

		return false;
	}

	void* ContextWGL::GetProcAddress(const char* name)
	{
		return GetProcAddressCallback(name);
	}

	bool ContextWGL::ChangeSurface(const WindowInfo& new_wi)
	{
		const bool was_current = (wglGetCurrentContext() == m_rc);

		if (m_dc)
		{
			ReleaseDC(GetHWND(), m_dc);
			m_dc = {};
		}

		m_wi = new_wi;
		if (!InitializeDC())
			return false;

		if (was_current && !wglMakeCurrent(m_dc, m_rc))
		{
			Console.Error("Failed to make context current again after surface change: 0x%08X", GetLastError());
			return false;
		}

		return true;
	}

	void ContextWGL::ResizeSurface(u32 new_surface_width /*= 0*/, u32 new_surface_height /*= 0*/)
	{
		RECT client_rc = {};
		GetClientRect(GetHWND(), &client_rc);
		m_wi.surface_width = static_cast<u32>(client_rc.right - client_rc.left);
		m_wi.surface_height = static_cast<u32>(client_rc.bottom - client_rc.top);
	}

	bool ContextWGL::SwapBuffers()
	{
		return ::SwapBuffers(m_dc);
	}

	bool ContextWGL::MakeCurrent()
	{
		if (!wglMakeCurrent(m_dc, m_rc))
		{
			Console.Error("wglMakeCurrent() failed: 0x%08X", GetLastError());
			return false;
		}

		return true;
	}

	bool ContextWGL::DoneCurrent()
	{
		return wglMakeCurrent(m_dc, nullptr);
	}

	bool ContextWGL::SetSwapInterval(s32 interval)
	{
		if (!GLAD_WGL_EXT_swap_control)
			return false;

		return wglSwapIntervalEXT(interval);
	}

	std::unique_ptr<Context> ContextWGL::CreateSharedContext(const WindowInfo& wi)
	{
		std::unique_ptr<ContextWGL> context = std::make_unique<ContextWGL>(wi);
		if (wi.type == WindowInfo::Type::Win32)
		{
			if (!context->InitializeDC())
				return nullptr;
		}
		else
		{
			Console.Error("PBuffer not implemented");
			return nullptr;
		}

		if (m_version.profile == Profile::NoProfile)
		{
			if (!context->CreateAnyContext(m_rc, false))
				return nullptr;
		}
		else
		{
			if (!context->CreateVersionContext(m_version, m_rc, false))
				return nullptr;
		}

		context->m_version = m_version;
		return context;
	}

	bool ContextWGL::InitializeDC()
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

		m_dc = GetDC(GetHWND());
		if (!m_dc)
		{
			Console.Error("GetDC() failed: 0x%08X", GetLastError());
			return false;
		}

		const int pf = ChoosePixelFormat(m_dc, &pfd);
		if (pf == 0)
		{
			Console.Error("ChoosePixelFormat() failed: 0x%08X", GetLastError());
			return false;
		}

		if (!SetPixelFormat(m_dc, pf, &pfd))
		{
			Console.Error("SetPixelFormat() failed: 0x%08X", GetLastError());
			return false;
		}

		return true;
	}

	bool ContextWGL::CreateAnyContext(HGLRC share_context, bool make_current)
	{
		m_rc = wglCreateContext(m_dc);
		if (!m_rc)
		{
			Console.Error("wglCreateContext() failed: 0x%08X", GetLastError());
			return false;
		}

		if (make_current)
		{
			if (!wglMakeCurrent(m_dc, m_rc))
			{
				Console.Error("wglMakeCurrent() failed: 0x%08X", GetLastError());
				return false;
			}

			// re-init glad-wgl
			if (!gladLoadWGLLoader([](const char* name) -> void* { return wglGetProcAddress(name); }, m_dc))
			{
				Console.Error("Loading GLAD WGL functions failed");
				return false;
			}
		}

		if (share_context && !wglShareLists(share_context, m_rc))
		{
			Console.Error("wglShareLists() failed: 0x%08X", GetLastError());
			return false;
		}

		return true;
	}

	bool ContextWGL::CreateVersionContext(const Version& version, HGLRC share_context, bool make_current)
	{
		// we need create context attribs
		if (!GLAD_WGL_ARB_create_context)
		{
			Console.Error("Missing GLAD_WGL_ARB_create_context.");
			return false;
		}

		HGLRC new_rc;
		if (version.profile == Profile::Core)
		{
			const int attribs[] = {
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				WGL_CONTEXT_MAJOR_VERSION_ARB, version.major_version,
				WGL_CONTEXT_MINOR_VERSION_ARB, version.minor_version,
#ifdef _DEBUG
				WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
#else
				WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
#endif
				0, 0};

			new_rc = wglCreateContextAttribsARB(m_dc, share_context, attribs);
		}
		else if (version.profile == Profile::ES)
		{
			if ((version.major_version >= 2 && !GLAD_WGL_EXT_create_context_es2_profile) ||
				(version.major_version < 2 && !GLAD_WGL_EXT_create_context_es_profile))
			{
				Console.Error("WGL_EXT_create_context_es_profile not supported");
				return false;
			}

			const int attribs[] = {
				WGL_CONTEXT_PROFILE_MASK_ARB, ((version.major_version >= 2) ? WGL_CONTEXT_ES2_PROFILE_BIT_EXT : WGL_CONTEXT_ES_PROFILE_BIT_EXT),
				WGL_CONTEXT_MAJOR_VERSION_ARB, version.major_version,
				WGL_CONTEXT_MINOR_VERSION_ARB, version.minor_version,
				0, 0};

			new_rc = wglCreateContextAttribsARB(m_dc, share_context, attribs);
		}
		else
		{
			Console.Error("Unknown profile");
			return false;
		}

		if (!new_rc)
			return false;

		// destroy and swap contexts
		if (m_rc)
		{
			if (!wglMakeCurrent(m_dc, make_current ? new_rc : nullptr))
			{
				Console.Error("wglMakeCurrent() failed: 0x%08X", GetLastError());
				wglDeleteContext(new_rc);
				return false;
			}

			// re-init glad-wgl
			if (make_current && !gladLoadWGLLoader([](const char* name) -> void* { return wglGetProcAddress(name); }, m_dc))
			{
				Console.Error("Loading GLAD WGL functions failed");
				return false;
			}

			wglDeleteContext(m_rc);
		}

		m_rc = new_rc;
		return true;
	}
} // namespace GL
