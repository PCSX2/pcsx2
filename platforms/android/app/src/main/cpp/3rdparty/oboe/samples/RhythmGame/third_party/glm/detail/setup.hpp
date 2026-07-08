/// @ref core
/// @file glm/detail/setup.hpp

#pragma once

#if (defined(GLM_FORCE_SWIZZLE) || defined(GLM_SWIZZLE)) && defined(GLM_FORCE_UNRESTRICTED_GENTYPE)
#	error "Both GLM_FORCE_SWIZZLE and GLM_FORCE_UNRESTRICTED_GENTYPE can't be defined at the same time"
#endif

///////////////////////////////////////////////////////////////////////////////////
// Messages

#ifdef GLM_MESSAGES
#	pragma message("GLM: GLM_MESSAGES is deprecated, use GLM_FORCE_MESSAGES instead")
#endif

#define GLM_MESSAGES_ENABLED 1
#define GLM_MESSAGES_DISABLE 0

#if defined(GLM_FORCE_MESSAGES) || defined(GLM_MESSAGES)
#	undef GLM_MESSAGES
#	define GLM_MESSAGES GLM_MESSAGES_ENABLED
#else
#	undef GLM_MESSAGES
#	define GLM_MESSAGES GLM_MESSAGES_DISABLE
#endif

#include <cassert>
#include <cstddef>
#include "../simd/platform.h"

///////////////////////////////////////////////////////////////////////////////////
// Version

#define GLM_VERSION					98
#define GLM_VERSION_MAJOR			0
#define GLM_VERSION_MINOR			9
#define GLM_VERSION_PATCH			8
#define GLM_VERSION_REVISION		4

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_VERSION_DISPLAYED)
#	define GLM_MESSAGE_VERSION_DISPLAYED
#	pragma message ("GLM: version 0.9.8.4")
#endif//GLM_MESSAGES

// Report compiler detection
#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_COMPILER_DISPLAYED)
#	define GLM_MESSAGE_COMPILER_DISPLAYED
#	if GLM_COMPILER & GLM_COMPILER_CUDA
#		pragma message("GLM: CUDA compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_VC
#		pragma message("GLM: Visual C++ compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_CLANG
#		pragma message("GLM: Clang compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#		pragma message("GLM: Intel Compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_GCC
#		pragma message("GLM: GCC compiler detected")
#	else
#		pragma message("GLM: Compiler not detected")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Build model

#if defined(__arch64__) || defined(__LP64__) || defined(_M_X64) || defined(__ppc64__) || defined(__x86_64__)
#	define GLM_MODEL	GLM_MODEL_64
#elif defined(__i386__) || defined(__ppc__)
#	define GLM_MODEL	GLM_MODEL_32
#else
#	define GLM_MODEL	GLM_MODEL_32
#endif//

#if !defined(GLM_MODEL) && GLM_COMPILER != 0
#	error "GLM_MODEL undefined, your compiler may not be supported by GLM. Add #define GLM_MODEL 0 to ignore this message."
#endif//GLM_MODEL

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_MODEL_DISPLAYED)
#	define GLM_MESSAGE_MODEL_DISPLAYED
#	if(GLM_MODEL == GLM_MODEL_64)
#		pragma message("GLM: 64 bits model")
#	elif(GLM_MODEL == GLM_MODEL_32)
#		pragma message("GLM: 32 bits model")
#	endif//GLM_MODEL
#endif//GLM_MESSAGES

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_ARCH_DISPLAYED)
#	define GLM_MESSAGE_ARCH_DISPLAYED
#	if(GLM_ARCH == GLM_ARCH_PURE)
#		pragma message("GLM: Platform independent code")
#	elif(GLM_ARCH == GLM_ARCH_AVX2)
#		pragma message("GLM: AVX2 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_AVX)
#		pragma message("GLM: AVX instruction set")
#	elif(GLM_ARCH == GLM_ARCH_SSE42)
#		pragma message("GLM: SSE4.2 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_SSE41)
#		pragma message("GLM: SSE4.1 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_SSSE3)
#		pragma message("GLM: SSSE3 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_SSE3)
#		pragma message("GLM: SSE3 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_SSE2)
#		pragma message("GLM: SSE2 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_X86)
#		pragma message("GLM: x86 instruction set")
#	elif(GLM_ARCH == GLM_ARCH_NEON)
#		pragma message("GLM: NEON instruction set")
#	elif(GLM_ARCH == GLM_ARCH_ARM)
#		pragma message("GLM: ARM instruction set")
#	elif(GLM_ARCH == GLM_ARCH_MIPS)
#		pragma message("GLM: MIPS instruction set")
#	elif(GLM_ARCH == GLM_ARCH_PPC)
#		pragma message("GLM: PowerPC architechture")
#	endif//GLM_ARCH
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// C++ Version

