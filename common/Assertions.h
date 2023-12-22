/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"

#ifndef __pxFUNCTION__
#if defined(__GNUG__)
#define __pxFUNCTION__ __PRETTY_FUNCTION__
#else
#define __pxFUNCTION__ __FUNCTION__
#endif
#endif

// pxAssertRel - assertion check even in Release builds.
// pxFailRel - aborts program even in Release builds.
// 
// pxAssert[Msg] - assertion check only in Debug/Devel builds, noop in Release.
// pxAssume[Msg] - assertion check in Debug/Devel builds, optimization hint in Release builds.
// pxFail - aborts program only in Debug/Devel builds, noop in Release.

extern void pxOnAssertFail(const char* file, int line, const char* func, const char* msg);

#define pxAssertRel(cond, msg) do { if (!(cond)) [[unlikely]] { pxOnAssertFail(__FILE__, __LINE__, __pxFUNCTION__, msg); } } while(0)
#define pxFailRel(msg) pxOnAssertFail(__FILE__, __LINE__, __pxFUNCTION__, msg)

#if defined(PCSX2_DEBUG) || defined(PCSX2_DEVBUILD)

#define pxAssertMsg(cond, msg) pxAssertRel(cond, msg)
#define pxAssumeMsg(cond, msg) pxAssertRel(cond, msg)
#define pxFail(msg) pxFailRel(msg)

#else

#define pxAssertMsg(cond, msg) ((void)0)
#define pxAssumeMsg(cond, msg) ASSUME(cond)
#define pxFail(msg) ((void)0)

#endif

#define pxAssert(cond) pxAssertMsg(cond, #cond)
#define pxAssume(cond) pxAssumeMsg(cond, #cond)

// jNO_DEFAULT -- disables the default case in a switch, which improves switch optimization.
#define jNO_DEFAULT \
	default: \
	{ \
		pxAssumeMsg(false, "Incorrect usage of jNO_DEFAULT detected (default case is not unreachable!)"); \
		break; \
	}
