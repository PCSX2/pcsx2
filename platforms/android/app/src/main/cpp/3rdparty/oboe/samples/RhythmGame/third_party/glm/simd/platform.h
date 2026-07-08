/// @ref simd
/// @file glm/simd/platform.h

#pragma once

///////////////////////////////////////////////////////////////////////////////////
// Platform

#define GLM_PLATFORM_UNKNOWN		0x00000000
#define GLM_PLATFORM_WINDOWS		0x00010000
#define GLM_PLATFORM_LINUX			0x00020000
#define GLM_PLATFORM_APPLE			0x00040000
//#define GLM_PLATFORM_IOS			0x00080000
#define GLM_PLATFORM_ANDROID		0x00100000
#define GLM_PLATFORM_CHROME_NACL	0x00200000
#define GLM_PLATFORM_UNIX			0x00400000
#define GLM_PLATFORM_QNXNTO			0x00800000
#define GLM_PLATFORM_WINCE			0x01000000
#define GLM_PLATFORM_CYGWIN			0x02000000

#ifdef GLM_FORCE_PLATFORM_UNKNOWN
#	define GLM_PLATFORM GLM_PLATFORM_UNKNOWN
#elif defined(__CYGWIN__)
#	define GLM_PLATFORM GLM_PLATFORM_CYGWIN
#elif defined(__QNXNTO__)
#	define GLM_PLATFORM GLM_PLATFORM_QNXNTO
#elif defined(__APPLE__)
#	define GLM_PLATFORM GLM_PLATFORM_APPLE
#elif defined(WINCE)
#	define GLM_PLATFORM GLM_PLATFORM_WINCE
#elif defined(_WIN32)
#	define GLM_PLATFORM GLM_PLATFORM_WINDOWS
#elif defined(__native_client__)
#	define GLM_PLATFORM GLM_PLATFORM_CHROME_NACL
#elif defined(__ANDROID__)
#	define GLM_PLATFORM GLM_PLATFORM_ANDROID
#elif defined(__linux)
#	define GLM_PLATFORM GLM_PLATFORM_LINUX
#elif defined(__unix)
#	define GLM_PLATFORM GLM_PLATFORM_UNIX
#else
#	define GLM_PLATFORM GLM_PLATFORM_UNKNOWN
#endif//

// Report platform detection
#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_PLATFORM_DISPLAYED)
#	define GLM_MESSAGE_PLATFORM_DISPLAYED
#	if(GLM_PLATFORM & GLM_PLATFORM_QNXNTO)
#		pragma message("GLM: QNX platform detected")
//#	elif(GLM_PLATFORM & GLM_PLATFORM_IOS)
//#		pragma message("GLM: iOS platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_APPLE)
#		pragma message("GLM: Apple platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_WINCE)
#		pragma message("GLM: WinCE platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_WINDOWS)
#		pragma message("GLM: Windows platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_CHROME_NACL)
#		pragma message("GLM: Native Client detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_ANDROID)
#		pragma message("GLM: Android platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_LINUX)
#		pragma message("GLM: Linux platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_UNIX)
#		pragma message("GLM: UNIX platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_UNKNOWN)
#		pragma message("GLM: platform unknown")
#	else
#		pragma message("GLM: platform not detected")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Compiler

#define GLM_COMPILER_UNKNOWN		0x00000000

// Intel
#define GLM_COMPILER_INTEL			0x00100000
#define GLM_COMPILER_INTEL12		0x00100010
#define GLM_COMPILER_INTEL12_1		0x00100020
#define GLM_COMPILER_INTEL13		0x00100030
#define GLM_COMPILER_INTEL14		0x00100040
#define GLM_COMPILER_INTEL15		0x00100050
#define GLM_COMPILER_INTEL16		0x00100060

// Visual C++ defines
#define GLM_COMPILER_VC				0x01000000
#define GLM_COMPILER_VC10			0x01000090
#define GLM_COMPILER_VC11			0x010000A0
#define GLM_COMPILER_VC12			0x010000B0
#define GLM_COMPILER_VC14			0x010000C0
#define GLM_COMPILER_VC15			0x010000D0

