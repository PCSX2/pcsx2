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

#include "common/GL/ContextEGL.h"

struct wl_egl_window;
struct wl_surface;

namespace GL
{
	class ContextEGLWayland final : public ContextEGL
	{
	public:
		ContextEGLWayland(const WindowInfo& wi);
		~ContextEGLWayland() override;

		static std::unique_ptr<Context> Create(const WindowInfo& wi, const Version* versions_to_try,
			size_t num_versions_to_try);

		std::unique_ptr<Context> CreateSharedContext(const WindowInfo& wi) override;
		void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;

	protected:
		EGLNativeWindowType GetNativeWindow(EGLConfig config) override;

	private:
		bool LoadModule();

		wl_egl_window* m_wl_window = nullptr;

		void* m_wl_module = nullptr;
		wl_egl_window* (*m_wl_egl_window_create)(struct wl_surface* surface, int width, int height);
		void (*m_wl_egl_window_destroy)(struct wl_egl_window* egl_window);
		void (*m_wl_egl_window_resize)(struct wl_egl_window* egl_window, int width, int height, int dx, int dy);
	};
} // namespace GL
