// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGLX11.h"

#include <X11/Xlib.h>

GLContextEGLX11::GLContextEGLX11(const WindowInfo& wi)
	: GLContextEGL(wi)
{
}
GLContextEGLX11::~GLContextEGLX11() = default;

std::unique_ptr<GLContext> GLContextEGLX11::Create(const WindowInfo& wi, std::span<const Version> versions_to_try)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	if (!context->Initialize(versions_to_try))
		return nullptr;

	return context;
}

std::unique_ptr<GLContext> GLContextEGLX11::CreateSharedContext(const WindowInfo& wi)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	context->m_display = m_display;

	if (!context->CreateContextAndSurface(m_version, m_context, false))
		return nullptr;

	return context;
}

void GLContextEGLX11::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
{
	GLContextEGL::ResizeSurface(new_surface_width, new_surface_height);
}

EGLNativeWindowType GLContextEGLX11::GetNativeWindow(EGLConfig config)
{
	return (EGLNativeWindowType) reinterpret_cast<Window>(m_wi.window_handle);
}
