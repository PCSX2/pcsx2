/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#pragma once

#include "GS/Renderers/OpenGL/GLContext.h"

#include "common/RedtapeWindows.h"

#include "glad_wgl.h"
#include "glad.h"

#include <optional>
#include <span>

class GLContextWGL final : public GLContext
{
public:
	GLContextWGL(const WindowInfo& wi);
	~GLContextWGL() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try);

	void* GetProcAddress(const char* name) override;
	bool ChangeSurface(const WindowInfo& new_wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
	bool SwapBuffers() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	bool SetSwapInterval(s32 interval) override;
	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;

private:
	__fi HWND GetHWND() const { return static_cast<HWND>(m_wi.window_handle); }

	HDC GetDCAndSetPixelFormat(HWND hwnd);

	bool Initialize(std::span<const Version> versions_to_try);
	bool InitializeDC();
	void ReleaseDC();
	bool CreatePBuffer();
	bool CreateAnyContext(HGLRC share_context, bool make_current);
	bool CreateVersionContext(const Version& version, HGLRC share_context, bool make_current);

	HDC m_dc = {};
	HGLRC m_rc = {};

	// Can't change pixel format once it's set for a RC.
	std::optional<int> m_pixel_format;

	// Dummy window for creating a PBuffer off when we're surfaceless.
	HWND m_dummy_window = {};
	HDC m_dummy_dc = {};
	HPBUFFERARB m_pbuffer = {};
};
