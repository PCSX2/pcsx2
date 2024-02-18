// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContextEGL.h"

#include <wayland-egl.h>

class GLContextEGLWayland final : public GLContextEGL
{
public:
	GLContextEGLWayland(const WindowInfo& wi);
	~GLContextEGLWayland() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try, Error* error);

	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;

protected:
	EGLDisplay GetPlatformDisplay(Error* error) override;
	EGLSurface CreatePlatformSurface(EGLConfig config, void* win, Error* error) override;

private:
	bool LoadModule(Error* error);

	wl_egl_window* m_wl_window = nullptr;

	void* m_wl_module = nullptr;
	wl_egl_window* (*m_wl_egl_window_create)(struct wl_surface* surface, int width, int height);
	void (*m_wl_egl_window_destroy)(struct wl_egl_window* egl_window);
	void (*m_wl_egl_window_resize)(struct wl_egl_window* egl_window, int width, int height, int dx, int dy);
};
