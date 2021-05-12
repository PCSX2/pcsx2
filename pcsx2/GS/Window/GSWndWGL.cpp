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

// Used by GSReplay. Perhaps the stuff used by GSReplay can be moved out? That way all
// the GSOpen 1 stuff can be removed. But that'll take a bit of thinking.
LRESULT CALLBACK GSWndWGL::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
			// This takes place before GSClose, so don't destroy the Window so we can clean up.
			ShowWindow(hWnd, SW_HIDE);
			// DestroyWindow(hWnd);
			return 0;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
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

	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	if (!wglCreateContextAttribsARB)
		win_error(L"Failed to init wglCreateContextAttribsARB function pointer");

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
	m_swapinterval = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	// To ease the process, extension management is itself an extension. Clever isn't it!
	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (wglGetExtensionsStringARB)
	{
		const char* ext = wglGetExtensionsStringARB(m_NativeDisplay);
		m_has_late_vsync = m_swapinterval && ext && strstr(ext, "WGL_EXT_swap_control_tear");
	}
	else
	{
		m_has_late_vsync = false;
	}
}

bool GSWndWGL::Attach(void* handle, bool managed)
{
	m_NativeWindow = (HWND)handle;
	m_managed = managed;

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

	// Used by GSReplay.
	if (m_NativeWindow && m_managed)
	{
		DestroyWindow(m_NativeWindow);
		m_NativeWindow = NULL;
	}
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

//TODO: GSopen 1 => Drop?
// Used by GSReplay. At least for now.
// More or less copy pasted from GSWndDX::Create and GSWndWGL::Attach with a few
// modifications
bool GSWndWGL::Create(const std::string& title, int w, int h)
{
	if (m_NativeWindow)
		return false;

	m_managed = true;

	WNDCLASS wc;

	memset(&wc, 0, sizeof(wc));

	wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = theApp.GetModuleHandle();
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = L"GSWndOGL";

	if (!GetClassInfo(wc.hInstance, wc.lpszClassName, &wc))
	{
		if (!RegisterClass(&wc))
		{
			return false;
		}
	}

	DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW | WS_BORDER;

	GSVector4i r;

	GetWindowRect(GetDesktopWindow(), r);

	// Old GSOpen ModeWidth and ModeHeight are not necessary with this.
	bool remote = !!GetSystemMetrics(SM_REMOTESESSION);

	if (w <= 0 || h <= 0 || remote)
	{
		w = r.width() / 3;
		h = r.width() / 4;

		if (!remote)
		{
			w *= 2;
			h *= 2;
		}
	}

	r.left = (r.left + r.right - w) / 2;
	r.top = (r.top + r.bottom - h) / 2;
	r.right = r.left + w;
	r.bottom = r.top + h;

	AdjustWindowRect(r, style, FALSE);

	std::wstring tmp = std::wstring(title.begin(), title.end());
	m_NativeWindow = CreateWindow(wc.lpszClassName, tmp.c_str(), style, r.left, r.top, r.width(), r.height(), NULL, NULL, wc.hInstance, (LPVOID)this);

	if (m_NativeWindow == NULL)
		return false;

	OpenWGLDisplay();

	FullContextInit();

	return true;
}

//Same as DX
GSVector4i GSWndWGL::GetClientRect()
{
	GSVector4i r;

	::GetClientRect(m_NativeWindow, r);

	return r;
}

void* GSWndWGL::GetProcAddress(const char* name, bool opt)
{
	void* ptr = (void*)wglGetProcAddress(name);
	// In order to get function pointer of GL1.0 and GL1.1 you need to get from
	// opengl32.dll directly
	// Here an example from https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions
	// Note: so far we use direct linking but it could become handy for the migration
	// to the new gl header (glcorearb.h)
#if 0
	if(ptr == 0 || (ptr == (void*)0x1) || (ptr == (void*)0x2) || (ptr == (void*)0x3) || (ptr == (void*)-1) )
	{
		HMODULE module = LoadLibraryA("opengl32.dll");
		ptr = (void *)GetProcAddress(module, name);
	}
#endif
	if (ptr == NULL)
	{
		if (theApp.GetConfigB("debug_opengl"))
			fprintf(stderr, "Failed to find %s\n", name);

		if (!opt)
			throw GSRecoverableError();
	}
	return ptr;
}

//TODO: check extensions supported or not
//FIXME : extension allocation
void GSWndWGL::SetSwapInterval()
{
	// m_swapinterval uses an integer as parameter
	// 0 -> disable vsync
	// n -> wait n frame
	if (m_swapinterval)
		m_swapinterval(m_vsync);
}

void GSWndWGL::Flip()
{
	if (m_vsync_change_requested.exchange(false))
		SetSwapInterval();

	SwapBuffers(m_NativeDisplay);
}

void GSWndWGL::Show()
{
	if (!m_managed)
		return;

	// Used by GSReplay
	SetForegroundWindow(m_NativeWindow);
	ShowWindow(m_NativeWindow, SW_SHOWNORMAL);
	UpdateWindow(m_NativeWindow);
}

void GSWndWGL::Hide()
{
}

void GSWndWGL::HideFrame()
{
}

// Returns FALSE if the window has no title, or if th window title is under the strict
// management of the emulator.

bool GSWndWGL::SetWindowText(const char* title)
{
	if (!m_managed)
		return false;

	const size_t tmp_size = strlen(title) + 1;
	std::wstring tmp(tmp_size, L'#');
	mbstowcs(&tmp[0], title, tmp_size);
	// Used by GSReplay.
	::SetWindowText(m_NativeWindow, tmp.c_str());

	return true;
}


#endif
