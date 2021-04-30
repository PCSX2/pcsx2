/*
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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#include "config.h"

#ifdef _WIN32

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <d3dcompiler.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <comutil.h>
#include <atlcomcli.h>

#else

#include <fcntl.h>

#endif

#include <PluginCompatibility.h>

#ifdef __x86_64__
#define _M_AMD64
#endif

// put these into vc9/common7/ide/usertype.dat to have them highlighted

typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned int uint32;
typedef signed int int32;
typedef unsigned long long uint64;
typedef signed long long int64;
#ifdef _M_AMD64
typedef uint64 uptr;
#else
typedef uint32 uptr;
#endif


// xbyak compatibilities
typedef int64 sint64;
#define MIE_INTEGER_TYPE_DEFINED
#define XBYAK_ENABLE_OMITTED_OPERAND

// stdc

#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <ctime>
#include <climits>
#include <cstring>
#include <cassert>

#if __GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 4)
#include <codecvt>
#include <locale>
#endif

#include <complex>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <bitset>

#include <zlib.h>

#include <unordered_map>
#include <unordered_set>

// Don't un-indent our ifdefs
// clang-format off

#ifdef _WIN32

	// Note use GL/glcorearb.h on the future
	// Requirements:
	//	* Update GSWndWGL::GetProcAddress to query 1.0 and 1.1 symbols
	//	* define all ENABLE_GL_VERSION_1_*
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <GL/wglext.h>
	#include "Renderers/OpenGL/GLLoader.h"

	#define DIRECTORY_SEPARATOR '\\'

#else

	// Note use GL/glcorearb.h on the future
	// Requirements:
	//	* Drop GLX that still include gl.h...
	//	  EGL/OGL status on AMD GPU pro driver is unknown
	//	* define all ENABLE_GL_VERSION_1_*
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include "Renderers/OpenGL/GLLoader.h"

	#include <sys/stat.h> // mkdir

	#define DIRECTORY_SEPARATOR '/'

#endif

#ifdef _MSC_VER

	#define EXPORT_C_(type) extern "C" type __stdcall
	#define EXPORT_C EXPORT_C_(void)

	#define ALIGN_STACK(n) alignas(n) int dummy__;

#else

	#ifndef __fastcall
		#define __fastcall __attribute__((fastcall))
	#endif

	#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type
	#define EXPORT_C EXPORT_C_(void)

	#ifdef __GNUC__
		#define __forceinline __inline__ __attribute__((always_inline,unused))
		// #define __forceinline __inline__ __attribute__((__always_inline__, __gnu_inline__))
		#define __assume(c) do { if (!(c)) __builtin_unreachable(); } while(0)

		// GCC removes the variable as dead code and generates some warnings.
		// Stack is automatically realigned due to SSE/AVX operations
		#define ALIGN_STACK(n) (void)0;

	#else

		// TODO Check clang behavior
		#define ALIGN_STACK(n) alignas(n) int dummy__;

	#endif


#endif

#define countof(a) (sizeof(a) / sizeof(a[0]))

#ifndef RESTRICT

	#ifdef __INTEL_COMPILER

		#define RESTRICT restrict

	#elif defined(_MSC_VER)

		#define RESTRICT __restrict

	#elif defined(__GNUC__)

		#define RESTRICT __restrict__

	#else

		#define RESTRICT

	#endif

#endif

#define ASSERT assert

#ifdef _M_AMD64
	// Yeah let use mips naming ;)
	#ifdef _WIN64
	#define a0 rcx
	#define a1 rdx
	#define a2 r8
	#define a3 r9
	#define t0 rdi
	#define t1 rsi
	#else
	#define a0 rdi
	#define a1 rsi
	#define a2 rdx
	#define a3 rcx
	#define t0 r8
	#define t1 r9
	#endif
#endif

// sse
#if defined(__GNUC__)

// Convert gcc see define into GSdx (windows) define
#if defined(__AVX2__)
	#if defined(__x86_64__)
		#define _M_SSE 0x500 // TODO
	#else
		#define _M_SSE 0x501
	#endif
#elif defined(__AVX__)
	#define _M_SSE 0x500
#elif defined(__SSE4_1__)
	#define _M_SSE 0x401
#endif

#endif

#if !defined(_M_SSE) && (!defined(_WIN32) || defined(_M_AMD64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2)

	#define _M_SSE 0x401

#endif

#include <xmmintrin.h>
#include <emmintrin.h>

#ifndef _MM_DENORMALS_ARE_ZERO
#define _MM_DENORMALS_ARE_ZERO 0x0040
#endif

#define MXCSR (_MM_DENORMALS_ARE_ZERO | _MM_MASK_MASK | _MM_ROUND_NEAREST | _MM_FLUSH_ZERO_ON)

#define _MM_TRANSPOSE4_SI128(row0, row1, row2, row3) \
{ \
	__m128 tmp0 = _mm_shuffle_ps(_mm_castsi128_ps(row0), _mm_castsi128_ps(row1), 0x44); \
	__m128 tmp2 = _mm_shuffle_ps(_mm_castsi128_ps(row0), _mm_castsi128_ps(row1), 0xEE); \
	__m128 tmp1 = _mm_shuffle_ps(_mm_castsi128_ps(row2), _mm_castsi128_ps(row3), 0x44); \
	__m128 tmp3 = _mm_shuffle_ps(_mm_castsi128_ps(row2), _mm_castsi128_ps(row3), 0xEE); \
	(row0) = _mm_castps_si128(_mm_shuffle_ps(tmp0, tmp1, 0x88)); \
	(row1) = _mm_castps_si128(_mm_shuffle_ps(tmp0, tmp1, 0xDD)); \
	(row2) = _mm_castps_si128(_mm_shuffle_ps(tmp2, tmp3, 0x88)); \
	(row3) = _mm_castps_si128(_mm_shuffle_ps(tmp2, tmp3, 0xDD)); \
}

#include <tmmintrin.h>
#include <smmintrin.h>

#if _M_SSE >= 0x500

	#include <immintrin.h>

#endif

#undef min
#undef max
#undef abs

#if !defined(_MSC_VER)
	#if defined(__USE_ISOC11) && !defined(ASAN_WORKAROUND) // not supported yet on gcc 4.9

	#define _aligned_malloc(size, a) aligned_alloc(a, size)

	#else

	extern void* _aligned_malloc(size_t size, size_t alignment);

	#endif

	static inline void _aligned_free(void* p)
	{
		free(p);
	}

	// http://svn.reactos.org/svn/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?view=markup

	__forceinline int _BitScanForward(unsigned long* const Index, const unsigned long Mask)
	{
#if defined(__GCC_ASM_FLAG_OUTPUTS__) && 0
		// Need GCC6 to test the code validity
		int flag;
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index), "=@ccz" (flag) : [Mask] "mr" (Mask));
		return flag;
#else
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask) : "cc");
		return Mask ? 1 : 0;
#endif
	}

	#ifdef __GNUC__

	// gcc 4.8 define __rdtsc but unfortunately the compiler crash...
	// The redefine allow to skip the gcc __rdtsc version -- Gregory
	#define __rdtsc _lnx_rdtsc
	//__forceinline unsigned long long __rdtsc()
	__forceinline unsigned long long _lnx_rdtsc()
	{
		#if defined(__amd64__) || defined(__x86_64__)
		unsigned long long low, high;
		__asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
		return low | (high << 32);
		#else
		unsigned long long retval;
		__asm__ __volatile__("rdtsc" : "=A"(retval));
		return retval;
		#endif
	}

	#endif

#endif

extern std::string format(const char* fmt, ...);

extern void* vmalloc(size_t size, bool code);
extern void vmfree(void* ptr, size_t size);

extern void* fifo_alloc(size_t size, size_t repeat);
extern void fifo_free(void* ptr, size_t size, size_t repeat);

#ifdef ENABLE_VTUNE

	#include "jitprofiling.h"

	#ifdef _WIN32

	#pragma comment(lib, "jitprofiling.lib")

	#endif

#endif

// Note: GL messages are present in common code, so in all renderers.

#define GL_INSERT(type, code, sev, ...) \
	do \
		if (glDebugMessageInsert) glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, type, code, sev, -1, format(__VA_ARGS__).c_str()); \
	while(0);

#if defined(_DEBUG)
#  define GL_CACHE(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xFEAD, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
#  define GL_CACHE(...) (void)(0);
#endif

#if defined(ENABLE_TRACE_REG) && defined(_DEBUG)
#  define GL_REG(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xB0B0, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
#  define GL_REG(...) (void)(0);
#endif

#if defined(ENABLE_EXTRA_LOG) && defined(_DEBUG)
#  define GL_DBG(...) GL_INSERT(GL_DEBUG_TYPE_OTHER, 0xD0D0, GL_DEBUG_SEVERITY_NOTIFICATION, __VA_ARGS__)
#else
#  define GL_DBG(...) (void)(0);
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

// Helper path to dump texture
extern const std::string root_sw;
extern const std::string root_hw;

#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#ifdef __cpp_constinit
#  define CONSTINIT constinit
#elif __has_attribute(require_constant_initialization)
#  define CONSTINIT __attribute__((require_constant_initialization))
#else
#  define CONSTINIT
#endif
