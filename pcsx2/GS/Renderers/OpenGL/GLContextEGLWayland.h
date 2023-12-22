// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContextEGL.h"

struct wl_egl_window;
struct wl_surface;

class GLContextEGLWayland final : public GLContextEGL
{
public:
	GLContextEGLWayland(const WindowInfo& wi);
	~GLContextEGLWayland() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try);

	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;

protected:
	EGLNativeWindowType GetNativeWindow(EGLConfig config) override;

private:
	bool LoadModule();

	wl_egl_window* m_wl_window = nullptr;

	void* m_wl_module = nullptr;
	wl_egl_window* (*m_wl_egl_window_create)(struct wl_surface* surface, int width, int height);
	void (*m_wl_egl_window_destroy)(struct wl_egl_window* egl_window);
	void (*m_wl_egl_window_resize)(struct wl_egl_window* egl_window, int width, int height, int dx, int dy);
};
