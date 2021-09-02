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

#pragma once

#include "Renderers/OpenGL/GLLoader.h"
#include "GSExtra.h"

// Note: GL messages are present in common code, so in all renderers.

#define GL_INSERT(type, code, sev, ...) \
	do \
		if (glDebugMessageInsert) glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, type, code, sev, -1, format(__VA_ARGS__).c_str()); \
	while(0);

#if defined(_DEBUG)
	#define GL_CACHE(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xFEAD, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
	#define GL_CACHE(...) (void)(0);
#endif

#if defined(ENABLE_TRACE_REG) && defined(_DEBUG)
	#define GL_REG(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xB0B0, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
	#define GL_REG(...) (void)(0);
#endif

#if defined(ENABLE_EXTRA_LOG) && defined(_DEBUG)
	#define GL_DBG(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xD0D0, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
	#define GL_DBG(...) (void)(0);
#endif

#if defined(ENABLE_OGL_DEBUG)
	struct GLAutoPop
	{
		~GLAutoPop()
		{
			if (glPopDebugGroup)
				glPopDebugGroup();
		}
	};

	#define GL_PUSH_(...) do if (glPushDebugGroup) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0xBAD, -1, format(__VA_ARGS__).c_str()); while(0);
	#define GL_PUSH(...)  do if (glPushDebugGroup) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0xBAD, -1, format(__VA_ARGS__).c_str()); while(0); GLAutoPop gl_auto_pop;
	#define GL_POP()      do if (glPopDebugGroup) glPopDebugGroup(); while(0);
	#define GL_INS(...)   GL_INSERT(GL_DEBUG_TYPE_ERROR, 0xDEAD, GL_DEBUG_SEVERITY_MEDIUM, __VA_ARGS__)
	#define GL_PERF(...)  GL_INSERT(GL_DEBUG_TYPE_PERFORMANCE, 0xFEE1, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
	#define GL_PUSH_(...) (void)(0);
	#define GL_PUSH(...) (void)(0);
	#define GL_POP()     (void)(0);
	#define GL_INS(...)  (void)(0);
	#define GL_PERF(...) (void)(0);
#endif