// GCC defines
#define GLM_COMPILER_GCC			0x02000000
#define GLM_COMPILER_GCC44			0x020000B0
#define GLM_COMPILER_GCC45			0x020000C0
#define GLM_COMPILER_GCC46			0x020000D0
#define GLM_COMPILER_GCC47			0x020000E0
#define GLM_COMPILER_GCC48			0x020000F0
#define GLM_COMPILER_GCC49			0x02000100
#define GLM_COMPILER_GCC50			0x02000200
#define GLM_COMPILER_GCC51			0x02000300
#define GLM_COMPILER_GCC52			0x02000400
#define GLM_COMPILER_GCC53			0x02000500
#define GLM_COMPILER_GCC54			0x02000600
#define GLM_COMPILER_GCC60			0x02000700
#define GLM_COMPILER_GCC61			0x02000800
#define GLM_COMPILER_GCC62			0x02000900
#define GLM_COMPILER_GCC70			0x02000A00
#define GLM_COMPILER_GCC71			0x02000B00
#define GLM_COMPILER_GCC72			0x02000C00
#define GLM_COMPILER_GCC80			0x02000D00

// CUDA
#define GLM_COMPILER_CUDA			0x10000000
#define GLM_COMPILER_CUDA40			0x10000040
#define GLM_COMPILER_CUDA41			0x10000050
#define GLM_COMPILER_CUDA42			0x10000060
#define GLM_COMPILER_CUDA50			0x10000070
#define GLM_COMPILER_CUDA60			0x10000080
#define GLM_COMPILER_CUDA65			0x10000090
#define GLM_COMPILER_CUDA70			0x100000A0
#define GLM_COMPILER_CUDA75			0x100000B0
#define GLM_COMPILER_CUDA80			0x100000C0

// Clang
#define GLM_COMPILER_CLANG			0x20000000
#define GLM_COMPILER_CLANG32		0x20000030
#define GLM_COMPILER_CLANG33		0x20000040
#define GLM_COMPILER_CLANG34		0x20000050
#define GLM_COMPILER_CLANG35		0x20000060
#define GLM_COMPILER_CLANG36		0x20000070
#define GLM_COMPILER_CLANG37		0x20000080
#define GLM_COMPILER_CLANG38		0x20000090
#define GLM_COMPILER_CLANG39		0x200000A0
#define GLM_COMPILER_CLANG40		0x200000B0
#define GLM_COMPILER_CLANG41		0x200000C0
#define GLM_COMPILER_CLANG42		0x200000D0

// Build model
#define GLM_MODEL_32				0x00000010
#define GLM_MODEL_64				0x00000020

// Force generic C++ compiler
#ifdef GLM_FORCE_COMPILER_UNKNOWN
#	define GLM_COMPILER GLM_COMPILER_UNKNOWN

#elif defined(__INTEL_COMPILER)
#	if __INTEL_COMPILER == 1200
#		define GLM_COMPILER GLM_COMPILER_INTEL12
#	elif __INTEL_COMPILER == 1210
#		define GLM_COMPILER GLM_COMPILER_INTEL12_1
#	elif __INTEL_COMPILER == 1300
#		define GLM_COMPILER GLM_COMPILER_INTEL13
#	elif __INTEL_COMPILER == 1400
#		define GLM_COMPILER GLM_COMPILER_INTEL14
#	elif __INTEL_COMPILER == 1500
#		define GLM_COMPILER GLM_COMPILER_INTEL15
#	elif __INTEL_COMPILER >= 1600
#		define GLM_COMPILER GLM_COMPILER_INTEL16
#	else
#		define GLM_COMPILER GLM_COMPILER_INTEL
#	endif

// CUDA
#elif defined(__CUDACC__)
#	if !defined(CUDA_VERSION) && !defined(GLM_FORCE_CUDA)
#		include <cuda.h>  // make sure version is defined since nvcc does not define it itself!
#	endif
#	if CUDA_VERSION < 3000
#		error "GLM requires CUDA 3.0 or higher"
#	else
#		define GLM_COMPILER GLM_COMPILER_CUDA
#	endif

