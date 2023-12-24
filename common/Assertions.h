// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
