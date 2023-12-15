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

#include "common/Pcsx2Defs.h"
#include "common/WindowInfo.h"

#include <array>
#include <memory>
#include <vector>

class GLContext
{
public:
	GLContext(const WindowInfo& wi);
	virtual ~GLContext();

	struct Version
	{
		int major_version;
		int minor_version;
	};

	__fi const WindowInfo& GetWindowInfo() const { return m_wi; }
	__fi u32 GetSurfaceWidth() const { return m_wi.surface_width; }
	__fi u32 GetSurfaceHeight() const { return m_wi.surface_height; }

	virtual void* GetProcAddress(const char* name) = 0;
	virtual bool ChangeSurface(const WindowInfo& new_wi) = 0;
	virtual void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) = 0;
	virtual bool SwapBuffers() = 0;
	virtual bool MakeCurrent() = 0;
	virtual bool DoneCurrent() = 0;
	virtual bool SetSwapInterval(s32 interval) = 0;
	virtual std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) = 0;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi);

protected:
	WindowInfo m_wi;
	Version m_version = {};
};
