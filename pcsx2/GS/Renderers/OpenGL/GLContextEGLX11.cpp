// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGLX11.h"

#include "common/Console.h"
#include "common/Error.h"

GLContextEGLX11::GLContextEGLX11(const WindowInfo& wi)
	: GLContextEGL(wi)
{
}

GLContextEGLX11::~GLContextEGLX11() = default;

std::unique_ptr<GLContext> GLContextEGLX11::Create(const WindowInfo& wi,
	std::span<const Version> versions_to_try, Error* error)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	return context;
}

std::unique_ptr<GLContext> GLContextEGLX11::CreateSharedContext(const WindowInfo& wi, Error* error)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	context->m_display = m_display;

	if (!context->CreateContextAndSurface(m_version, m_context, false))
		return nullptr;

	return context;
}

EGLDisplay GLContextEGLX11::GetPlatformDisplay(Error* error)
{
	EGLDisplay dpy = TryGetPlatformDisplay(EGL_PLATFORM_X11_KHR, "EGL_EXT_platform_x11");
	if (dpy == EGL_NO_DISPLAY)
		dpy = GetFallbackDisplay(error);

	return dpy;
}

EGLSurface GLContextEGLX11::CreatePlatformSurface(EGLConfig config, void* win, Error* error)
{
	// This is hideous.. the EXT version requires a pointer to the window, whereas the base
	// version requires the window itself, casted to void*...
	EGLSurface surface = TryCreatePlatformSurface(config, &win, error);
	if (surface == EGL_NO_SURFACE)
		surface = CreateFallbackSurface(config, win, error);

	return surface;
}