// User defines: GLM_FORCE_CXX98, GLM_FORCE_CXX03, GLM_FORCE_CXX11, GLM_FORCE_CXX14

#define GLM_LANG_CXX98_FLAG			(1 << 1)
#define GLM_LANG_CXX03_FLAG			(1 << 2)
#define GLM_LANG_CXX0X_FLAG			(1 << 3)
#define GLM_LANG_CXX11_FLAG			(1 << 4)
#define GLM_LANG_CXX1Y_FLAG			(1 << 5)
#define GLM_LANG_CXX14_FLAG			(1 << 6)
#define GLM_LANG_CXX1Z_FLAG			(1 << 7)
#define GLM_LANG_CXXMS_FLAG			(1 << 8)
#define GLM_LANG_CXXGNU_FLAG		(1 << 9)

#define GLM_LANG_CXX98			GLM_LANG_CXX98_FLAG
#define GLM_LANG_CXX03			(GLM_LANG_CXX98 | GLM_LANG_CXX03_FLAG)
#define GLM_LANG_CXX0X			(GLM_LANG_CXX03 | GLM_LANG_CXX0X_FLAG)
#define GLM_LANG_CXX11			(GLM_LANG_CXX0X | GLM_LANG_CXX11_FLAG)
#define GLM_LANG_CXX1Y			(GLM_LANG_CXX11 | GLM_LANG_CXX1Y_FLAG)
#define GLM_LANG_CXX14			(GLM_LANG_CXX1Y | GLM_LANG_CXX14_FLAG)
#define GLM_LANG_CXX1Z			(GLM_LANG_CXX14 | GLM_LANG_CXX1Z_FLAG)
#define GLM_LANG_CXXMS			GLM_LANG_CXXMS_FLAG
#define GLM_LANG_CXXGNU			GLM_LANG_CXXGNU_FLAG

