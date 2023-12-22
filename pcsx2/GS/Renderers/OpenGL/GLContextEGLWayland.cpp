// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContextEGLWayland.h"

#include "common/Console.h"

#include <dlfcn.h>

static const char* WAYLAND_EGL_MODNAME = "libwayland-egl.so.1";

GLContextEGLWayland::GLContextEGLWayland(const WindowInfo& wi)
	: GLContextEGL(wi)
{
}

GLContextEGLWayland::~GLContextEGLWayland()
{
	if (m_wl_window)
		m_wl_egl_window_destroy(m_wl_window);
	if (m_wl_module)
		dlclose(m_wl_module);
}

std::unique_ptr<GLContext> GLContextEGLWayland::Create(const WindowInfo& wi, std::span<const Version> versions_to_try)
{
	std::unique_ptr<GLContextEGLWayland> context = std::make_unique<GLContextEGLWayland>(wi);
	if (!context->LoadModule() || !context->Initialize(versions_to_try))
		return nullptr;

	return context;
}

std::unique_ptr<GLContext> GLContextEGLWayland::CreateSharedContext(const WindowInfo& wi)
{
	std::unique_ptr<GLContextEGLWayland> context = std::make_unique<GLContextEGLWayland>(wi);
	context->m_display = m_display;

	if (!context->LoadModule() || !context->CreateContextAndSurface(m_version, m_context, false))
		return nullptr;

	return context;
}

void GLContextEGLWayland::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
{
	if (m_wl_window)
		m_wl_egl_window_resize(m_wl_window, new_surface_width, new_surface_height, 0, 0);

	GLContextEGL::ResizeSurface(new_surface_width, new_surface_height);
}

EGLNativeWindowType GLContextEGLWayland::GetNativeWindow(EGLConfig config)
{
	if (m_wl_window)
	{
		m_wl_egl_window_destroy(m_wl_window);
		m_wl_window = nullptr;
	}

	m_wl_window =
		m_wl_egl_window_create(static_cast<wl_surface*>(m_wi.window_handle), m_wi.surface_width, m_wi.surface_height);
	if (!m_wl_window)
		return {};

	return reinterpret_cast<EGLNativeWindowType>(m_wl_window);
}

bool GLContextEGLWayland::LoadModule()
{
	m_wl_module = dlopen(WAYLAND_EGL_MODNAME, RTLD_NOW | RTLD_GLOBAL);
	if (!m_wl_module)
	{
		Console.Error("Failed to load %s.", WAYLAND_EGL_MODNAME);
		return false;
	}

	m_wl_egl_window_create =
		reinterpret_cast<decltype(m_wl_egl_window_create)>(dlsym(m_wl_module, "wl_egl_window_create"));
	m_wl_egl_window_destroy =
		reinterpret_cast<decltype(m_wl_egl_window_destroy)>(dlsym(m_wl_module, "wl_egl_window_destroy"));
	m_wl_egl_window_resize =
		reinterpret_cast<decltype(m_wl_egl_window_resize)>(dlsym(m_wl_module, "wl_egl_window_resize"));
	if (!m_wl_egl_window_create || !m_wl_egl_window_destroy || !m_wl_egl_window_resize)
	{
		Console.Error("Failed to load one or more functions from %s.", WAYLAND_EGL_MODNAME);
		return false;
	}

	return true;
}
