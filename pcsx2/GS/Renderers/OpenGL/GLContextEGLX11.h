// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContextEGL.h"

class GLContextEGLX11 final : public GLContextEGL
{
public:
	GLContextEGLX11(const WindowInfo& wi);
	~GLContextEGLX11() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try);

	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;

protected:
	EGLNativeWindowType GetNativeWindow(EGLConfig config) override;
};