// Clang
#elif defined(__clang__)
#	if GLM_PLATFORM & GLM_PLATFORM_APPLE
#		if __clang_major__ == 5 && __clang_minor__ == 0
#			define GLM_COMPILER GLM_COMPILER_CLANG33
#		elif __clang_major__ == 5 && __clang_minor__ == 1
#			define GLM_COMPILER GLM_COMPILER_CLANG34
#		elif __clang_major__ == 6 && __clang_minor__ == 0
#			define GLM_COMPILER GLM_COMPILER_CLANG35
#		elif __clang_major__ == 6 && __clang_minor__ >= 1
#			define GLM_COMPILER GLM_COMPILER_CLANG36
#		elif __clang_major__ >= 7
#			define GLM_COMPILER GLM_COMPILER_CLANG37
#		else
#			define GLM_COMPILER GLM_COMPILER_CLANG
#		endif
#	else
#		if __clang_major__ == 3 && __clang_minor__ == 0
#			define GLM_COMPILER GLM_COMPILER_CLANG30
#		elif __clang_major__ == 3 && __clang_minor__ == 1
#			define GLM_COMPILER GLM_COMPILER_CLANG31
#		elif __clang_major__ == 3 && __clang_minor__ == 2
#			define GLM_COMPILER GLM_COMPILER_CLANG32
#		elif __clang_major__ == 3 && __clang_minor__ == 3
#			define GLM_COMPILER GLM_COMPILER_CLANG33
#		elif __clang_major__ == 3 && __clang_minor__ == 4
#			define GLM_COMPILER GLM_COMPILER_CLANG34
#		elif __clang_major__ == 3 && __clang_minor__ == 5
#			define GLM_COMPILER GLM_COMPILER_CLANG35
#		elif __clang_major__ == 3 && __clang_minor__ == 6
#			define GLM_COMPILER GLM_COMPILER_CLANG36
#		elif __clang_major__ == 3 && __clang_minor__ == 7
#			define GLM_COMPILER GLM_COMPILER_CLANG37
#		elif __clang_major__ == 3 && __clang_minor__ == 8
#			define GLM_COMPILER GLM_COMPILER_CLANG38
#		elif __clang_major__ == 3 && __clang_minor__ >= 9
#			define GLM_COMPILER GLM_COMPILER_CLANG39
#		elif __clang_major__ == 4 && __clang_minor__ == 0
#			define GLM_COMPILER GLM_COMPILER_CLANG40
#		elif __clang_major__ == 4 && __clang_minor__ == 1
#			define GLM_COMPILER GLM_COMPILER_CLANG41
#		elif __clang_major__ == 4 && __clang_minor__ >= 2
#			define GLM_COMPILER GLM_COMPILER_CLANG42
#		elif __clang_major__ >= 4
#			define GLM_COMPILER GLM_COMPILER_CLANG42
#		else
#			define GLM_COMPILER GLM_COMPILER_CLANG
#		endif
#	endif

// Visual C++
#elif defined(_MSC_VER)
#	if _MSC_VER < 1600
#		error "GLM requires Visual C++ 10 - 2010 or higher"
#	elif _MSC_VER == 1600
#		define GLM_COMPILER GLM_COMPILER_VC11
#	elif _MSC_VER == 1700
#		define GLM_COMPILER GLM_COMPILER_VC11
#	elif _MSC_VER == 1800
#		define GLM_COMPILER GLM_COMPILER_VC12
#	elif _MSC_VER == 1900
#		define GLM_COMPILER GLM_COMPILER_VC14
#	elif _MSC_VER >= 1910
#		define GLM_COMPILER GLM_COMPILER_VC15
#	else//_MSC_VER
#		define GLM_COMPILER GLM_COMPILER_VC
#	endif//_MSC_VER

