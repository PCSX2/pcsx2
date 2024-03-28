// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGLX11.h"

#include "common/Console.h"
#include "common/Error.h"

#include <X11/Xlib.h>

GLContextEGLX11::GLContextEGLX11(const WindowInfo& wi)
	: GLContextEGL(wi)
{
}
GLContextEGLX11::~GLContextEGLX11() = default;

std::unique_ptr<GLContext> GLContextEGLX11::Create(const WindowInfo& wi, std::span<const Version> versions_to_try,
	Error* error)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	if (!context->Initialize(versions_to_try, error))
		return nullptr;

	Console.WriteLn("EGL Platform: X11");
	return context;
}

std::unique_ptr<GLContext> GLContextEGLX11::CreateSharedContext(const WindowInfo& wi)
{
	std::unique_ptr<GLContextEGLX11> context = std::make_unique<GLContextEGLX11>(wi);
	context->m_display = m_display;
	context->m_supports_surfaceless = m_supports_surfaceless;

	if (!context->CreateContextAndSurface(m_version, m_context, false))
		return nullptr;

	return context;
}

EGLDisplay GLContextEGLX11::GetPlatformDisplay(const EGLAttrib* attribs, Error* error)
{
	if (!CheckExtension("EGL_KHR_platform_x11", "EGL_EXT_platform_x11", error))
		return nullptr;

	EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, m_wi.display_connection, attribs);
	if (dpy == EGL_NO_DISPLAY)
	{
		const EGLint err = eglGetError();
		Error::SetStringFmt(error, "eglGetPlatformDisplay() for X11 failed: {} (0x{:X})", err, err);
		return nullptr;
	}

	return dpy;
}

EGLSurface GLContextEGLX11::CreatePlatformSurface(EGLConfig config, const EGLAttrib* attribs, Error* error)
{
	EGLSurface surface = eglCreatePlatformWindowSurface(m_display, config, &m_wi.window_handle, attribs);
	if (surface == EGL_NO_SURFACE)
	{
		const EGLint err = eglGetError();
		Error::SetStringFmt(error, "eglCreatePlatformWindowSurface() for X11 failed: {} (0x{:X})", err, err);
		return nullptr;
	}

	return surface;
}
