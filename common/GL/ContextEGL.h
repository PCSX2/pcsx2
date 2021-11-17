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
#include "glad_egl.h"

namespace GL
{
	class ContextEGL : public Context
	{
	public:
		ContextEGL(const WindowInfo& wi);
		~ContextEGL() override;

		static std::unique_ptr<Context> Create(const WindowInfo& wi, const Version* versions_to_try,
			size_t num_versions_to_try);

		void* GetProcAddress(const char* name) override;
		virtual bool ChangeSurface(const WindowInfo& new_wi) override;
		virtual void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
		bool SwapBuffers() override;
		bool MakeCurrent() override;
		bool DoneCurrent() override;
		bool SetSwapInterval(s32 interval) override;
		virtual std::unique_ptr<Context> CreateSharedContext(const WindowInfo& wi) override;

	protected:
		virtual bool SetDisplay();
		virtual EGLNativeWindowType GetNativeWindow(EGLConfig config);

		bool Initialize(const Version* versions_to_try, size_t num_versions_to_try);
		bool CreateDisplay();
		bool CreateContext(const Version& version, EGLContext share_context);
		bool CreateContextAndSurface(const Version& version, EGLContext share_context, bool make_current);
		bool CreateSurface();
		bool CreatePBufferSurface();
		bool CheckConfigSurfaceFormat(EGLConfig config) const;
		void DestroyContext();
		void DestroySurface();

		EGLDisplay m_display = EGL_NO_DISPLAY;
		EGLSurface m_surface = EGL_NO_SURFACE;
		EGLContext m_context = EGL_NO_CONTEXT;

		EGLConfig m_config = {};

		bool m_supports_surfaceless = false;
	};

} // namespace GL
