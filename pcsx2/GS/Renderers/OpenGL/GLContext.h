// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
