// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/OpenGL/GLContext.h"
#ifdef __ANDROID__
#include "GS/Renderers/OpenGL/GLContextEGLAndroid.h"
#endif

#if defined(_WIN32)
#include "GS/Renderers/OpenGL/GLContextWGL.h"
#else // Linux
#ifdef X11_API
#include "GS/Renderers/OpenGL/GLContextEGLX11.h"
#endif
#ifdef WAYLAND_API
#include "GS/Renderers/OpenGL/GLContextEGLWayland.h"
#endif
#endif

#include "common/Console.h"
#include "common/Error.h"

#include "glad/gl.h"

static bool ShouldPreferESContext()
{
	const char* value = std::getenv("PREFER_GLES_CONTEXT");
	return (value && std::strcmp(value, "1") == 0);
}

GLContext::GLContext(const WindowInfo& wi)
	: m_wi(wi)
{
}

GLContext::~GLContext() = default;

std::unique_ptr<GLContext> GLContext::Create(const WindowInfo& wi, Error* error)
{
	static constexpr Version vlist[] = {
		{Profile::Core, 4, 6},
		{Profile::Core, 4, 5},
		{Profile::Core, 4, 4},
		{Profile::Core, 4, 3},
		{Profile::Core, 4, 2},
		{Profile::Core, 4, 1},
		{Profile::Core, 4, 0},
		{Profile::Core, 3, 3},
		{Profile::ES, 3, 2},
		{Profile::ES, 3, 1},
		{Profile::ES, 3, 0},
		{Profile::ES, 2, 0},
	};
	static constexpr size_t num_versions = std::size(vlist);

	const Version* versions_to_try = vlist;
	size_t num_versions_to_try = num_versions;

	Version reordered[num_versions];
	if (ShouldPreferESContext())
	{
		size_t count = 0;
		for (size_t i = 0; i < num_versions_to_try; i++)
		{
			if (versions_to_try[i].profile == Profile::ES)
				reordered[count++] = versions_to_try[i];
		}
		for (size_t i = 0; i < num_versions_to_try; i++)
		{
			if (versions_to_try[i].profile != Profile::ES)
				reordered[count++] = versions_to_try[i];
		}
		versions_to_try = reordered;
	}

	std::unique_ptr<GLContext> context;
#ifdef __ANDROID__
	if (wi.type == WindowInfo::Type::Android)
		context = GLContextEGLAndroid::Create(wi, versions_to_try, num_versions_to_try);
#endif
#if defined(_WIN32)
	context = GLContextWGL::Create(wi, std::span<const Version>(versions_to_try, num_versions_to_try), error);
#else
#ifdef X11_API
	if (wi.type == WindowInfo::Type::X11)
		context = GLContextEGLX11::Create(wi, std::span<const Version>(versions_to_try, num_versions_to_try), error);
#endif
#ifdef WAYLAND_API
	if (!context && wi.type == WindowInfo::Type::Wayland)
		context = GLContextEGLWayland::Create(wi, std::span<const Version>(versions_to_try, num_versions_to_try), error);
#endif
#endif

	if (!context)
		return nullptr;

	// NOTE: Not thread-safe. But this is okay, since we're not going to be creating more than one context at a time.
	static GLContext* context_being_created;
	context_being_created = context.get();

	if (!context->IsGLES())
	{
		if (!gladLoadGL([](const char* name) { return reinterpret_cast<GLADapiproc>(context_being_created->GetProcAddress(name)); }))
		{
			Error::SetStringView(error, "Failed to load GL functions for GLAD");
			return nullptr;
		}
	}
	else
	{
		if (!gladLoadGLES2([](const char* name) { return reinterpret_cast<GLADapiproc>(context_being_created->GetProcAddress(name)); }))
		{
			Error::SetStringView(error, "Failed to load GLES functions for GLAD");
			return nullptr;
		}
	}

	context_being_created = nullptr;

	return context;
}
