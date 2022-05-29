/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/PrecompiledHeader.h"

#include "common/Console.h"
#include "common/GL/Context.h"
#include "glad.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#if defined(_WIN32) && !defined(_M_ARM64)
#include "common/GL/ContextWGL.h"
#elif defined(__APPLE__)
#include "common/GL/ContextAGL.h"
#else
#ifdef X11_API
#include "common/GL/ContextEGLX11.h"
#endif
#ifdef WAYLAND_API
#include "common/GL/ContextEGLWayland.h"
#endif
#endif

namespace GL
{
	static bool ShouldPreferESContext()
	{
#ifndef _MSC_VER
		const char* value = std::getenv("PREFER_GLES_CONTEXT");
		return (value && std::strcmp(value, "1") == 0);
#else
		char buffer[2] = {};
		size_t buffer_size = sizeof(buffer);
		getenv_s(&buffer_size, buffer, "PREFER_GLES_CONTEXT");
		return (std::strcmp(buffer, "1") == 0);
#endif
	}

	static void DisableBrokenExtensions(const char* gl_vendor, const char* gl_renderer)
	{
		if (std::strstr(gl_vendor, "ARM"))
		{
			// GL_{EXT,OES}_copy_image seem to be implemented on the CPU in the Mali drivers...
			Console.Warning("Mali driver detected, disabling GL_{EXT,OES}_copy_image");
			GLAD_GL_EXT_copy_image = 0;
			GLAD_GL_OES_copy_image = 0;
		}
	}

	Context::Context(const WindowInfo& wi)
		: m_wi(wi)
	{
	}

	Context::~Context() = default;

	std::vector<Context::FullscreenModeInfo> Context::EnumerateFullscreenModes()
	{
		return {};
	}

	std::unique_ptr<GL::Context> Context::Create(const WindowInfo& wi, const Version* versions_to_try,
		size_t num_versions_to_try)
	{
		if (ShouldPreferESContext())
		{
			// move ES versions to the front
			Version* new_versions_to_try = static_cast<Version*>(alloca(sizeof(Version) * num_versions_to_try));
			size_t count = 0;
			for (size_t i = 0; i < num_versions_to_try; i++)
			{
				if (versions_to_try[i].profile == Profile::ES)
					new_versions_to_try[count++] = versions_to_try[i];
			}
			for (size_t i = 0; i < num_versions_to_try; i++)
			{
				if (versions_to_try[i].profile != Profile::ES)
					new_versions_to_try[count++] = versions_to_try[i];
			}
			versions_to_try = new_versions_to_try;
		}

		std::unique_ptr<Context> context;
#if defined(_WIN32) && !defined(_M_ARM64)
		context = ContextWGL::Create(wi, versions_to_try, num_versions_to_try);
#elif defined(__APPLE__)
		context = ContextAGL::Create(wi, versions_to_try, num_versions_to_try);
#endif

#if defined(X11_API)
		if (wi.type == WindowInfo::Type::X11)
			context = ContextEGLX11::Create(wi, versions_to_try, num_versions_to_try);
#endif

#if defined(WAYLAND_API)
		if (wi.type == WindowInfo::Type::Wayland)
			context = ContextEGLWayland::Create(wi, versions_to_try, num_versions_to_try);
#endif

		if (!context)
			return nullptr;

		Console.WriteLn("Created a %s context", context->IsGLES() ? "OpenGL ES" : "OpenGL");

		// NOTE: Not thread-safe. But this is okay, since we're not going to be creating more than one context at a time.
		static Context* context_being_created;
		context_being_created = context.get();

		// load up glad
		if (!context->IsGLES())
		{
			if (!gladLoadGLLoader([](const char* name) { return context_being_created->GetProcAddress(name); }))
			{
				Console.Error("Failed to load GL functions for GLAD");
				return nullptr;
			}
		}
		else
		{
			if (!gladLoadGLES2Loader([](const char* name) { return context_being_created->GetProcAddress(name); }))
			{
				Console.Error("Failed to load GLES functions for GLAD");
				return nullptr;
			}
		}

		context_being_created = nullptr;

		const char* gl_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		const char* gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		const char* gl_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const char* gl_shading_language_version = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
		DevCon.WriteLn(Color_Magenta, "GL_VENDOR: %s", gl_vendor);
		DevCon.WriteLn(Color_Magenta, "GL_RENDERER: %s", gl_renderer);
		DevCon.WriteLn(Color_Magenta, "GL_VERSION: %s", gl_version);
		DevCon.WriteLn(Color_Magenta, "GL_SHADING_LANGUAGE_VERSION: %s", gl_shading_language_version);

		DisableBrokenExtensions(gl_vendor, gl_renderer);

		return context;
	}

	const std::array<Context::Version, 16>& Context::GetAllVersionsList()
	{
		static constexpr std::array<Version, 16> vlist = {{{Profile::Core, 4, 6},
			{Profile::Core, 4, 5},
			{Profile::Core, 4, 4},
			{Profile::Core, 4, 3},
			{Profile::Core, 4, 2},
			{Profile::Core, 4, 1},
			{Profile::Core, 4, 0},
			{Profile::Core, 3, 3},
			{Profile::Core, 3, 2},
			{Profile::Core, 3, 1},
			{Profile::Core, 3, 0},
			{Profile::ES, 3, 2},
			{Profile::ES, 3, 1},
			{Profile::ES, 3, 0},
			{Profile::ES, 2, 0},
			{Profile::NoProfile, 0, 0}}};
		return vlist;
	}
} // namespace GL