#if defined(GLM_FORCE_CXX14)
#	if((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER <= GLM_COMPILER_GCC50)) || ((GLM_COMPILER & GLM_COMPILER_CLANG) && (GLM_COMPILER <= GLM_COMPILER_CLANG34))
#			pragma message("GLM: Using GLM_FORCE_CXX14 with a compiler that doesn't fully support C++14")
#	elif GLM_COMPILER & GLM_COMPILER_VC
#			pragma message("GLM: Using GLM_FORCE_CXX14 but there is no known version of Visual C++ compiler that fully supports C++14")
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#			pragma message("GLM: Using GLM_FORCE_CXX14 but there is no known version of ICC compiler that fully supports C++14")
#	endif
#	define GLM_LANG GLM_LANG_CXX14
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX11)
#	if((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER <= GLM_COMPILER_GCC48)) || ((GLM_COMPILER & GLM_COMPILER_CLANG) && (GLM_COMPILER <= GLM_COMPILER_CLANG33))
#			pragma message("GLM: Using GLM_FORCE_CXX11 with a compiler that doesn't fully support C++11")
#	elif GLM_COMPILER & GLM_COMPILER_VC
#			pragma message("GLM: Using GLM_FORCE_CXX11 but there is no known version of Visual C++ compiler that fully supports C++11")
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#			pragma message("GLM: Using GLM_FORCE_CXX11 but there is no known version of ICC compiler that fully supports C++11")
#	endif
#	define GLM_LANG GLM_LANG_CXX11
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX03)
#	define GLM_LANG GLM_LANG_CXX03
#elif defined(GLM_FORCE_CXX98)
#	define GLM_LANG GLM_LANG_CXX98
#else
#	if GLM_COMPILER & GLM_COMPILER_CLANG
#		if __cplusplus >= 201402L // GLM_COMPILER_CLANG34 + -std=c++14
#			define GLM_LANG GLM_LANG_CXX14
#		elif __has_feature(cxx_decltype_auto) && __has_feature(cxx_aggregate_nsdmi) // GLM_COMPILER_CLANG33 + -std=c++1y
#			define GLM_LANG GLM_LANG_CXX1Y
#		elif __cplusplus >= 201103L // GLM_COMPILER_CLANG33 + -std=c++11
#			define GLM_LANG GLM_LANG_CXX11
#		elif __has_feature(cxx_static_assert) // GLM_COMPILER_CLANG29 + -std=c++11
#			define GLM_LANG GLM_LANG_CXX0X
#		elif __cplusplus >= 199711L
#			define GLM_LANG GLM_LANG_CXX98
#		else
#			define GLM_LANG GLM_LANG_CXX
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_GCC
#		if __cplusplus >= 201402L
#			define GLM_LANG GLM_LANG_CXX14
#		elif __cplusplus >= 201103L
#			define GLM_LANG GLM_LANG_CXX11
#		elif defined(__GXX_EXPERIMENTAL_CXX0X__)
#			define GLM_LANG GLM_LANG_CXX0X
#		else
#			define GLM_LANG GLM_LANG_CXX98
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_VC
#		ifdef _MSC_EXTENSIONS
#			if __cplusplus >= 201402L
#				define GLM_LANG (GLM_LANG_CXX14 | GLM_LANG_CXXMS_FLAG)
//#			elif GLM_COMPILER >= GLM_COMPILER_VC14
//#				define GLM_LANG (GLM_LANG_CXX1Y | GLM_LANG_CXXMS_FLAG)
#			elif __cplusplus >= 201103L
#				define GLM_LANG (GLM_LANG_CXX11 | GLM_LANG_CXXMS_FLAG)
#			elif GLM_COMPILER >= GLM_COMPILER_VC10
#				define GLM_LANG (GLM_LANG_CXX0X | GLM_LANG_CXXMS_FLAG)
#			elif __cplusplus >= 199711L
#				define GLM_LANG (GLM_LANG_CXX98 | GLM_LANG_CXXMS_FLAG)
#			else
#				define GLM_LANG (GLM_LANG_CXX | GLM_LANG_CXXMS_FLAG)
#			endif
#		else
#			if __cplusplus >= 201402L
#				define GLM_LANG GLM_LANG_CXX14
#			elif __cplusplus >= 201103L
#				define GLM_LANG GLM_LANG_CXX11
#			elif GLM_COMPILER >= GLM_COMPILER_VC10
#				define GLM_LANG GLM_LANG_CXX0X
#			elif __cplusplus >= 199711L
#				define GLM_LANG GLM_LANG_CXX98
#			else
#				define GLM_LANG GLM_LANG_CXX
#			endif
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#		ifdef _MSC_EXTENSIONS
#			define GLM_MSC_EXT GLM_LANG_CXXMS_FLAG
#		else
#			define GLM_MSC_EXT 0
#		endif
#		if __cplusplus >= 201402L
#			define GLM_LANG (GLM_LANG_CXX14 | GLM_MSC_EXT)
#		elif __cplusplus >= 201103L
#			define GLM_LANG (GLM_LANG_CXX11 | GLM_MSC_EXT)
#		elif __INTEL_CXX11_MODE__
#			define GLM_LANG (GLM_LANG_CXX0X | GLM_MSC_EXT)
#		elif __cplusplus >= 199711L
#			define GLM_LANG (GLM_LANG_CXX98 | GLM_MSC_EXT)
#		else
#			define GLM_LANG (GLM_LANG_CXX | GLM_MSC_EXT)
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_CUDA
#		ifdef _MSC_EXTENSIONS
#			define GLM_MSC_EXT GLM_LANG_CXXMS_FLAG
#		else
#			define GLM_MSC_EXT 0
#		endif
#		if GLM_COMPILER >= GLM_COMPILER_CUDA75
#			define GLM_LANG (GLM_LANG_CXX0X | GLM_MSC_EXT)
#		else
#			define GLM_LANG (GLM_LANG_CXX98 | GLM_MSC_EXT)
#		endif
#	else // Unknown compiler
#		if __cplusplus >= 201402L
#			define GLM_LANG GLM_LANG_CXX14
#		elif __cplusplus >= 201103L
#			define GLM_LANG GLM_LANG_CXX11
#		elif __cplusplus >= 199711L
#			define GLM_LANG GLM_LANG_CXX98
#		else
#			define GLM_LANG GLM_LANG_CXX // Good luck with that!
#		endif
#		ifndef GLM_FORCE_PURE
#			define GLM_FORCE_PURE
#		endif
#	endif
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_LANG_DISPLAYED)
#	define GLM_MESSAGE_LANG_DISPLAYED

