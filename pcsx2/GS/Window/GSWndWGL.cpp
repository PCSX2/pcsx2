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
#include "GSWndWGL.h"

#ifdef _WIN32

static void win_error(const wchar_t* msg, bool fatal = true)
{
	DWORD errorID = ::GetLastError();
	if (errorID)
		fprintf(stderr, "WIN API ERROR:%ld\t", errorID);

	if (fatal)
	{
		MessageBox(NULL, msg, L"ERROR", MB_OK | MB_ICONEXCLAMATION);
		throw GSRecoverableError();
	}
	else
	{
		fprintf(stderr, "ERROR:%ls\n", msg);
	}
}


GSWndWGL::GSWndWGL()
	: m_NativeWindow(nullptr), m_NativeDisplay(nullptr), m_context(nullptr), m_has_late_vsync(false)
{
}

void GSWndWGL::CreateContext(int major, int minor)
{
	if (!m_NativeDisplay || !m_NativeWindow)
	{
		win_error(L"Wrong display/window", false);
		exit(1);
	}

	ASSERT(major >= 3);

	// GL2 context are quite easy but we need GL3 which is another painful story...
	m_context = wglCreateContext(m_NativeDisplay);
	if (!m_context)
		win_error(L"Failed to create a 2.0 context");

	// FIXME test it
	// Note: albeit every tutorial said that we need an opengl context to use the GL function wglCreateContextAttribsARB
	// On linux it works without the extra temporary context, not sure the limitation still applied
	AttachContext();

	if (!gladLoadWGLLoader([](const char* name) { return static_cast<void*>(wglGetProcAddress(name)); }, m_NativeDisplay))
		win_error(L"Failed to load WGL");

	// Create a context
	int context_attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, major,
		WGL_CONTEXT_MINOR_VERSION_ARB, minor,
		// FIXME : Request a debug context to ease opengl development
		// Note: don't support deprecated feature (pre openg 3.1)
		//GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB | GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#ifdef ENABLE_OGL_DEBUG
			| WGL_CONTEXT_DEBUG_BIT_ARB
#else
			| GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR
#endif
			,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	HGLRC context30 = wglCreateContextAttribsARB(m_NativeDisplay, NULL, context_attribs);
	if (!context30)
	{
		win_error(L"Failed to create a 3.x context with standard flags", false);
		// retry with more compatible option for (Mesa on Windows, OpenGL on WINE)
		context_attribs[2 * 2 + 1] = 0;

		context30 = wglCreateContextAttribsARB(m_NativeDisplay, NULL, context_attribs);
	}

	DetachContext();
	wglDeleteContext(m_context);

	if (!context30)
		win_error(L"Failed to create a 3.x context with compatible flags");

	m_context = context30;
	fprintf(stdout, "3.x GL context successfully created\n");
}

void GSWndWGL::AttachContext()
{
	if (!IsContextAttached())
	{
		wglMakeCurrent(m_NativeDisplay, m_context);
		m_ctx_attached = true;
	}
}

void GSWndWGL::DetachContext()
{
	if (IsContextAttached())
	{
		wglMakeCurrent(NULL, NULL);
		m_ctx_attached = false;
	}
}

void GSWndWGL::PopulateWndGlFunction()
{
	m_has_late_vsync = GLAD_WGL_EXT_swap_control_tear;
}

bool GSWndWGL::Attach(const WindowInfo& wi)
{
	if (wi.type != WindowInfo::Type::Win32)
		return false;

	m_NativeWindow = static_cast<HWND>(wi.window_handle);

	OpenWGLDisplay();

	FullContextInit();

	UpdateWindow(m_NativeWindow);

	return true;
}

void GSWndWGL::Detach()
{
	// Actually the destructor is not called when there is only a GSclose/GSshutdown
	// The window still need to be closed
	DetachContext();

	if (m_context)
		wglDeleteContext(m_context);
	m_context = NULL;

	CloseWGLDisplay();
}

void GSWndWGL::OpenWGLDisplay()
{
	GLuint PixelFormat;                // Holds The Results After Searching For A Match
	PIXELFORMATDESCRIPTOR pfd =        // pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR), // Size Of This Pixel Format Descriptor
		1,                             // Version Number
		PFD_DRAW_TO_WINDOW |           // Format Must Support Window
		PFD_SUPPORT_OPENGL |           // Format Must Support OpenGL
		PFD_DOUBLEBUFFER,              // Must Support Double Buffering
		PFD_TYPE_RGBA,                 // Request An RGBA Format
		32,                            // Select Our Color Depth
		0, 0, 0, 0, 0, 0,              // Color Bits Ignored
		0,                             // 8bit Alpha Buffer
		0,                             // Shift Bit Ignored
		0,                             // No Accumulation Buffer
		0, 0, 0, 0,                    // Accumulation Bits Ignored
		0,                             // 24Bit Z-Buffer (Depth Buffer)
		8,                             // 8bit Stencil Buffer
		0,                             // No Auxiliary Buffer
		PFD_MAIN_PLANE,                // Main Drawing Layer
		0,                             // Reserved
		0, 0, 0                        // Layer Masks Ignored
	};

	m_NativeDisplay = GetDC(m_NativeWindow);
	if (!m_NativeDisplay)
		win_error(L"(1) Can't Create A GL Device Context.");

	PixelFormat = ChoosePixelFormat(m_NativeDisplay, &pfd);
	if (!PixelFormat)
		win_error(L"(2) Can't Find A Suitable PixelFormat.");

	if (!SetPixelFormat(m_NativeDisplay, PixelFormat, &pfd))
		win_error(L"(3) Can't Set The PixelFormat.", false);
}

void GSWndWGL::CloseWGLDisplay()
{
	if (m_NativeDisplay && !ReleaseDC(m_NativeWindow, m_NativeDisplay))
		win_error(L"Release Device Context Failed.");

	m_NativeDisplay = NULL;
}

//Same as DX
GSVector4i GSWndWGL::GetClientRect()
{
	GSVector4i r;

	::GetClientRect(m_NativeWindow, r);

	return r;
}

//TODO: check extensions supported or not
//FIXME : extension allocation
void GSWndWGL::SetSwapInterval()
{
	// m_swapinterval uses an integer as parameter
	// 0 -> disable vsync
	// n -> wait n frame
	if (GLAD_WGL_EXT_swap_control)
		wglSwapIntervalEXT(m_vsync);
}

void GSWndWGL::Flip()
{
	if (m_vsync_change_requested.exchange(false))
		SetSwapInterval();

	SwapBuffers(m_NativeDisplay);
}
#endif