// G++
#elif defined(__GNUC__) || defined(__MINGW32__)
#	if (__GNUC__ == 4) && (__GNUC_MINOR__ == 2)
#		define GLM_COMPILER (GLM_COMPILER_GCC42)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 3)
#		define GLM_COMPILER (GLM_COMPILER_GCC43)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 4)
#		define GLM_COMPILER (GLM_COMPILER_GCC44)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 5)
#		define GLM_COMPILER (GLM_COMPILER_GCC45)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 6)
#		define GLM_COMPILER (GLM_COMPILER_GCC46)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 7)
#		define GLM_COMPILER (GLM_COMPILER_GCC47)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ == 8)
#		define GLM_COMPILER (GLM_COMPILER_GCC48)
#	elif (__GNUC__ == 4) && (__GNUC_MINOR__ >= 9)
#		define GLM_COMPILER (GLM_COMPILER_GCC49)
#	elif (__GNUC__ == 5) && (__GNUC_MINOR__ == 0)
#		define GLM_COMPILER (GLM_COMPILER_GCC50)
#	elif (__GNUC__ == 5) && (__GNUC_MINOR__ == 1)
#		define GLM_COMPILER (GLM_COMPILER_GCC51)
#	elif (__GNUC__ == 5) && (__GNUC_MINOR__ == 2)
#		define GLM_COMPILER (GLM_COMPILER_GCC52)
#	elif (__GNUC__ == 5) && (__GNUC_MINOR__ == 3)
#		define GLM_COMPILER (GLM_COMPILER_GCC53)
#	elif (__GNUC__ == 5) && (__GNUC_MINOR__ >= 4)
#		define GLM_COMPILER (GLM_COMPILER_GCC54)
#	elif (__GNUC__ == 6) && (__GNUC_MINOR__ == 0)
#		define GLM_COMPILER (GLM_COMPILER_GCC60)
#	elif (__GNUC__ == 6) && (__GNUC_MINOR__ == 1)
#		define GLM_COMPILER (GLM_COMPILER_GCC61)
#	elif (__GNUC__ == 6) && (__GNUC_MINOR__ >= 2)
#		define GLM_COMPILER (GLM_COMPILER_GCC62)
#	elif (__GNUC__ == 7) && (__GNUC_MINOR__ == 0)
#		define GLM_COMPILER (GLM_COMPILER_GCC70)
#	elif (__GNUC__ == 7) && (__GNUC_MINOR__ == 1)
#		define GLM_COMPILER (GLM_COMPILER_GCC71)
#	elif (__GNUC__ == 7) && (__GNUC_MINOR__ == 2)
#		define GLM_COMPILER (GLM_COMPILER_GCC72)
#	elif (__GNUC__ >= 8)
#		define GLM_COMPILER (GLM_COMPILER_GCC80)
#	else
#		define GLM_COMPILER (GLM_COMPILER_GCC)
#	endif

#else
#	define GLM_COMPILER GLM_COMPILER_UNKNOWN
#endif

#ifndef GLM_COMPILER
#	error "GLM_COMPILER undefined, your compiler may not be supported by GLM. Add #define GLM_COMPILER 0 to ignore this message."
#endif//GLM_COMPILER

///////////////////////////////////////////////////////////////////////////////////
// Instruction sets

// User defines: GLM_FORCE_PURE GLM_FORCE_SSE2 GLM_FORCE_SSE3 GLM_FORCE_AVX GLM_FORCE_AVX2 GLM_FORCE_AVX2

#define GLM_ARCH_X86_BIT		0x00000001
#define GLM_ARCH_SSE2_BIT		0x00000002
#define GLM_ARCH_SSE3_BIT		0x00000004
#define GLM_ARCH_SSSE3_BIT		0x00000008
#define GLM_ARCH_SSE41_BIT		0x00000010
#define GLM_ARCH_SSE42_BIT		0x00000020
#define GLM_ARCH_AVX_BIT		0x00000040
#define GLM_ARCH_AVX2_BIT		0x00000080
#define GLM_ARCH_AVX512_BIT		0x00000100 // Skylake subset
#define GLM_ARCH_ARM_BIT		0x00000100
#define GLM_ARCH_NEON_BIT		0x00000200
#define GLM_ARCH_MIPS_BIT		0x00010000
#define GLM_ARCH_PPC_BIT		0x01000000

