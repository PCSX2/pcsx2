/*
 *	Copyright (C) 2011-2014 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
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

#include "stdafx.h"
#include "GSWnd.h"

void GSWndGL::PopulateGlFunction()
{
	// Load mandatory function pointer
#define GL_EXT_LOAD(ext) *(void**)&(ext) = GetProcAddress(#ext, false)
	// Load extra function pointer
#define GL_EXT_LOAD_OPT(ext) *(void**)&(ext) = GetProcAddress(#ext, true)

#include "PFN_WND.h"

	// GL1.X mess
#if defined(__unix__) || defined(__APPLE__)
	GL_EXT_LOAD(glBlendFuncSeparate);
#endif
	GL_EXT_LOAD_OPT(glTexturePageCommitmentEXT);

	// Check openGL requirement as soon as possible so we can switch to another
	// renderer/device
	GLLoader::check_gl_requirements();
}

void GSWndGL::FullContextInit()
{
	CreateContext(3, 3);
	AttachContext();
	PopulateGlFunction();
	PopulateWndGlFunction();
}

void GSWndGL::SetVSync(int vsync)
{
	if (!HasLateVsyncSupport() && vsync < 0)
		m_vsync = -vsync; // Late vsync not supported, fallback to standard vsync
	else
		m_vsync = vsync;

	// The WGL/GLX/EGL swap interval function must be called on the rendering
	// thread or else the change won't be properly applied.
	m_vsync_change_requested = true;
}
