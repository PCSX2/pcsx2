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

#include "GSWnd.h"

#ifdef _WIN32

#include "glad_wgl.h"

class GSWndWGL final : public GSWndGL
{
	HWND  m_NativeWindow;
	HDC   m_NativeDisplay;
	HGLRC m_context;
	bool  m_has_late_vsync;

	void PopulateWndGlFunction();
	void CreateContext(int major, int minor);

	void CloseWGLDisplay();
	void OpenWGLDisplay();

	void SetSwapInterval();
	bool HasLateVsyncSupport() { return m_has_late_vsync; }

public:
	GSWndWGL();
	~GSWndWGL() override = default;

	bool Attach(const WindowInfo& wi) override;
	void Detach() override;

	void* GetHandle() override { return m_NativeWindow; }
	GSVector4i GetClientRect() override;

	void AttachContext() override;
	void DetachContext() override;

	void Flip() override;
};

#endif
