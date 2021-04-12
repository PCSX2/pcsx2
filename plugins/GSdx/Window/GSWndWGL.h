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

#include "GSWnd.h"

#ifdef _WIN32

class GSWndWGL : public GSWndGL
{
	HWND  m_NativeWindow;
	HDC   m_NativeDisplay;
	HGLRC m_context;
	bool  m_has_late_vsync;

	PFNWGLSWAPINTERVALEXTPROC m_swapinterval;

	void PopulateWndGlFunction();
	void CreateContext(int major, int minor);

	void CloseWGLDisplay();
	void OpenWGLDisplay();

	void SetSwapInterval();
	bool HasLateVsyncSupport() { return m_has_late_vsync; }

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	GSWndWGL();
	virtual ~GSWndWGL() {}

	bool Create(const std::string& title, int w, int h);
	bool Attach(void* handle, bool managed = true);
	void Detach();

	void* GetDisplay() { return m_NativeWindow; }
	void* GetHandle() { return m_NativeWindow; }
	GSVector4i GetClientRect();
	bool SetWindowText(const char* title);

	void AttachContext();
	void DetachContext();
	void* GetProcAddress(const char* name, bool opt);

	void Show();
	void Hide();
	void HideFrame();
	void Flip();
};

#endif
