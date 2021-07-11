/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
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
