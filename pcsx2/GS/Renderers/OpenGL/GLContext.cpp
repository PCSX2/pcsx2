// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLContext.h"

#if defined(_WIN32)
#include "GS/Renderers/OpenGL/GLContextWGL.h"
#elif defined(__APPLE__)
#include "GS/Renderers/OpenGL/GLContextAGL.h"
#else // Linux
#ifdef X11_API
#include "GS/Renderers/OpenGL/GLContextEGLX11.h"
#endif
#ifdef WAYLAND_API
#include "GS/Renderers/OpenGL/GLContextEGLWayland.h"
#endif
#endif

#include "common/Console.h"

#include "glad.h"

GLContext::GLContext(const WindowInfo& wi)
	: m_wi(wi)
{
}

GLContext::~GLContext() = default;

std::unique_ptr<GLContext> GLContext::Create(const WindowInfo& wi)
{
	// We need at least GL3.3.
	static constexpr Version vlist[] = {
		{4, 6},
		{4, 5},
		{4, 4},
		{4, 3},
		{4, 2},
		{4, 1},
		{4, 0},
		{3, 3},
	};

	std::unique_ptr<GLContext> context;
#if defined(_WIN32)
	context = GLContextWGL::Create(wi, vlist);
#elif defined(__APPLE__)
	context = GLContextAGL::Create(wi, vlist);
#else // Linux
#if defined(X11_API)
	if (wi.type == WindowInfo::Type::X11)
		context = GLContextEGLX11::Create(wi, vlist);
#endif

#if defined(WAYLAND_API)
	if (wi.type == WindowInfo::Type::Wayland)
		context = GLContextEGLWayland::Create(wi, vlist);
#endif
#endif

	if (!context)
		return nullptr;

	// NOTE: Not thread-safe. But this is okay, since we're not going to be creating more than one context at a time.
	static GLContext* context_being_created;
	context_being_created = context.get();

	// load up glad
	if (!gladLoadGLLoader([](const char* name) { return context_being_created->GetProcAddress(name); }))
	{
		Console.Error("Failed to load GL functions for GLAD");
		return nullptr;
	}

	context_being_created = nullptr;

	return context;
}