#	if GLM_LANG & GLM_LANG_CXX1Z_FLAG
#		pragma message("GLM: C++1z")
#	elif GLM_LANG & GLM_LANG_CXX14_FLAG
#		pragma message("GLM: C++14")
#	elif GLM_LANG & GLM_LANG_CXX1Y_FLAG
#		pragma message("GLM: C++1y")
#	elif GLM_LANG & GLM_LANG_CXX11_FLAG
#		pragma message("GLM: C++11")
#	elif GLM_LANG & GLM_LANG_CXX0X_FLAG
#		pragma message("GLM: C++0x")
#	elif GLM_LANG & GLM_LANG_CXX03_FLAG
#		pragma message("GLM: C++03")
#	elif GLM_LANG & GLM_LANG_CXX98_FLAG
#		pragma message("GLM: C++98")
#	else
#		pragma message("GLM: C++ language undetected")
#	endif//GLM_LANG

#	if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#		pragma message("GLM: Language extensions enabled")
#	endif//GLM_LANG
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Has of C++ features

// http://clang.llvm.org/cxx_status.html
// http://gcc.gnu.org/projects/cxx0x.html
// http://msdn.microsoft.com/en-us/library/vstudio/hh567368(v=vs.120).aspx

// Android has multiple STLs but C++11 STL detection doesn't always work #284 #564
#if GLM_PLATFORM == GLM_PLATFORM_ANDROID && !defined(GLM_LANG_STL11_FORCED)
#	define GLM_HAS_CXX11_STL 0
#elif GLM_COMPILER & GLM_COMPILER_CLANG
#	if (defined(_LIBCPP_VERSION) && GLM_LANG & GLM_LANG_CXX11_FLAG) || defined(GLM_LANG_STL11_FORCED)
#		define GLM_HAS_CXX11_STL 1
#	else
#		define GLM_HAS_CXX11_STL 0
#	endif
#else
#	define GLM_HAS_CXX11_STL ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC48)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_PLATFORM != GLM_PLATFORM_WINDOWS) && (GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL15))))
#endif

// N1720
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_STATIC_ASSERT __has_feature(cxx_static_assert)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_STATIC_ASSERT 1
#else
#	define GLM_HAS_STATIC_ASSERT ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA)) || \
		((GLM_COMPILER & GLM_COMPILER_VC))))
#endif

// N1988
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_EXTENDED_INTEGER_TYPE 1
#else
#	define GLM_HAS_EXTENDED_INTEGER_TYPE (\
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC11)) || \
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_CUDA)) || \
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_GCC)) || \
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_CLANG)))
#endif

// N2235
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_CONSTEXPR __has_feature(cxx_constexpr)
#	define GLM_HAS_CONSTEXPR_PARTIAL GLM_HAS_CONSTEXPR
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_CONSTEXPR 1
#	define GLM_HAS_CONSTEXPR_PARTIAL GLM_HAS_CONSTEXPR
#else
#	define GLM_HAS_CONSTEXPR ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15)) || \
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC48)))) // GCC 4.6 support constexpr but there is a compiler bug causing a crash
#	define GLM_HAS_CONSTEXPR_PARTIAL (GLM_HAS_CONSTEXPR || ((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14)))
#endif

