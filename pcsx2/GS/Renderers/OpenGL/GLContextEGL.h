// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContext.h"

#include "glad_egl.h"

#include <span>

class GLContextEGL : public GLContext
{
public:
	GLContextEGL(const WindowInfo& wi);
	~GLContextEGL() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try);

	void* GetProcAddress(const char* name) override;
	virtual bool ChangeSurface(const WindowInfo& new_wi) override;
	virtual void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
	bool SwapBuffers() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	bool SetSwapInterval(s32 interval) override;
	virtual std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;

protected:
	virtual bool SetDisplay();
	virtual EGLNativeWindowType GetNativeWindow(EGLConfig config);

	bool Initialize(std::span<const Version> versions_to_try);
	bool CreateDisplay();
	bool CreateContext(const Version& version, EGLContext share_context);
	bool CreateContextAndSurface(const Version& version, EGLContext share_context, bool make_current);
	bool CreateSurface();
	bool CreatePBufferSurface();
	bool CheckConfigSurfaceFormat(EGLConfig config) const;
	void DestroyContext();
	void DestroySurface();

	EGLDisplay m_display = EGL_NO_DISPLAY;
	EGLSurface m_surface = EGL_NO_SURFACE;
	EGLContext m_context = EGL_NO_CONTEXT;

	EGLConfig m_config = {};

	bool m_supports_surfaceless = false;
};
