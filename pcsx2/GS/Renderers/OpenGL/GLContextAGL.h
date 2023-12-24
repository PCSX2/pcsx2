// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/OpenGL/GLContext.h"

#include "glad.h"

#include <span>

#if defined(__APPLE__) && defined(__OBJC__)
#import <AppKit/AppKit.h>
#else
struct NSView;
struct NSOpenGLContext;
struct NSOpenGLPixelFormat;
#endif

class GLContextAGL final : public GLContext
{
public:
	GLContextAGL(const WindowInfo& wi);
	~GLContextAGL() override;

	static std::unique_ptr<GLContext> Create(const WindowInfo& wi, std::span<const Version> versions_to_try);

	void* GetProcAddress(const char* name) override;
	bool ChangeSurface(const WindowInfo& new_wi) override;
	void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;
	bool SwapBuffers() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	bool SetSwapInterval(s32 interval) override;
	std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi) override;

private:
	bool Initialize(std::span<const Version> versions_to_try);
	bool CreateContext(NSOpenGLContext* share_context, int profile, bool make_current);
	void BindContextToView();
	void CleanupView();

	// returns true if dimensions have changed
	bool UpdateDimensions();

	NSView* m_view = nullptr;
	NSOpenGLContext* m_context = nullptr;
	NSOpenGLPixelFormat* m_pixel_format = nullptr;
	void* m_opengl_module_handle = nullptr;
};