// N2672
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_INITIALIZER_LISTS __has_feature(cxx_generalized_initializers)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_INITIALIZER_LISTS 1
#else
#	define GLM_HAS_INITIALIZER_LISTS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA75))))
#endif

// N2544 Unrestricted unions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2544.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_UNRESTRICTED_UNIONS __has_feature(cxx_unrestricted_unions)
#elif GLM_LANG & (GLM_LANG_CXX11_FLAG | GLM_LANG_CXXMS_FLAG)
#	define GLM_HAS_UNRESTRICTED_UNIONS 1
#else
#	define GLM_HAS_UNRESTRICTED_UNIONS (GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_LANG & GLM_LANG_CXXMS_FLAG)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA75)) || \
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC46)))
#endif

// N2346
#if defined(GLM_FORCE_UNRESTRICTED_GENTYPE)
#	define GLM_HAS_DEFAULTED_FUNCTIONS 0
#elif GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_DEFAULTED_FUNCTIONS __has_feature(cxx_defaulted_functions)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_DEFAULTED_FUNCTIONS 1
#else
#	define GLM_HAS_DEFAULTED_FUNCTIONS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL12)) || \
		(GLM_COMPILER & GLM_COMPILER_CUDA)))
#endif

// N2118
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_RVALUE_REFERENCES __has_feature(cxx_rvalue_references)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_RVALUE_REFERENCES 1
#else
#	define GLM_HAS_RVALUE_REFERENCES ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC11)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA50))))
#endif

// N2437 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2437.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS __has_feature(cxx_explicit_conversions)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS 1
#else
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC45)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL14)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA50))))
#endif

// N2258 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2258.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_TEMPLATE_ALIASES __has_feature(cxx_alias_templates)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_TEMPLATE_ALIASES 1
#else
#	define GLM_HAS_TEMPLATE_ALIASES ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL12_1)) || \
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC47)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA50))))
#endif

// N2930 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2930.html
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_RANGE_FOR __has_feature(cxx_range_for)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_RANGE_FOR 1
#else
#	define GLM_HAS_RANGE_FOR ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC46)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL13)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC11)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA50))))
#endif

// N2341 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2341.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_ALIGNOF __has_feature(c_alignof)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_ALIGNOF 1
#else
#	define GLM_HAS_ALIGNOF ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC48)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL15)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA70))))
#endif

#define GLM_HAS_ONLY_XYZW ((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER < GLM_COMPILER_GCC46))
#if GLM_HAS_ONLY_XYZW
#	pragma message("GLM: GCC older than 4.6 has a bug presenting the use of rgba and stpq components")
#endif

//
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_ASSIGNABLE 1
#else
#	define GLM_HAS_ASSIGNABLE ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15)) || \
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC49))))
#endif

//
#define GLM_HAS_TRIVIAL_QUERIES 0

//
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_MAKE_SIGNED 1
#else
#	define GLM_HAS_MAKE_SIGNED ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA50))))
#endif

#if GLM_ARCH == GLM_ARCH_PURE
#	define GLM_HAS_BITSCAN_WINDOWS 0
#else
#	define GLM_HAS_BITSCAN_WINDOWS ((GLM_PLATFORM & GLM_PLATFORM_WINDOWS) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14) && (GLM_ARCH & GLM_ARCH_X86_BIT))))
#endif

// OpenMP
#ifdef _OPENMP
#	if GLM_COMPILER & GLM_COMPILER_GCC
#		if GLM_COMPILER >= GLM_COMPILER_GCC61
#			define GLM_HAS_OPENMP 45
#		elif GLM_COMPILER >= GLM_COMPILER_GCC49
#			define GLM_HAS_OPENMP 40
#		elif GLM_COMPILER >= GLM_COMPILER_GCC47
#			define GLM_HAS_OPENMP 31
#		elif GLM_COMPILER >= GLM_COMPILER_GCC44
#			define GLM_HAS_OPENMP 30
#		elif GLM_COMPILER >= GLM_COMPILER_GCC42
#			define GLM_HAS_OPENMP 25
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_CLANG
#		if GLM_COMPILER >= GLM_COMPILER_CLANG38
#			define GLM_HAS_OPENMP 31
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_VC
#		if GLM_COMPILER >= GLM_COMPILER_VC10
#			define GLM_HAS_OPENMP 20
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#		if GLM_COMPILER >= GLM_COMPILER_INTEL16
#			define GLM_HAS_OPENMP 40
#		elif GLM_COMPILER >= GLM_COMPILER_INTEL12
#			define GLM_HAS_OPENMP 31
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	else
#		define GLM_HAS_OPENMP 0
#	endif// GLM_COMPILER & GLM_COMPILER_VC
#endif

