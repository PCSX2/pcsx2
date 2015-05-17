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

#ifdef _WINDOWS

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <comutil.h>
#include "../../common/include/comptr.h"


#define D3DCOLORWRITEENABLE_RGBA (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA)
#define D3D11_SHADER_MACRO D3D10_SHADER_MACRO
#define ID3D11Blob ID3D10Blob

#endif


#ifdef ENABLE_OPENCL

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#endif

// Require at least Visual Studio 2012
#if defined(__linux__) || (defined(_MSC_VER) && (_MSC_VER >= 1700))
#define _CX11_
#define ENABLE_BOOST // queue is from boost but it doesn't require a full boost install
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
#ifdef __x86_64__
typedef uint64 uptr;
#else
typedef uint32 uptr;
#endif

// stdc

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <limits.h>

#include <complex>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#ifdef _CX11_
#include <thread>
#include <atomic>
#endif
#if defined(__linux__) || defined(_CX11_)
#include <mutex>
#include <condition_variable>
#endif

using namespace std;

#include <memory>

#ifdef _WINDOWS

	// Note use GL/glcorearb.h on the future
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include <GL/wglext.h>
	#include "GLLoader.h"

	#include <hash_map>
	#include <hash_set>

	using namespace stdext;

	// hashing algoritms at: http://www.cris.com/~Ttwang/tech/inthash.htm
	// default hash_compare does ldiv and other crazy stuff to reduce speed

	template<> class hash_compare<uint32>
	{
	public:
		enum {bucket_size = 1};

		size_t operator()(uint32 key) const
		{
			key += ~(key << 15);
			key ^= (key >> 10);
			key += (key << 3);
			key ^= (key >> 6);
			key += ~(key << 11);
			key ^= (key >> 16);

			return (size_t)key;
		}

		bool operator()(uint32 a, uint32 b) const
		{
			return a < b;
		}
	};

	template<> class hash_compare<uint64>
	{
	public:
		enum {bucket_size = 1};

		size_t operator()(uint64 key) const
		{
			key += ~(key << 32);
			key ^= (key >> 22);
			key += ~(key << 13);
			key ^= (key >> 8);
			key += (key << 3);
			key ^= (key >> 15);
			key += ~(key << 27);
			key ^= (key >> 31);

			return (size_t)key;
		}

		bool operator()(uint64 a, uint64 b) const
		{
			return a < b;
		}
	};

	#define vsnprintf _vsnprintf
	#define snprintf _snprintf

	#define DIRECTORY_SEPARATOR '\\'

#else

	#define hash_map map
	#define hash_set set

	//#include <ext/hash_map>
	//#include <ext/hash_set>

	// Note use GL/glcorearb.h on the future
	#include <GL/gl.h>
	#include <GL/glext.h>
	#include "GLLoader.h"

	//using namespace __gnu_cxx;

	#define DIRECTORY_SEPARATOR '/'

#endif

#ifdef _MSC_VER

    #define __aligned(t, n) __declspec(align(n)) t

    #define EXPORT_C_(type) extern "C" __declspec(dllexport) type __stdcall
    #define EXPORT_C EXPORT_C_(void)

#else

    #define __aligned(t, n) t __attribute__((aligned(n)))
    #define __fastcall __attribute__((fastcall))

    #define EXPORT_C_(type) extern "C" __attribute__((stdcall,externally_visible,visibility("default"))) type
    #define EXPORT_C EXPORT_C_(void)

    #ifdef __GNUC__

        #include "assert.h"
        #define __forceinline __inline__ __attribute__((always_inline,unused))
        // #define __forceinline __inline__ __attribute__((__always_inline__,__gnu_inline__))
        #define __assume(c) do { if (!(c)) __builtin_unreachable(); } while(0)

    #endif

#endif

extern string format(const char* fmt, ...);

struct delete_object {template<class T> void operator()(T& p) {delete p;}};
struct delete_first {template<class T> void operator()(T& p) {delete p.first;}};
struct delete_second {template<class T> void operator()(T& p) {delete p.second;}};
struct aligned_free_object {template<class T> void operator()(T& p) {_aligned_free(p);}};
struct aligned_free_first {template<class T> void operator()(T& p) {_aligned_free(p.first);}};
struct aligned_free_second {template<class T> void operator()(T& p) {_aligned_free(p.second);}};

#define countof(a) (sizeof(a) / sizeof(a[0]))

#define ALIGN_STACK(n) __aligned(int, n) __dummy;

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

#if defined(_DEBUG) //&& defined(_MSC_VER)

	#include <assert.h>
	#define ASSERT assert

#else

	#define ASSERT(exp) ((void)0)

#endif

#ifdef __x86_64__

	#define _M_AMD64

#endif

// sse
#ifdef __GNUC__
// Convert gcc see define into GSdx (windows) define
#if defined(__AVX2__)
	#define _M_SSE 0x501
#elif defined(__AVX__)
	#define _M_SSE 0x500
#elif defined(__SSE4_2__)
	#define _M_SSE 0x402
#elif defined(__SSE4_1__)
	#define _M_SSE 0x401
#elif defined(__SSSE3__)
	#define _M_SSE 0x301
#elif defined(__SSE2__)
	#define _M_SSE 0x200
#elif defined(__SSE__)
	#define _M_SSE 0x100
#endif
#endif

#if !defined(_M_SSE) && (!defined(_WINDOWS) || defined(_M_AMD64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2)

	#define _M_SSE 0x200

