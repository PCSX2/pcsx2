// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContextEGL.h"

class GLContextEGLX11 final : public GLContextEGL
{
public:
	GLContextEGLX11(const WindowInfo& wi);
	~GLContextEGLX11() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try, Error* error);

	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;

protected:
	EGLDisplay GetPlatformDisplay(const EGLAttrib* attribs, Error* error) override;
	EGLSurface CreatePlatformSurface(EGLConfig config, const EGLAttrib* attribs, Error* error) override;
};
