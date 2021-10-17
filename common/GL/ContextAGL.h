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
#include "glad.h"

#if defined(__APPLE__) && defined(__OBJC__)
#import <AppKit/AppKit.h>
#else
struct NSView;
struct NSOpenGLContext;
struct NSOpenGLPixelFormat;
#endif

namespace GL
{
	class ContextAGL final : public Context
	{
	public:
		ContextAGL(const WindowInfo& wi);
		~ContextAGL() override;

		static std::unique_ptr<Context> Create(const WindowInfo& wi, const Version* versions_to_try,
			size_t num_versions_to_try);

		void* GetProcAddress(const char* name) override;
		bool ChangeSurface(const WindowInfo& new_wi) override;
		void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
		bool SwapBuffers() override;
		bool MakeCurrent() override;
		bool DoneCurrent() override;
		bool SetSwapInterval(s32 interval) override;
		std::unique_ptr<Context> CreateSharedContext(const WindowInfo& wi) override;

	private:
		bool Initialize(const Version* versions_to_try, size_t num_versions_to_try);
		bool CreateContext(NSOpenGLContext* share_context, int profile, bool make_current);
		void BindContextToView();
		void CleanupView();

		// returns true if dimensions have changed
		bool UpdateDimensions();

		NSView* m_view = nullptr;
		NSOpenGLContext* m_context = nullptr;
		NSOpenGLPixelFormat* m_pixel_format = nullptr;
		void* m_opengl_module_handle = nullptr;
	};

} // namespace GL
