// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContext.h"

#include "common/RedtapeWindows.h"

#include "glad/wgl.h"

#include <optional>
#include <span>

class GLContextWGL final : public GLContext
{
public:
	GLContextWGL(const WindowInfo& wi);
	~GLContextWGL() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try, Error* error);

	void* GetProcAddress(const char* name) override;
	bool ChangeSurface(const WindowInfo& new_wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
	bool SwapBuffers() override;
	bool IsCurrent() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	bool SupportsNegativeSwapInterval() const override;
	bool SetSwapInterval(s32 interval) override;
	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override;

private:
	__fi HWND GetHWND() const { return static_cast<HWND>(m_wi.window_handle); }

	HDC GetDCAndSetPixelFormat(HWND hwnd, Error* error);

	bool Initialize(std::span<const Version> versions_to_try, Error* error);
	bool InitializeDC(Error* error);
	void ReleaseDC();
	bool CreatePBuffer(Error* error);
	bool CreateAnyContext(HGLRC share_context, bool make_current, Error* error);
	bool CreateVersionContext(const Version& version, HGLRC share_context, bool make_current, Error* error);

	HDC m_dc = {};
	HGLRC m_rc = {};

	// Can't change pixel format once it's set for a RC.
	std::optional<int> m_pixel_format;

	// Dummy window for creating a PBuffer off when we're surfaceless.
	HWND m_dummy_window = {};
	HDC m_dummy_dc = {};
	HPBUFFERARB m_pbuffer = {};
};
