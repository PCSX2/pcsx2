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

#pragma once

#include "common/GL/Context.h"

#include "glad_wgl.h"
#include "glad.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <optional>

namespace GL
{
	class ContextWGL final : public Context
	{
	public:
		ContextWGL(const WindowInfo& wi);
		~ContextWGL() override;

		static std::unique_ptr<Context> Create(const WindowInfo& wi, gsl::span<const Version> versions_to_try);

		void* GetProcAddress(const char* name) override;
		bool ChangeSurface(const WindowInfo& new_wi) override;
		void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
		bool SwapBuffers() override;
		bool MakeCurrent() override;
		bool DoneCurrent() override;
		bool SetSwapInterval(s32 interval) override;
		std::unique_ptr<Context> CreateSharedContext(const WindowInfo& wi) override;

	private:
		__fi HWND GetHWND() const { return static_cast<HWND>(m_wi.window_handle); }

		HDC GetDCAndSetPixelFormat(HWND hwnd);

		bool Initialize(gsl::span<const Version> versions_to_try);
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
} // namespace GL