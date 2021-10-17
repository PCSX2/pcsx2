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

#include "common/GL/ContextAGL.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "glad.h"
#include <dlfcn.h>

#if ! __has_feature(objc_arc)
#error "Compile this with -fobjc-arc"
#endif

namespace GL
{
	ContextAGL::ContextAGL(const WindowInfo& wi)
		: Context(wi)
	{
		m_opengl_module_handle = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_NOW);
		if (!m_opengl_module_handle)
			Console.Error("Could not open OpenGL.framework, function lookups will probably fail");
	}

	ContextAGL::~ContextAGL()
	{
		if ([NSOpenGLContext currentContext] == m_context)
			[NSOpenGLContext clearCurrentContext];

		CleanupView();

		if (m_opengl_module_handle)
			dlclose(m_opengl_module_handle);
	}

	std::unique_ptr<Context> ContextAGL::Create(const WindowInfo& wi, const Version* versions_to_try,
		size_t num_versions_to_try)
	{
		std::unique_ptr<ContextAGL> context = std::make_unique<ContextAGL>(wi);
		if (!context->Initialize(versions_to_try, num_versions_to_try))
			return nullptr;

		return context;
	}

	bool ContextAGL::Initialize(const Version* versions_to_try, size_t num_versions_to_try)
	{
		for (size_t i = 0; i < num_versions_to_try; i++)
		{
			const Version& cv = versions_to_try[i];
			if (cv.profile == Profile::NoProfile && CreateContext(nullptr, NSOpenGLProfileVersionLegacy, true))
			{
				// we already have the dummy context, so just use that
				m_version = cv;
				return true;
			}
			else if (cv.profile == Profile::Core)
			{
				if (cv.major_version > 4 || cv.minor_version > 1)
					continue;

				const NSOpenGLPixelFormatAttribute profile = (cv.major_version > 3 || cv.minor_version > 2) ? NSOpenGLProfileVersion4_1Core : NSOpenGLProfileVersion3_2Core;
				if (CreateContext(nullptr, static_cast<int>(profile), true))
				{
					m_version = cv;
					return true;
				}
			}
		}

		return false;
	}

	void* ContextAGL::GetProcAddress(const char* name)
	{
		void* addr = m_opengl_module_handle ? dlsym(m_opengl_module_handle, name) : nullptr;
		if (addr)
			return addr;

		return dlsym(RTLD_NEXT, name);
	}

	bool ContextAGL::ChangeSurface(const WindowInfo& new_wi)
	{
		m_wi = new_wi;
		BindContextToView();
		return true;
	}

	void ContextAGL::ResizeSurface(u32 new_surface_width /*= 0*/, u32 new_surface_height /*= 0*/)
	{
		UpdateDimensions();
	}

	bool ContextAGL::UpdateDimensions()
	{
		if (![NSThread isMainThread])
		{
			bool ret;
			dispatch_sync(dispatch_get_main_queue(), [this, &ret]{ ret = UpdateDimensions(); });
			return ret;
		}

		const NSSize window_size = [m_view frame].size;
		const CGFloat window_scale = [[m_view window] backingScaleFactor];
		const u32 new_width = static_cast<u32>(window_size.width * window_scale);
		const u32 new_height = static_cast<u32>(window_size.height * window_scale);

		if (m_wi.surface_width == new_width && m_wi.surface_height == new_height)
			return false;

		m_wi.surface_width = new_width;
		m_wi.surface_height = new_height;

		[m_context update];

		return true;
	}

	bool ContextAGL::SwapBuffers()
	{
		[m_context flushBuffer];
		return true;
	}

	bool ContextAGL::MakeCurrent()
	{
		[m_context makeCurrentContext];
		return true;
	}

	bool ContextAGL::DoneCurrent()
	{
		[NSOpenGLContext clearCurrentContext];
		return true;
	}

	bool ContextAGL::SetSwapInterval(s32 interval)
	{
		GLint gl_interval = static_cast<GLint>(interval);
		[m_context setValues:&gl_interval forParameter:NSOpenGLCPSwapInterval];
		return true;
	}

	std::unique_ptr<Context> ContextAGL::CreateSharedContext(const WindowInfo& wi)
	{
		std::unique_ptr<ContextAGL> context = std::make_unique<ContextAGL>(wi);

		context->m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format shareContext:m_context];
		if (context->m_context == nil)
			return nullptr;

		context->m_version = m_version;
		context->m_pixel_format = m_pixel_format;

		if (wi.type == WindowInfo::Type::MacOS)
			context->BindContextToView();

		return context;
	}

	bool ContextAGL::CreateContext(NSOpenGLContext* share_context, int profile, bool make_current)
	{
		if (m_context)
			m_context = nullptr;

		const NSOpenGLPixelFormatAttribute attribs[] = {
			NSOpenGLPFADoubleBuffer,
			NSOpenGLPFAOpenGLProfile, static_cast<NSOpenGLPixelFormatAttribute>(profile),
			NSOpenGLPFAAccelerated,
			0
		};

		m_pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
		if (m_pixel_format == nil)
		{
			Console.Error("(ContextAGL) Failed to initialize pixel format");
			return false;
		}

		m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixel_format shareContext:nil];
		if (m_context == nil)
			return false;

		if (m_wi.type == WindowInfo::Type::MacOS)
			BindContextToView();

		if (make_current)
			[m_context makeCurrentContext];

		return true;
	}

	void ContextAGL::BindContextToView()
	{
		if (![NSThread isMainThread])
		{
			dispatch_sync(dispatch_get_main_queue(), [this]{ BindContextToView(); });
			return;
		}

#ifdef PCSX2_CORE
		m_view = (__bridge NSView*)m_wi.window_handle;
#else
		// Drawing to wx's wxView somehow causes fighting between us and wx, resulting in massive CPU usage on the main thread and no image
		// Avoid that by adding our own subview
		CleanupView();
		NSView* const superview = (__bridge NSView*)m_wi.window_handle;
		m_view = [[NSView alloc] initWithFrame:[superview frame]];
		[superview addSubview:m_view];
		[m_view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
#endif
		[m_view setWantsBestResolutionOpenGLSurface:YES];

		UpdateDimensions();

		[m_context setView:m_view];
	}

	void ContextAGL::CleanupView()
	{
#ifndef PCSX2_CORE
		if (![NSThread isMainThread])
		{
			dispatch_sync(dispatch_get_main_queue(), [this]{ CleanupView(); });
			return;
		}

		if (m_view)
			[m_view removeFromSuperview];
#endif
		m_view = nullptr;
	}
} // namespace GL