#define GLM_ARCH_PURE		(0x00000000)
#define GLM_ARCH_X86		(GLM_ARCH_X86_BIT)
#define GLM_ARCH_SSE2		(GLM_ARCH_SSE2_BIT | GLM_ARCH_X86)
#define GLM_ARCH_SSE3		(GLM_ARCH_SSE3_BIT | GLM_ARCH_SSE2)
#define GLM_ARCH_SSSE3		(GLM_ARCH_SSSE3_BIT | GLM_ARCH_SSE3)
#define GLM_ARCH_SSE41		(GLM_ARCH_SSE41_BIT | GLM_ARCH_SSSE3)
#define GLM_ARCH_SSE42		(GLM_ARCH_SSE42_BIT | GLM_ARCH_SSE41)
#define GLM_ARCH_AVX		(GLM_ARCH_AVX_BIT | GLM_ARCH_SSE42)
#define GLM_ARCH_AVX2		(GLM_ARCH_AVX2_BIT | GLM_ARCH_AVX)
#define GLM_ARCH_AVX512		(GLM_ARCH_AVX512_BIT | GLM_ARCH_AVX2) // Skylake subset
#define GLM_ARCH_ARM		(GLM_ARCH_ARM_BIT)
#define GLM_ARCH_NEON		(GLM_ARCH_NEON_BIT | GLM_ARCH_ARM)
#define GLM_ARCH_MIPS		(GLM_ARCH_MIPS_BIT)
#define GLM_ARCH_PPC		(GLM_ARCH_PPC_BIT)