#endif

#if _M_SSE >= 0x200

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

#else

#error TODO: GSVector4 and GSRasterizer needs SSE2

#endif

#if _M_SSE >= 0x301

	#include <tmmintrin.h>

#endif

#if _M_SSE >= 0x401

	#include <smmintrin.h>

#endif

#if _M_SSE >= 0x500

	#include <immintrin.h>

#endif

#undef min
#undef max
#undef abs

#if !defined(_MSC_VER)

	#if defined(__USE_ISOC11) && !defined(ASAN_WORKAROUND) // not supported yet on gcc 4.9

	#define _aligned_malloc(size, a) aligned_alloc(a, size)
	static inline void _aligned_free(void* p) { free(p); }

	#elif !defined(HAVE_ALIGNED_MALLOC)

	extern void* _aligned_malloc(size_t size, size_t alignment);
	extern void _aligned_free(void* p);

	#endif

	// http://svn.reactos.org/svn/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?view=markup

	__forceinline unsigned char _BitScanForward(unsigned long* const Index, const unsigned long Mask)
	{
		__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask));
		
		return Mask ? 1 : 0;
	}

	__forceinline unsigned char _interlockedbittestandreset(volatile long* a, const long b)
	{
		unsigned char retval;
		
		__asm__("lock; btrl %k[b], %[a]; setb %b[retval]" : [retval] "=q" (retval), [a] "+m" (*a) : [b] "Ir" (b) : "memory");
		
		return retval;
	}

	__forceinline unsigned char _interlockedbittestandset(volatile long* a, const long b)
	{
		unsigned char retval;
		
		__asm__("lock; btsl %k[b], %[a]; setc %b[retval]" : [retval] "=q" (retval), [a] "+m" (*a) : [b] "Ir" (b) : "memory");
		
		return retval;
	}

	__forceinline long _InterlockedCompareExchange(volatile long* const Destination, const long Exchange, const long Comperand)
	{
		long retval = Comperand;
		
		__asm__("lock; cmpxchgl %k[Exchange], %[Destination]" : [retval] "+a" (retval) : [Destination] "m" (*Destination), [Exchange] "q" (Exchange): "memory");
		
		return retval;
	}

	__forceinline long _InterlockedExchange(volatile long* const Target, const long Value)
	{
		long retval = Value;
		
		__asm__("xchgl %[retval], %[Target]" : [retval] "+r" (retval) : [Target] "m" (*Target) : "memory");

		return retval;
	}

	__forceinline long _InterlockedExchangeAdd(volatile long* const Addend, const long Value)
	{
		long retval = Value;
		
		__asm__("lock; xaddl %[retval], %[Addend]" : [retval] "+r" (retval) : [Addend] "m" (*Addend) : "memory");
		
		return retval;
	}
	
	__forceinline short _InterlockedExchangeAdd16(volatile short* const Addend, const short Value)
	{
		short retval = Value;
		
		__asm__("lock; xaddw %[retval], %[Addend]" : [retval] "+r" (retval) : [Addend] "m" (*Addend) : "memory");
		
		return retval;
	}

	__forceinline long _InterlockedDecrement(volatile long* const lpAddend)
	{
		return _InterlockedExchangeAdd(lpAddend, -1) - 1;
	}
	
	__forceinline long _InterlockedIncrement(volatile long* const lpAddend)
	{
		return _InterlockedExchangeAdd(lpAddend, 1) + 1;
	}
	
	__forceinline short _InterlockedDecrement16(volatile short* const lpAddend)
	{
		return _InterlockedExchangeAdd16(lpAddend, -1) - 1;
	}
	
	__forceinline short _InterlockedIncrement16(volatile short* const lpAddend)
	{
		return _InterlockedExchangeAdd16(lpAddend, 1) + 1;
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

extern void* vmalloc(size_t size, bool code);
extern void vmfree(void* ptr, size_t size);

#ifdef _WINDOWS

	#ifdef ENABLE_VTUNE

	#include <JITProfiling.h>

	#pragma comment(lib, "jitprofiling.lib")

	#endif

#endif

// Except apple any sane driver support this extension
#if defined(__linux__) && defined(_DEBUG)
#define GL_CACHE(...) gl_DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0xFEAD, GL_DEBUG_SEVERITY_NOTIFICATION, -1, format(__VA_ARGS__).c_str());
#else
#define GL_CACHE(...) (0);
#endif

#if defined(__linux__) && defined(ENABLE_OGL_DEBUG)
#define GL_PUSH(...)	gl_PushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0xBAD, -1, format(__VA_ARGS__).c_str());
#define GL_POP()        gl_PopDebugGroup();
#define GL_INS(...)		gl_DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR, 0xDEAD, GL_DEBUG_SEVERITY_MEDIUM, -1, format(__VA_ARGS__).c_str());
#define GL_PERF(...)	gl_DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, 0xFEE1, GL_DEBUG_SEVERITY_MEDIUM, -1, format(__VA_ARGS__).c_str());
#else
#define GL_PUSH(...) (0);
#define GL_POP()     (0);
#define GL_INS(...)  (0);
#define GL_PERF(...) (0);
#endif

// Helper path to dump texture
#ifdef _WINDOWS
const std::string root_sw("c:\\temp1\\_");
const std::string root_hw("c:\\temp2\\_");
#else
const std::string root_sw("/tmp/GS_SW_dump/");
const std::string root_hw("/tmp/GS_HW_dump/");
#endif
