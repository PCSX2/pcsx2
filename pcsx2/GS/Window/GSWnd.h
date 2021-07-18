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

#pragma once

#include "PrecompiledHeader.h"
#include "Utilities/WindowInfo.h"
#include "GS/GS.h"
#include "GS/GSVector.h"

class GSWnd
{
public:
	GSWnd() = default;
	virtual ~GSWnd() = default;

	virtual bool Attach(const WindowInfo& wi) = 0;
	virtual void Detach() = 0;

	virtual void* GetHandle() = 0;
	virtual GSVector4i GetClientRect() = 0;

	virtual void AttachContext() {}
	virtual void DetachContext() {}

	virtual void Flip() {}
	virtual void SetVSync(int vsync) {}
};

class GSWndGL : public GSWnd
{
protected:
	bool m_ctx_attached;
	std::atomic<bool> m_vsync_change_requested;
	std::atomic<int> m_vsync;

	bool IsContextAttached() const { return m_ctx_attached; }
	void PopulateGlFunction();
	virtual void PopulateWndGlFunction() = 0;
	void FullContextInit();
	virtual void CreateContext(int major, int minor) = 0;

	virtual void SetSwapInterval() = 0;
	virtual bool HasLateVsyncSupport() = 0;

public:
	GSWndGL()
		: m_ctx_attached(false)
		, m_vsync_change_requested(false)
		, m_vsync(0)
	{
	}
	virtual ~GSWndGL() override = default;

	virtual void* GetProcAddress(const char* name, bool opt = false) = 0;

	void SetVSync(int vsync) final;
};
