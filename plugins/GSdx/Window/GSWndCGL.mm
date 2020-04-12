/*
 *	Copyright (C) 2020 TellowKrinkle
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef __APPLE__

#if ! __has_feature(objc_arc)
#error "Compile this with -fobjc-arc"
#endif

#define GL_SILENCE_DEPRECATION

#include "GSWndCGL.h"
#include "GSWndCGLShim.h"
#include <dlfcn.h>
#include <dispatch/dispatch.h>

std::shared_ptr<GSWndGL> makeGSWndCGL() {
	return std::make_shared<GSWndCGL>();
}

GSWndCGL::GSWndCGL() : m_NativeWindow(nil), m_view(nil), m_context(nil) {}
GSWndCGL::~GSWndCGL() {
	if (m_context) CGLReleaseContext(m_context);
}

void GSWndCGL::CreateContext(int major, int minor) {
	if (!m_NativeWindow) throw GSDXRecoverableError();

	NSOpenGLPixelFormatAttribute profileVer = NSOpenGLProfileVersion3_2Core;
	if (major > 3 || (major == 3 && minor > 2)) {
		profileVer = NSOpenGLProfileVersion4_1Core;
	}

	const NSOpenGLPixelFormatAttribute attrs[] = {
		NSOpenGLPFAOpenGLProfile, profileVer,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFADepthSize, 0,
		0
	};

	dispatch_sync(dispatch_get_main_queue(), [&]{
		NSOpenGLPixelFormat *pxformat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

		m_view = [[NSOpenGLView alloc] initWithFrame:[m_NativeWindow contentRectForFrameRect:[m_NativeWindow frame]] pixelFormat:pxformat];
		[m_NativeWindow setContentView:m_view];
		[m_view setWantsBestResolutionOpenGLSurface:YES];
		m_context = CGLRetainContext([[m_view openGLContext] CGLContextObj]);
	});
}

void GSWndCGL::AttachContext() {
	if (IsContextAttached()) return;
	CGLSetCurrentContext(m_context);
	m_ctx_attached = true;
}
void GSWndCGL::DetachContext() {
	if (!IsContextAttached()) return;
	CGLSetCurrentContext(nullptr);
	m_ctx_attached = false;
}
void GSWndCGL::PopulateWndGlFunction() {}

bool GSWndCGL::Attach(void* handle, bool managed) {
	dispatch_sync(dispatch_get_main_queue(), [&]{
		m_NativeWindow = [(__bridge NSView*)handle window];
	});
	m_managed = managed;

	FullContextInit();

	return true;
}

void GSWndCGL::Detach() {
	DetachContext();
	if (m_context) {
		CGLReleaseContext(m_context);
		m_context = nil;
	}
	m_view = nil;
}

bool GSWndCGL::Create(const std::string& title, int w, int h) {
	if (m_NativeWindow) throw GSDXRecoverableError();

	if (w <= 0 || h <= 0) {
		w = theApp.GetConfigI("ModeWidth");
		h = theApp.GetConfigI("ModeHeight");
	}

	m_managed = true;

	dispatch_sync(dispatch_get_main_queue(), [&]{
		NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
		m_NativeWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
		                                             styleMask:style
		                                               backing:NSBackingStoreBuffered
		                                                 defer:NO];
	});

	FullContextInit();

	return true;
}

void* GSWndCGL::GetProcAddress(const char* name, bool opt) {
	void *ptr = dlsym(RTLD_NEXT, name);
	if (!ptr) {
		if (theApp.GetConfigB("debug_opengl")) {
			fprintf(stderr, "Failed to find %s\n", name);
		}

		if (!opt) throw GSDXRecoverableError();
	}
	return ptr;
}

void* GSWndCGL::GetDisplay() {
	return (__bridge void*)[m_NativeWindow screen];
}

GSVector4i GSWndCGL::GetClientRect() {
	NSRect rect;
	dispatch_sync(dispatch_get_main_queue(), [&]{
		rect = [m_view frame];
	});
	return GSVector4i(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

bool GSWndCGL::SetWindowText(const char* title) {
	if (!m_managed) return true;
	[m_NativeWindow setTitle:[[NSString alloc] initWithCString:title encoding:NSUTF8StringEncoding]];
	return true;
}

void GSWndCGL::SetSwapInterval() {
	GLint vsync = m_vsync.load();
	CGLSetParameter(m_context, kCGLCPSwapInterval, &vsync);
}

void GSWndCGL::Flip() {
	if (m_vsync_change_requested.exchange(false)) SetSwapInterval();

	CGLFlushDrawable(m_context);
}

void GSWndCGL::Show() {
	dispatch_sync(dispatch_get_main_queue(), [&]{
		[m_NativeWindow makeKeyAndOrderFront:nil];
	});
}

void GSWndCGL::Hide() {
	dispatch_sync(dispatch_get_main_queue(), [&]{
		[m_NativeWindow orderOut:nil];
	});
}

void GSWndCGL::HideFrame() {
	dispatch_sync(dispatch_get_main_queue(), [&]{
		[m_NativeWindow setStyleMask:[m_NativeWindow styleMask] & ~NSWindowStyleMaskTitled];
	});
}

#endif // __APPLE__