#if defined(GLM_FORCE_PURE)
#	define GLM_ARCH GLM_ARCH_PURE
#elif defined(GLM_FORCE_MIPS)
#	define GLM_ARCH (GLM_ARCH_MIPS)
#elif defined(GLM_FORCE_PPC)
#	define GLM_ARCH (GLM_ARCH_PPC)
#elif defined(GLM_FORCE_NEON)
#	define GLM_ARCH (GLM_ARCH_NEON)
#elif defined(GLM_FORCE_AVX512)
#	define GLM_ARCH (GLM_ARCH_AVX512)
#elif defined(GLM_FORCE_AVX2)
#	define GLM_ARCH (GLM_ARCH_AVX2)
#elif defined(GLM_FORCE_AVX)
#	define GLM_ARCH (GLM_ARCH_AVX)
#elif defined(GLM_FORCE_SSE42)
#	define GLM_ARCH (GLM_ARCH_SSE42)
#elif defined(GLM_FORCE_SSE41)
#	define GLM_ARCH (GLM_ARCH_SSE41)
#elif defined(GLM_FORCE_SSSE3)
#	define GLM_ARCH (GLM_ARCH_SSSE3)
#elif defined(GLM_FORCE_SSE3)
#	define GLM_ARCH (GLM_ARCH_SSE3)
#elif defined(GLM_FORCE_SSE2)
#	define GLM_ARCH (GLM_ARCH_SSE2)
#elif (GLM_COMPILER & (GLM_COMPILER_CLANG | GLM_COMPILER_GCC)) || ((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_PLATFORM & GLM_PLATFORM_LINUX))
//	This is Skylake set of instruction set
#	if defined(__AVX512BW__) && defined(__AVX512F__) && defined(__AVX512CD__) && defined(__AVX512VL__) && defined(__AVX512DQ__)
#		define GLM_ARCH (GLM_ARCH_AVX512)
#	elif defined(__AVX2__)
#		define GLM_ARCH (GLM_ARCH_AVX2)
#	elif defined(__AVX__)
#		define GLM_ARCH (GLM_ARCH_AVX)
#	elif defined(__SSE4_2__)
#		define GLM_ARCH (GLM_ARCH_SSE42)
#	elif defined(__SSE4_1__)
#		define GLM_ARCH (GLM_ARCH_SSE41)
#	elif defined(__SSSE3__)
#		define GLM_ARCH (GLM_ARCH_SSSE3)
#	elif defined(__SSE3__)
#		define GLM_ARCH (GLM_ARCH_SSE3)
#	elif defined(__SSE2__)
#		define GLM_ARCH (GLM_ARCH_SSE2)
#	elif defined(__i386__) || defined(__x86_64__)
#		define GLM_ARCH (GLM_ARCH_X86)
#	elif defined(__ARM_NEON)
#		define GLM_ARCH (GLM_ARCH_ARM | GLM_ARCH_NEON)
#	elif defined(__arm__ )
#		define GLM_ARCH (GLM_ARCH_ARM)
#	elif defined(__mips__ )
#		define GLM_ARCH (GLM_ARCH_MIPS)
#	elif defined(__powerpc__ )
#		define GLM_ARCH (GLM_ARCH_PPC)
#	else
#		define GLM_ARCH (GLM_ARCH_PURE)
#	endif
#elif (GLM_COMPILER & GLM_COMPILER_VC) || ((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_PLATFORM & GLM_PLATFORM_WINDOWS))
#	if defined(_M_ARM)
#		define GLM_ARCH (GLM_ARCH_ARM)
#	elif defined(__AVX2__)
#		define GLM_ARCH (GLM_ARCH_AVX2)
#	elif defined(__AVX__)
#		define GLM_ARCH (GLM_ARCH_AVX)
#	elif defined(_M_X64)
#		define GLM_ARCH (GLM_ARCH_SSE2)
#	elif defined(_M_IX86_FP)
#		if _M_IX86_FP >= 2
#			define GLM_ARCH (GLM_ARCH_SSE2)
#		else
#			define GLM_ARCH (GLM_ARCH_PURE)
#		endif
#	elif defined(_M_PPC)
#		define GLM_ARCH (GLM_ARCH_PPC)
#	else
#		define GLM_ARCH (GLM_ARCH_PURE)
#	endif
#else
#	define GLM_ARCH GLM_ARCH_PURE
#endif

// With MinGW-W64, including intrinsic headers before intrin.h will produce some errors. The problem is
// that windows.h (and maybe other headers) will silently include intrin.h, which of course causes problems.
// To fix, we just explicitly include intrin.h here.
#if defined(__MINGW64__) && (GLM_ARCH != GLM_ARCH_PURE)
#	include <intrin.h>
#endif

#if GLM_ARCH & GLM_ARCH_AVX2_BIT
#	include <immintrin.h>
#elif GLM_ARCH & GLM_ARCH_AVX_BIT
#	include <immintrin.h>
#elif GLM_ARCH & GLM_ARCH_SSE42_BIT
#	if GLM_COMPILER & GLM_COMPILER_CLANG
#		include <popcntintrin.h>
#	endif
#	include <nmmintrin.h>
#elif GLM_ARCH & GLM_ARCH_SSE41_BIT
#	include <smmintrin.h>
#elif GLM_ARCH & GLM_ARCH_SSSE3_BIT
#	include <tmmintrin.h>
#elif GLM_ARCH & GLM_ARCH_SSE3_BIT
#	include <pmmintrin.h>
#elif GLM_ARCH & GLM_ARCH_SSE2_BIT
#	include <emmintrin.h>
#endif//GLM_ARCH

#if GLM_ARCH & GLM_ARCH_SSE2_BIT
	typedef __m128		glm_vec4;
	typedef __m128i		glm_ivec4;
	typedef __m128i		glm_uvec4;
#endif

#if GLM_ARCH & GLM_ARCH_AVX_BIT
	typedef __m256d		glm_dvec4;
#endif

#if GLM_ARCH & GLM_ARCH_AVX2_BIT
	typedef __m256i		glm_i64vec4;
	typedef __m256i		glm_u64vec4;
#endif