///////////////////////////////////////////////////////////////////////////////////
// Static assert

#if GLM_HAS_STATIC_ASSERT
#	define GLM_STATIC_ASSERT(x, message) static_assert(x, message)
#elif defined(BOOST_STATIC_ASSERT)
#	define GLM_STATIC_ASSERT(x, message) BOOST_STATIC_ASSERT(x)
#elif GLM_COMPILER & GLM_COMPILER_VC
#	define GLM_STATIC_ASSERT(x, message) typedef char __CASSERT__##__LINE__[(x) ? 1 : -1]
#else
#	define GLM_STATIC_ASSERT(x, message)
#	define GLM_STATIC_ASSERT_NULL
#endif//GLM_LANG

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if GLM_COMPILER & GLM_COMPILER_CUDA
#	define GLM_CUDA_FUNC_DEF __device__ __host__
#	define GLM_CUDA_FUNC_DECL __device__ __host__
#else
#	define GLM_CUDA_FUNC_DEF
#	define GLM_CUDA_FUNC_DECL
#endif

#if GLM_COMPILER & GLM_COMPILER_GCC
#	define GLM_VAR_USED __attribute__ ((unused))
#else
#	define GLM_VAR_USED
#endif

#if defined(GLM_FORCE_INLINE)
#	if GLM_COMPILER & GLM_COMPILER_VC
#		define GLM_INLINE __forceinline
#		define GLM_NEVER_INLINE __declspec((noinline))
#	elif GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG)
#		define GLM_INLINE inline __attribute__((__always_inline__))
#		define GLM_NEVER_INLINE __attribute__((__noinline__))
#	elif GLM_COMPILER & GLM_COMPILER_CUDA
#		define GLM_INLINE __forceinline__
#		define GLM_NEVER_INLINE __noinline__
#	else
#		define GLM_INLINE inline
#		define GLM_NEVER_INLINE
#	endif//GLM_COMPILER
#else
#	define GLM_INLINE inline
#	define GLM_NEVER_INLINE
#endif//defined(GLM_FORCE_INLINE)

#define GLM_FUNC_DECL GLM_CUDA_FUNC_DECL
#define GLM_FUNC_QUALIFIER GLM_CUDA_FUNC_DEF GLM_INLINE

///////////////////////////////////////////////////////////////////////////////////
// Swizzle operators

// User defines: GLM_FORCE_SWIZZLE

#ifdef GLM_SWIZZLE
#	pragma message("GLM: GLM_SWIZZLE is deprecated, use GLM_FORCE_SWIZZLE instead")
#endif

#define GLM_SWIZZLE_ENABLED 1
#define GLM_SWIZZLE_DISABLE 0

#if defined(GLM_FORCE_SWIZZLE) || defined(GLM_SWIZZLE)
#	undef GLM_SWIZZLE
#	define GLM_SWIZZLE GLM_SWIZZLE_ENABLED
#else
#	undef GLM_SWIZZLE
#	define GLM_SWIZZLE GLM_SWIZZLE_DISABLE
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_SWIZZLE_DISPLAYED)
#	define GLM_MESSAGE_SWIZZLE_DISPLAYED
#	if GLM_SWIZZLE == GLM_SWIZZLE_ENABLED
#		pragma message("GLM: Swizzling operators enabled")
#	else
#		pragma message("GLM: Swizzling operators disabled, #define GLM_SWIZZLE to enable swizzle operators")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Allows using not basic types as genType

// #define GLM_FORCE_UNRESTRICTED_GENTYPE

