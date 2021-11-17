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

#include "common/GL/ContextEGLX11.h"

#include <X11/Xlib.h>

namespace GL
{
	ContextEGLX11::ContextEGLX11(const WindowInfo& wi)
		: ContextEGL(wi)
	{
	}
	ContextEGLX11::~ContextEGLX11() = default;

	std::unique_ptr<Context> ContextEGLX11::Create(const WindowInfo& wi, const Version* versions_to_try,
		size_t num_versions_to_try)
	{
		std::unique_ptr<ContextEGLX11> context = std::make_unique<ContextEGLX11>(wi);
		if (!context->Initialize(versions_to_try, num_versions_to_try))
			return nullptr;

		return context;
	}

	std::unique_ptr<Context> ContextEGLX11::CreateSharedContext(const WindowInfo& wi)
	{
		std::unique_ptr<ContextEGLX11> context = std::make_unique<ContextEGLX11>(wi);
		context->m_display = m_display;

		if (!context->CreateContextAndSurface(m_version, m_context, false))
			return nullptr;

		return context;
	}

	void ContextEGLX11::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
	{
		ContextEGL::ResizeSurface(new_surface_width, new_surface_height);
	}

	EGLNativeWindowType ContextEGLX11::GetNativeWindow(EGLConfig config)
	{
		return (EGLNativeWindowType)reinterpret_cast<Window>(m_wi.window_handle);
	}
} // namespace GL
