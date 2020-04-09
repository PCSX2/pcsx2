/*
 *	Copyright (C) 2020 TellowKrinkle
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSWnd.h"

#ifndef __OBJC__
#error "This header is for use with Objective-C++ only.  You probably wanted GSWndCGLShim.h"
#endif

#ifdef __APPLE__

#include <AppKit/AppKit.h>

class GSWndCGL final : public GSWndGL
{
	NSWindow *m_NativeWindow;
	NSOpenGLView *m_view;
	CGLContextObj m_context;

	virtual void PopulateWndGlFunction() override;
	virtual void CreateContext(int major, int minor) override;

	virtual void SetSwapInterval() override;
	virtual bool HasLateVsyncSupport() override { return false; };
public:
	GSWndCGL();
	virtual ~GSWndCGL() override;

	virtual bool Create(const std::string& title, int w, int h) override;
	virtual bool Attach(void* handle, bool managed = true) override;
	virtual void Detach() override;
	virtual void* GetDisplay() override;
	virtual void* GetHandle() override { return (__bridge void*)(m_NativeWindow); };
	virtual GSVector4i GetClientRect() override;
	virtual bool SetWindowText(const char* title) override;

	virtual void AttachContext() override;
	virtual void DetachContext() override;
	virtual void* GetProcAddress(const char* name, bool opt = false) override;

	virtual void Show() override;
	virtual void Hide() override;
	virtual void HideFrame() override;
	virtual void Flip() override;
};

#endif