#ifdef GLM_FORCE_UNRESTRICTED_GENTYPE
#	define GLM_UNRESTRICTED_GENTYPE 1
#else
#	define GLM_UNRESTRICTED_GENTYPE 0
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_UNRESTRICTED_GENTYPE_DISPLAYED)
#	define GLM_MESSAGE_UNRESTRICTED_GENTYPE_DISPLAYED
#	ifdef GLM_FORCE_UNRESTRICTED_GENTYPE
#		pragma message("GLM: Use unrestricted genType")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Clip control

#ifdef GLM_DEPTH_ZERO_TO_ONE // Legacy 0.9.8 development
#	error Define GLM_FORCE_DEPTH_ZERO_TO_ONE instead of GLM_DEPTH_ZERO_TO_ONE to use 0 to 1 clip space.
#endif

#define GLM_DEPTH_ZERO_TO_ONE				0x00000001
#define GLM_DEPTH_NEGATIVE_ONE_TO_ONE		0x00000002

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
#	define GLM_DEPTH_CLIP_SPACE GLM_DEPTH_ZERO_TO_ONE
#else
#	define GLM_DEPTH_CLIP_SPACE GLM_DEPTH_NEGATIVE_ONE_TO_ONE
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_DEPTH_DISPLAYED)
#	define GLM_MESSAGE_DEPTH_DISPLAYED
#	if GLM_DEPTH_CLIP_SPACE == GLM_DEPTH_ZERO_TO_ONE
#		pragma message("GLM: Depth clip space: Zero to one")
#	else
#		pragma message("GLM: Depth clip space: negative one to one")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Coordinate system, define GLM_FORCE_LEFT_HANDED before including GLM
// to use left handed coordinate system by default.

#ifdef GLM_LEFT_HANDED // Legacy 0.9.8 development
#	error Define GLM_FORCE_LEFT_HANDED instead of GLM_LEFT_HANDED left handed coordinate system by default.
#endif

#define GLM_LEFT_HANDED				0x00000001	// For DirectX, Metal, Vulkan
#define GLM_RIGHT_HANDED			0x00000002	// For OpenGL, default in GLM

#ifdef GLM_FORCE_LEFT_HANDED
#	define GLM_COORDINATE_SYSTEM GLM_LEFT_HANDED
#else
#	define GLM_COORDINATE_SYSTEM GLM_RIGHT_HANDED
#endif

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_HANDED_DISPLAYED)
#	define GLM_MESSAGE_HANDED_DISPLAYED
#	if GLM_COORDINATE_SYSTEM == GLM_LEFT_HANDED
#		pragma message("GLM: Coordinate system: left handed")
#	else
#		pragma message("GLM: Coordinate system: right handed")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if (GLM_COMPILER & GLM_COMPILER_VC) || ((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_PLATFORM & GLM_PLATFORM_WINDOWS))
#	define GLM_DEPRECATED __declspec(deprecated)
#	define GLM_ALIGN(x) __declspec(align(x))
#	define GLM_ALIGNED_STRUCT(x) struct __declspec(align(x))
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef __declspec(align(alignment)) type name
#	define GLM_RESTRICT_FUNC __declspec(restrict)
#	define GLM_RESTRICT __restrict
#	if GLM_COMPILER >= GLM_COMPILER_VC12
#		define GLM_VECTOR_CALL __vectorcall
#	else
#		define GLM_VECTOR_CALL
#	endif
#elif GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG | GLM_COMPILER_INTEL)
#	define GLM_DEPRECATED __attribute__((__deprecated__))
#	define GLM_ALIGN(x) __attribute__((aligned(x)))
#	define GLM_ALIGNED_STRUCT(x) struct __attribute__((aligned(x)))
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __attribute__((aligned(alignment)))
#	define GLM_RESTRICT_FUNC __restrict__
#	define GLM_RESTRICT __restrict__
#	if GLM_COMPILER & GLM_COMPILER_CLANG
#		if GLM_COMPILER >= GLM_COMPILER_CLANG37
#			define GLM_VECTOR_CALL __vectorcall
#		else
#			define GLM_VECTOR_CALL
#		endif
#	else
#		define GLM_VECTOR_CALL
#	endif
#elif GLM_COMPILER & GLM_COMPILER_CUDA
#	define GLM_DEPRECATED
#	define GLM_ALIGN(x) __align__(x)
#	define GLM_ALIGNED_STRUCT(x) struct __align__(x)
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __align__(x)
#	define GLM_RESTRICT_FUNC __restrict__
#	define GLM_RESTRICT __restrict__
#	define GLM_VECTOR_CALL
#else
#	define GLM_DEPRECATED
#	define GLM_ALIGN
#	define GLM_ALIGNED_STRUCT(x) struct
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name
#	define GLM_RESTRICT_FUNC
#	define GLM_RESTRICT
#	define GLM_VECTOR_CALL
#endif//GLM_COMPILER

#if GLM_HAS_DEFAULTED_FUNCTIONS
#	define GLM_DEFAULT = default
#	ifdef GLM_FORCE_NO_CTOR_INIT
#		define GLM_DEFAULT_CTOR = default
#	else
#		define GLM_DEFAULT_CTOR
#	endif
#else
#	define GLM_DEFAULT
#	define GLM_DEFAULT_CTOR
#endif

#if GLM_HAS_CONSTEXPR || GLM_HAS_CONSTEXPR_PARTIAL
#	define GLM_CONSTEXPR constexpr
#	if GLM_COMPILER & GLM_COMPILER_VC // Visual C++ has a bug #594 https://github.com/g-truc/glm/issues/594
#		define GLM_CONSTEXPR_CTOR
#	else
#		define GLM_CONSTEXPR_CTOR constexpr
#	endif
#else
#	define GLM_CONSTEXPR
#	define GLM_CONSTEXPR_CTOR
#endif

#if GLM_HAS_CONSTEXPR
#	define GLM_RELAXED_CONSTEXPR constexpr
#else
#	define GLM_RELAXED_CONSTEXPR const
#endif

#if GLM_ARCH == GLM_ARCH_PURE
#	define GLM_CONSTEXPR_SIMD GLM_CONSTEXPR_CTOR
#else
#	define GLM_CONSTEXPR_SIMD
#endif

#ifdef GLM_FORCE_EXPLICIT_CTOR
#	define GLM_EXPLICIT explicit
#else
#	define GLM_EXPLICIT
#endif

///////////////////////////////////////////////////////////////////////////////////

#define GLM_HAS_ALIGNED_TYPE GLM_HAS_UNRESTRICTED_UNIONS

///////////////////////////////////////////////////////////////////////////////////
// Length type

// User defines: GLM_FORCE_SIZE_T_LENGTH GLM_FORCE_SIZE_FUNC

namespace glm
{
	using std::size_t;
#	if defined(GLM_FORCE_SIZE_T_LENGTH)
		typedef size_t length_t;
#	else
		typedef int length_t;
#	endif
}//namespace glm

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_MESSAGE_FORCE_SIZE_T_LENGTH)
#	define GLM_MESSAGE_FORCE_SIZE_T_LENGTH
#	if defined GLM_FORCE_SIZE_T_LENGTH
#		pragma message("GLM: .length() returns glm::length_t, a typedef of std::size_t")
#	else
#		pragma message("GLM: .length() returns glm::length_t, a typedef of int following the GLSL specification")
#	endif
#endif//GLM_MESSAGES

///////////////////////////////////////////////////////////////////////////////////
// countof

#ifndef __has_feature
#	define __has_feature(x) 0 // Compatibility with non-clang compilers.
#endif

#if GLM_HAS_CONSTEXPR_PARTIAL
	namespace glm
	{
		template <typename T, std::size_t N>
		constexpr std::size_t countof(T const (&)[N])
		{
			return N;
		}
	}//namespace glm
#	define GLM_COUNTOF(arr) glm::countof(arr)
#elif defined(_MSC_VER)
#	define GLM_COUNTOF(arr) _countof(arr)
#else
#	define GLM_COUNTOF(arr) sizeof(arr) / sizeof(arr[0])
#endif

///////////////////////////////////////////////////////////////////////////////////
// Uninitialize constructors

namespace glm
{
	enum ctor{uninitialize};
}//namespace glm
