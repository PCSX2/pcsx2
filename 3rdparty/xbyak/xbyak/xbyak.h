#pragma once
#ifndef XBYAK_XBYAK_H_
#define XBYAK_XBYAK_H_
/*!
	@file xbyak.h
	@brief Xbyak ; JIT assembler for x86(IA32)/x64 by C++
	@author herumi
	@url https://github.com/herumi/xbyak
	@note modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause
*/
#if (not +0) && !defined(XBYAK_NO_OP_NAMES) // trick to detect whether 'not' is operator or not
	#define XBYAK_NO_OP_NAMES
#endif

#include <stdio.h> // for debug print
#include <assert.h>
#include <vector>
#include <string>
#include <algorithm>
#ifndef NDEBUG
#include <iostream>
#endif

// #define XBYAK_DISABLE_AVX512

#if !defined(XBYAK_USE_MMAP_ALLOCATOR) && !defined(XBYAK_DONT_USE_MMAP_ALLOCATOR)
	#define XBYAK_USE_MMAP_ALLOCATOR
#endif
#if !defined(__GNUC__) || defined(__MINGW32__)
	#undef XBYAK_USE_MMAP_ALLOCATOR
#endif

#ifdef __GNUC__
	#define XBYAK_GNUC_PREREQ(major, minor) ((__GNUC__) * 100 + (__GNUC_MINOR__) >= (major) * 100 + (minor))
#else
	#define XBYAK_GNUC_PREREQ(major, minor) 0
#endif

// User defined (must define all 3)
#if defined(XBYAK_STD_UNORDERED_SET) || defined(XBYAK_STD_UNORDERED_MAP) || defined(XBYAK_STD_UNORDERED_MULTIMAP)
	#ifndef XBYAK_STD_UNORDERED_SET
		#error "Define XBYAK_STD_UNORDERED_SET"
	#endif
	#ifndef XBYAK_STD_UNORDERED_MAP
		#error "Define XBYAK_STD_UNORDERED_MAP"
	#endif
	#ifndef XBYAK_STD_UNORDERED_MULTIMAP
		#error "Define XBYAK_STD_UNORDERED_MULTIMAP"
	#endif
// This covers -std=(gnu|c)++(0x|11|1y), -stdlib=libc++, and modern Microsoft.
#elif ((defined(_MSC_VER) && (_MSC_VER >= 1600)) || defined(_LIBCPP_VERSION) ||\
	 			 ((__cplusplus >= 201103) || defined(__GXX_EXPERIMENTAL_CXX0X__)))
	#include <unordered_set>
	#define XBYAK_STD_UNORDERED_SET std::unordered_set
	#include <unordered_map>
	#define XBYAK_STD_UNORDERED_MAP std::unordered_map
	#define XBYAK_STD_UNORDERED_MULTIMAP std::unordered_multimap

/*
	Clang/llvm-gcc and ICC-EDG in 'GCC-mode' always claim to be GCC 4.2, using
	libstdcxx 20070719 (from GCC 4.2.1, the last GPL 2 version).
*/
#elif XBYAK_GNUC_PREREQ(4, 5) || (XBYAK_GNUC_PREREQ(4, 2) && __GLIBCXX__ >= 20070719) || defined(__INTEL_COMPILER) || defined(__llvm__)
	#include <tr1/unordered_set>
	#define XBYAK_STD_UNORDERED_SET std::tr1::unordered_set
	#include <tr1/unordered_map>
	#define XBYAK_STD_UNORDERED_MAP std::tr1::unordered_map
	#define XBYAK_STD_UNORDERED_MULTIMAP std::tr1::unordered_multimap

#elif defined(_MSC_VER) && (_MSC_VER >= 1500) && (_MSC_VER < 1600)
	#include <unordered_set>
	#define XBYAK_STD_UNORDERED_SET std::tr1::unordered_set
	#include <unordered_map>
	#define XBYAK_STD_UNORDERED_MAP std::tr1::unordered_map
	#define XBYAK_STD_UNORDERED_MULTIMAP std::tr1::unordered_multimap

#else
	#include <set>
	#define XBYAK_STD_UNORDERED_SET std::set
	#include <map>
	#define XBYAK_STD_UNORDERED_MAP std::map
	#define XBYAK_STD_UNORDERED_MULTIMAP std::multimap
#endif
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <malloc.h>
	#ifdef _MSC_VER
		#define XBYAK_TLS __declspec(thread)
	#else
		#define XBYAK_TLS __thread
	#endif
#elif defined(__GNUC__)
	#include <unistd.h>
	#include <sys/mman.h>
	#include <stdlib.h>
	#define XBYAK_TLS __thread
#endif
#if defined(__APPLE__) && !defined(XBYAK_DONT_USE_MAP_JIT)
	#define XBYAK_USE_MAP_JIT
	#include <sys/sysctl.h>
	#ifndef MAP_JIT
		#define MAP_JIT 0x800
	#endif
#endif
#if !defined(_MSC_VER) || (_MSC_VER >= 1600)
	#include <stdint.h>
#endif

// MFD_CLOEXEC defined only linux 3.17 or later.
// Android wraps the memfd_create syscall from API version 30.
#if !defined(MFD_CLOEXEC) || (defined(__ANDROID__) && __ANDROID_API__ < 30)
	#undef XBYAK_USE_MEMFD
#endif

#if !defined(XBYAK64_WIN) && !defined(XBYAK64_GCC)
	#if defined(_WIN64) || defined(__MINGW64__) || (defined(__CYGWIN__) && defined(__x86_64__))
		#define XBYAK64_WIN
	#elif defined(__x86_64__)
		#define XBYAK64_GCC
	#endif
#endif
#if !defined(XBYAK64) && !defined(XBYAK32)
	#if defined(XBYAK64_GCC) || defined(XBYAK64_WIN)
		#define XBYAK64
	#else
		#define XBYAK32
	#endif
#endif

#if (__cplusplus >= 201103) || (defined(_MSC_VER) && _MSC_VER >= 1900)
	#undef XBYAK_TLS
	#define XBYAK_TLS thread_local
	#define XBYAK_VARIADIC_TEMPLATE
	#define XBYAK_NOEXCEPT noexcept
	#define XBYAK_OVERRIDE override
#else
	#define XBYAK_NOEXCEPT throw()
	#define XBYAK_OVERRIDE
#endif

// require c++14 or later
// Visual Studio 2017 version 15.0 or later
// g++-6 or later
#if ((__cplusplus >= 201402L) && !(!defined(__clang__) && defined(__GNUC__) && (__GNUC__ <= 5))) || (defined(_MSC_VER) && _MSC_VER >= 1910)
	#define XBYAK_CONSTEXPR constexpr
#else
	#define XBYAK_CONSTEXPR
#endif

#ifdef __cpp_inline_variables
	#define XBYAK_USE_CONSTEXPR_REGISTERS 1
#endif

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4514) /* remove inline function */
	#pragma warning(disable : 4786) /* identifier is too long */
	#pragma warning(disable : 4503) /* name is too long */
	#pragma warning(disable : 4127) /* constant expresison */
#endif

// disable -Warray-bounds because it may be a bug of gcc. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104603
#if defined(__GNUC__) && !defined(__clang__)
	#define XBYAK_DISABLE_WARNING_ARRAY_BOUNDS
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

// Heuristically check general-purpose register and memory operand size matching such as add(dword[eax], al);
// Not exact but useful to find mistakes. Define as 0 to disable.
// This macro may be removed in future versions.
#ifndef XBYAK_STRICT_CHECK_MEM_REG_SIZE
	#define XBYAK_STRICT_CHECK_MEM_REG_SIZE 1
#endif

// F is called: F(Type, name, init...)
#define XBYAK_FOR_EACH_REG_REG8(F)		\
	F(Reg8, al, Operand::AL)		\
	F(Reg8, cl, Operand::CL)		\
	F(Reg8, dl, Operand::DL)		\
	F(Reg8, bl, Operand::BL)		\
	F(Reg8, ah, Operand::AH)		\
	F(Reg8, ch, Operand::CH)		\
	F(Reg8, dh, Operand::DH)		\
	F(Reg8, bh, Operand::BH)		\
	/**/
#define XBYAK_FOR_EACH_REG_REG16(F)		\
	F(Reg16, ax, Operand::AX)		\
	F(Reg16, cx, Operand::CX)		\
	F(Reg16, dx, Operand::DX)		\
	F(Reg16, bx, Operand::BX)		\
	F(Reg16, sp, Operand::SP)		\
	F(Reg16, bp, Operand::BP)		\
	F(Reg16, si, Operand::SI)		\
	F(Reg16, di, Operand::DI)		\
	/**/
#define XBYAK_FOR_EACH_REG_REG32(F)		\
	F(Reg32, eax, Operand::EAX)		\
	F(Reg32, ecx, Operand::ECX)		\
	F(Reg32, edx, Operand::EDX)		\
	F(Reg32, ebx, Operand::EBX)		\
	F(Reg32, esp, Operand::ESP)		\
	F(Reg32, ebp, Operand::EBP)		\
	F(Reg32, esi, Operand::ESI)		\
	F(Reg32, edi, Operand::EDI)		\
	/**/
#define XBYAK_FOR_EACH_REG_MMX(F)		\
	F(Mmx, mm0, 0)				\
	F(Mmx, mm1, 1)				\
	F(Mmx, mm2, 2)				\
	F(Mmx, mm3, 3)				\
	F(Mmx, mm4, 4)				\
	F(Mmx, mm5, 5)				\
	F(Mmx, mm6, 6)				\
	F(Mmx, mm7, 7)				\
	/**/
#define XBYAK_FOR_EACH_REG_XMM(F)		\
	F(Xmm, xmm0, 0)				\
	F(Xmm, xmm1, 1)				\
	F(Xmm, xmm2, 2)				\
	F(Xmm, xmm3, 3)				\
	F(Xmm, xmm4, 4)				\
	F(Xmm, xmm5, 5)				\
	F(Xmm, xmm6, 6)				\
	F(Xmm, xmm7, 7)				\
	/**/
#define XBYAK_FOR_EACH_REG_YMM(F)		\
	F(Ymm, ymm0, 0)				\
	F(Ymm, ymm1, 1)				\
	F(Ymm, ymm2, 2)				\
	F(Ymm, ymm3, 3)				\
	F(Ymm, ymm4, 4)				\
	F(Ymm, ymm5, 5)				\
	F(Ymm, ymm6, 6)				\
	F(Ymm, ymm7, 7)				\
	/**/
#define XBYAK_FOR_EACH_REG_ZMM(F)		\
	F(Zmm, zmm0, 0)				\
	F(Zmm, zmm1, 1)				\
	F(Zmm, zmm2, 2)				\
	F(Zmm, zmm3, 3)				\
	F(Zmm, zmm4, 4)				\
	F(Zmm, zmm5, 5)				\
	F(Zmm, zmm6, 6)				\
	F(Zmm, zmm7, 7)				\
	/**/
// xword is same as oword of NASM
// ptr_b/xword_b/yword_b/zword_b are broadcast such as {1to2}, {1to4}, {1to8}, {1to16}, {b}
#define XBYAK_FOR_EACH_REG_ADDRESS(F)		\
	F(AddressFrame, ptr, 0)			\
	F(AddressFrame, byte, 8)		\
	F(AddressFrame, word, 16)		\
	F(AddressFrame, dword, 32)		\
	F(AddressFrame, qword, 64)		\
	F(AddressFrame, xword, 128)		\
	F(AddressFrame, yword, 256)		\
	F(AddressFrame, zword, 512)		\
	F(AddressFrame, ptr_b, 0, true)		\
	F(AddressFrame, xword_b, 128, true)	\
	F(AddressFrame, yword_b, 256, true)	\
	F(AddressFrame, zword_b, 512, true)	\
	/**/
#define XBYAK_FOR_EACH_REG_FPU(F)		\
	F(Fpu, st0, 0)				\
	F(Fpu, st1, 1)				\
	F(Fpu, st2, 2)				\
	F(Fpu, st3, 3)				\
	F(Fpu, st4, 4)				\
	F(Fpu, st5, 5)				\
	F(Fpu, st6, 6)				\
	F(Fpu, st7, 7)				\
	/**/
#define XBYAK_FOR_EACH_REG_OPMASK(F)		\
	F(Opmask, k0, 0)			\
	F(Opmask, k1, 1)			\
	F(Opmask, k2, 2)			\
	F(Opmask, k3, 3)			\
	F(Opmask, k4, 4)			\
	F(Opmask, k5, 5)			\
	F(Opmask, k6, 6)			\
	F(Opmask, k7, 7)			\
	/**/
#define XBYAK_FOR_EACH_REG_BOUNDS(F)		\
	F(BoundsReg, bnd0, 0)			\
	F(BoundsReg, bnd1, 1)			\
	F(BoundsReg, bnd2, 2)			\
	F(BoundsReg, bnd3, 3)			\
	/**/
// {sae}, {rn-sae}, {rd-sae}, {ru-sae}, {rz-sae} and {z}
#define XBYAK_FOR_EACH_EVEX_MODIFIER(F)					\
	F(EvexModifierRounding, T_sae, EvexModifierRounding::T_SAE)	\
	F(EvexModifierRounding, T_rn_sae, EvexModifierRounding::T_RN_SAE) \
	F(EvexModifierRounding, T_rd_sae, EvexModifierRounding::T_RD_SAE) \
	F(EvexModifierRounding, T_ru_sae, EvexModifierRounding::T_RU_SAE) \
	F(EvexModifierRounding, T_rz_sae, EvexModifierRounding::T_RZ_SAE) \
	F(EvexModifierZero, T_z, 0)					\
	/**/

#ifdef XBYAK64
#define XBYAK_FOR_EACH_REG_REG8_REX(F)		\
	F(Reg8, spl, Operand::SPL, true)	\
	F(Reg8, bpl, Operand::BPL, true)	\
	F(Reg8, sil, Operand::SIL, true)	\
	F(Reg8, dil, Operand::DIL, true)	\
	/**/
#define XBYAK_FOR_EACH_REG_REG8_EXT(F)		\
	F(Reg8, r8b, 8)				\
	F(Reg8, r9b, 9)				\
	F(Reg8, r10b, 10)			\
	F(Reg8, r11b, 11)			\
	F(Reg8, r12b, 12)			\
	F(Reg8, r13b, 13)			\
	F(Reg8, r14b, 14)			\
	F(Reg8, r15b, 15)			\
	F(Reg8, r16b, 16)			\
	F(Reg8, r17b, 17)			\
	F(Reg8, r18b, 18)			\
	F(Reg8, r19b, 19)			\
	F(Reg8, r20b, 20)			\
	F(Reg8, r21b, 21)			\
	F(Reg8, r22b, 22)			\
	F(Reg8, r23b, 23)			\
	F(Reg8, r24b, 24)			\
	F(Reg8, r25b, 25)			\
	F(Reg8, r26b, 26)			\
	F(Reg8, r27b, 27)			\
	F(Reg8, r28b, 28)			\
	F(Reg8, r29b, 29)			\
	F(Reg8, r30b, 30)			\
	F(Reg8, r31b, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_REG16_EXT(F)		\
	F(Reg16, r8w, 8)			\
	F(Reg16, r9w, 9)			\
	F(Reg16, r10w, 10)			\
	F(Reg16, r11w, 11)			\
	F(Reg16, r12w, 12)			\
	F(Reg16, r13w, 13)			\
	F(Reg16, r14w, 14)			\
	F(Reg16, r15w, 15)			\
	F(Reg16, r16w, 16)			\
	F(Reg16, r17w, 17)			\
	F(Reg16, r18w, 18)			\
	F(Reg16, r19w, 19)			\
	F(Reg16, r20w, 20)			\
	F(Reg16, r21w, 21)			\
	F(Reg16, r22w, 22)			\
	F(Reg16, r23w, 23)			\
	F(Reg16, r24w, 24)			\
	F(Reg16, r25w, 25)			\
	F(Reg16, r26w, 26)			\
	F(Reg16, r27w, 27)			\
	F(Reg16, r28w, 28)			\
	F(Reg16, r29w, 29)			\
	F(Reg16, r30w, 30)			\
	F(Reg16, r31w, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_REG32_EXT(F)		\
	F(Reg32, r8d, 8)			\
	F(Reg32, r9d, 9)			\
	F(Reg32, r10d, 10)			\
	F(Reg32, r11d, 11)			\
	F(Reg32, r12d, 12)			\
	F(Reg32, r13d, 13)			\
	F(Reg32, r14d, 14)			\
	F(Reg32, r15d, 15)			\
	F(Reg32, r16d, 16)			\
	F(Reg32, r17d, 17)			\
	F(Reg32, r18d, 18)			\
	F(Reg32, r19d, 19)			\
	F(Reg32, r20d, 20)			\
	F(Reg32, r21d, 21)			\
	F(Reg32, r22d, 22)			\
	F(Reg32, r23d, 23)			\
	F(Reg32, r24d, 24)			\
	F(Reg32, r25d, 25)			\
	F(Reg32, r26d, 26)			\
	F(Reg32, r27d, 27)			\
	F(Reg32, r28d, 28)			\
	F(Reg32, r29d, 29)			\
	F(Reg32, r30d, 30)			\
	F(Reg32, r31d, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_REG64(F)		\
	F(Reg64, rax, Operand::RAX)		\
	F(Reg64, rcx, Operand::RCX)		\
	F(Reg64, rdx, Operand::RDX)		\
	F(Reg64, rbx, Operand::RBX)		\
	F(Reg64, rsp, Operand::RSP)		\
	F(Reg64, rbp, Operand::RBP)		\
	F(Reg64, rsi, Operand::RSI)		\
	F(Reg64, rdi, Operand::RDI)		\
	F(Reg64, r8, Operand::R8)		\
	F(Reg64, r9, Operand::R9)		\
	F(Reg64, r10, Operand::R10)		\
	F(Reg64, r11, Operand::R11)		\
	F(Reg64, r12, Operand::R12)		\
	F(Reg64, r13, Operand::R13)		\
	F(Reg64, r14, Operand::R14)		\
	F(Reg64, r15, Operand::R15)		\
	F(Reg64, r16, 16)			\
	F(Reg64, r17, 17)			\
	F(Reg64, r18, 18)			\
	F(Reg64, r19, 19)			\
	F(Reg64, r20, 20)			\
	F(Reg64, r21, 21)			\
	F(Reg64, r22, 22)			\
	F(Reg64, r23, 23)			\
	F(Reg64, r24, 24)			\
	F(Reg64, r25, 25)			\
	F(Reg64, r26, 26)			\
	F(Reg64, r27, 27)			\
	F(Reg64, r28, 28)			\
	F(Reg64, r29, 29)			\
	F(Reg64, r30, 30)			\
	F(Reg64, r31, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_XMM_EXT(F)		\
	F(Xmm, xmm8, 8)				\
	F(Xmm, xmm9, 9)				\
	F(Xmm, xmm10, 10)			\
	F(Xmm, xmm11, 11)			\
	F(Xmm, xmm12, 12)			\
	F(Xmm, xmm13, 13)			\
	F(Xmm, xmm14, 14)			\
	F(Xmm, xmm15, 15)			\
	F(Xmm, xmm16, 16)			\
	F(Xmm, xmm17, 17)			\
	F(Xmm, xmm18, 18)			\
	F(Xmm, xmm19, 19)			\
	F(Xmm, xmm20, 20)			\
	F(Xmm, xmm21, 21)			\
	F(Xmm, xmm22, 22)			\
	F(Xmm, xmm23, 23)			\
	F(Xmm, xmm24, 24)			\
	F(Xmm, xmm25, 25)			\
	F(Xmm, xmm26, 26)			\
	F(Xmm, xmm27, 27)			\
	F(Xmm, xmm28, 28)			\
	F(Xmm, xmm29, 29)			\
	F(Xmm, xmm30, 30)			\
	F(Xmm, xmm31, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_YMM_EXT(F)		\
	F(Ymm, ymm8, 8)				\
	F(Ymm, ymm9, 9)				\
	F(Ymm, ymm10, 10)			\
	F(Ymm, ymm11, 11)			\
	F(Ymm, ymm12, 12)			\
	F(Ymm, ymm13, 13)			\
	F(Ymm, ymm14, 14)			\
	F(Ymm, ymm15, 15)			\
	F(Ymm, ymm16, 16)			\
	F(Ymm, ymm17, 17)			\
	F(Ymm, ymm18, 18)			\
	F(Ymm, ymm19, 19)			\
	F(Ymm, ymm20, 20)			\
	F(Ymm, ymm21, 21)			\
	F(Ymm, ymm22, 22)			\
	F(Ymm, ymm23, 23)			\
	F(Ymm, ymm24, 24)			\
	F(Ymm, ymm25, 25)			\
	F(Ymm, ymm26, 26)			\
	F(Ymm, ymm27, 27)			\
	F(Ymm, ymm28, 28)			\
	F(Ymm, ymm29, 29)			\
	F(Ymm, ymm30, 30)			\
	F(Ymm, ymm31, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_ZMM_EXT(F)		\
	F(Zmm, zmm8, 8)				\
	F(Zmm, zmm9, 9)				\
	F(Zmm, zmm10, 10)			\
	F(Zmm, zmm11, 11)			\
	F(Zmm, zmm12, 12)			\
	F(Zmm, zmm13, 13)			\
	F(Zmm, zmm14, 14)			\
	F(Zmm, zmm15, 15)			\
	F(Zmm, zmm16, 16)			\
	F(Zmm, zmm17, 17)			\
	F(Zmm, zmm18, 18)			\
	F(Zmm, zmm19, 19)			\
	F(Zmm, zmm20, 20)			\
	F(Zmm, zmm21, 21)			\
	F(Zmm, zmm22, 22)			\
	F(Zmm, zmm23, 23)			\
	F(Zmm, zmm24, 24)			\
	F(Zmm, zmm25, 25)			\
	F(Zmm, zmm26, 26)			\
	F(Zmm, zmm27, 27)			\
	F(Zmm, zmm28, 28)			\
	F(Zmm, zmm29, 29)			\
	F(Zmm, zmm30, 30)			\
	F(Zmm, zmm31, 31)			\
	/**/
#define XBYAK_FOR_EACH_REG_TMM(F)		\
	F(Tmm, tmm0, 0)				\
	F(Tmm, tmm1, 1)				\
	F(Tmm, tmm2, 2)				\
	F(Tmm, tmm3, 3)				\
	F(Tmm, tmm4, 4)				\
	F(Tmm, tmm5, 5)				\
	F(Tmm, tmm6, 6)				\
	F(Tmm, tmm7, 7)				\
	/**/
#define XBYAK_FOR_EACH_REG_BSR(F)		\
	F(Bsr, bsr0, 0)				\
	/**/
#define XBYAK_FOR_EACH_REG_RIP(F)		\
	F(RegRip, rip, 0)			\
	/**/
#else
#define XBYAK_FOR_EACH_REG_REG8_REX(F)
#define XBYAK_FOR_EACH_REG_REG8_EXT(F)
#define XBYAK_FOR_EACH_REG_REG16_EXT(F)
#define XBYAK_FOR_EACH_REG_REG32_EXT(F)
#define XBYAK_FOR_EACH_REG_REG64(F)
#define XBYAK_FOR_EACH_REG_XMM_EXT(F)
#define XBYAK_FOR_EACH_REG_YMM_EXT(F)
#define XBYAK_FOR_EACH_REG_ZMM_EXT(F)
#define XBYAK_FOR_EACH_REG_TMM(F)
#define XBYAK_FOR_EACH_REG_BSR(F)
#define XBYAK_FOR_EACH_REG_RIP(F)
#endif

// T_nf/T_zu are used by cfcmov etc. even on 32-bit, so they are defined regardless of XBYAK64
#define XBYAK_FOR_EACH_APX_MODIFIER(F)		\
	F(ApxFlagNF, T_nf, 0)			\
	F(ApxFlagZU, T_zu, 0)			\
	/**/

#ifdef XBYAK_DISABLE_SEGMENT
#define XBYAK_FOR_EACH_REG_SEGMENT(F)
#else
#define XBYAK_FOR_EACH_REG_SEGMENT(F)		\
	F(Segment, es, Segment::es)		\
	F(Segment, cs, Segment::cs)		\
	F(Segment, ss, Segment::ss)		\
	F(Segment, ds, Segment::ds)		\
	F(Segment, fs, Segment::fs)		\
	F(Segment, gs, Segment::gs)		\
	/**/
#endif

#define XBYAK_FOR_EACH_REGISTER(F)		\
	XBYAK_FOR_EACH_REG_REG8(F)		\
	XBYAK_FOR_EACH_REG_REG16(F)		\
	XBYAK_FOR_EACH_REG_REG32(F)		\
	XBYAK_FOR_EACH_REG_MMX(F)		\
	XBYAK_FOR_EACH_REG_XMM(F)		\
	XBYAK_FOR_EACH_REG_YMM(F)		\
	XBYAK_FOR_EACH_REG_ZMM(F)		\
	XBYAK_FOR_EACH_REG_ADDRESS(F)		\
	XBYAK_FOR_EACH_REG_FPU(F)		\
	XBYAK_FOR_EACH_REG_OPMASK(F)		\
	XBYAK_FOR_EACH_REG_BOUNDS(F)		\
	XBYAK_FOR_EACH_EVEX_MODIFIER(F)		\
	XBYAK_FOR_EACH_APX_MODIFIER(F)		\
	XBYAK_FOR_EACH_REG_REG8_REX(F)		\
	XBYAK_FOR_EACH_REG_REG8_EXT(F)		\
	XBYAK_FOR_EACH_REG_REG16_EXT(F)		\
	XBYAK_FOR_EACH_REG_REG32_EXT(F)		\
	XBYAK_FOR_EACH_REG_REG64(F)		\
	XBYAK_FOR_EACH_REG_XMM_EXT(F)		\
	XBYAK_FOR_EACH_REG_YMM_EXT(F)		\
	XBYAK_FOR_EACH_REG_ZMM_EXT(F)		\
	XBYAK_FOR_EACH_REG_TMM(F)		\
	XBYAK_FOR_EACH_REG_BSR(F)		\
	XBYAK_FOR_EACH_REG_RIP(F)		\
	XBYAK_FOR_EACH_REG_SEGMENT(F)		\
	/**/

// xmN/ymN/zmN are aliases of xmmN/ymmN/zmmN (for my convenience)
#define XBYAK_FOR_EACH_REG_CONVENIENCE(F)	\
	F(Xmm, xm0, xmm0)			\
	F(Xmm, xm1, xmm1)			\
	F(Xmm, xm2, xmm2)			\
	F(Xmm, xm3, xmm3)			\
	F(Xmm, xm4, xmm4)			\
	F(Xmm, xm5, xmm5)			\
	F(Xmm, xm6, xmm6)			\
	F(Xmm, xm7, xmm7)			\
	F(Ymm, ym0, ymm0)			\
	F(Ymm, ym1, ymm1)			\
	F(Ymm, ym2, ymm2)			\
	F(Ymm, ym3, ymm3)			\
	F(Ymm, ym4, ymm4)			\
	F(Ymm, ym5, ymm5)			\
	F(Ymm, ym6, ymm6)			\
	F(Ymm, ym7, ymm7)			\
	F(Zmm, zm0, zmm0)			\
	F(Zmm, zm1, zmm1)			\
	F(Zmm, zm2, zmm2)			\
	F(Zmm, zm3, zmm3)			\
	F(Zmm, zm4, zmm4)			\
	F(Zmm, zm5, zmm5)			\
	F(Zmm, zm6, zmm6)			\
	F(Zmm, zm7, zmm7)			\
	/**/
#ifdef XBYAK64
#define XBYAK_FOR_EACH_REG_CONVENIENCE_EXT(F)	\
	F(Xmm, xm8, xmm8)			\
	F(Xmm, xm9, xmm9)			\
	F(Xmm, xm10, xmm10)			\
	F(Xmm, xm11, xmm11)			\
	F(Xmm, xm12, xmm12)			\
	F(Xmm, xm13, xmm13)			\
	F(Xmm, xm14, xmm14)			\
	F(Xmm, xm15, xmm15)			\
	F(Xmm, xm16, xmm16)			\
	F(Xmm, xm17, xmm17)			\
	F(Xmm, xm18, xmm18)			\
	F(Xmm, xm19, xmm19)			\
	F(Xmm, xm20, xmm20)			\
	F(Xmm, xm21, xmm21)			\
	F(Xmm, xm22, xmm22)			\
	F(Xmm, xm23, xmm23)			\
	F(Xmm, xm24, xmm24)			\
	F(Xmm, xm25, xmm25)			\
	F(Xmm, xm26, xmm26)			\
	F(Xmm, xm27, xmm27)			\
	F(Xmm, xm28, xmm28)			\
	F(Xmm, xm29, xmm29)			\
	F(Xmm, xm30, xmm30)			\
	F(Xmm, xm31, xmm31)			\
	F(Ymm, ym8, ymm8)			\
	F(Ymm, ym9, ymm9)			\
	F(Ymm, ym10, ymm10)			\
	F(Ymm, ym11, ymm11)			\
	F(Ymm, ym12, ymm12)			\
	F(Ymm, ym13, ymm13)			\
	F(Ymm, ym14, ymm14)			\
	F(Ymm, ym15, ymm15)			\
	F(Ymm, ym16, ymm16)			\
	F(Ymm, ym17, ymm17)			\
	F(Ymm, ym18, ymm18)			\
	F(Ymm, ym19, ymm19)			\
	F(Ymm, ym20, ymm20)			\
	F(Ymm, ym21, ymm21)			\
	F(Ymm, ym22, ymm22)			\
	F(Ymm, ym23, ymm23)			\
	F(Ymm, ym24, ymm24)			\
	F(Ymm, ym25, ymm25)			\
	F(Ymm, ym26, ymm26)			\
	F(Ymm, ym27, ymm27)			\
	F(Ymm, ym28, ymm28)			\
	F(Ymm, ym29, ymm29)			\
	F(Ymm, ym30, ymm30)			\
	F(Ymm, ym31, ymm31)			\
	F(Zmm, zm8, zmm8)			\
	F(Zmm, zm9, zmm9)			\
	F(Zmm, zm10, zmm10)			\
	F(Zmm, zm11, zmm11)			\
	F(Zmm, zm12, zmm12)			\
	F(Zmm, zm13, zmm13)			\
	F(Zmm, zm14, zmm14)			\
	F(Zmm, zm15, zmm15)			\
	F(Zmm, zm16, zmm16)			\
	F(Zmm, zm17, zmm17)			\
	F(Zmm, zm18, zmm18)			\
	F(Zmm, zm19, zmm19)			\
	F(Zmm, zm20, zmm20)			\
	F(Zmm, zm21, zmm21)			\
	F(Zmm, zm22, zmm22)			\
	F(Zmm, zm23, zmm23)			\
	F(Zmm, zm24, zmm24)			\
	F(Zmm, zm25, zmm25)			\
	F(Zmm, zm26, zmm26)			\
	F(Zmm, zm27, zmm27)			\
	F(Zmm, zm28, zmm28)			\
	F(Zmm, zm29, zmm29)			\
	F(Zmm, zm30, zmm30)			\
	F(Zmm, zm31, zmm31)			\
	/**/
#else
#define XBYAK_FOR_EACH_REG_CONVENIENCE_EXT(F)
#endif
#define XBYAK_FOR_EACH_CONVENIENCE_ALL(F)	\
	XBYAK_FOR_EACH_REG_CONVENIENCE(F)	\
	XBYAK_FOR_EACH_REG_CONVENIENCE_EXT(F)	\
	/**/

namespace Xbyak {

enum {
	DEFAULT_MAX_CODE_SIZE = 4096,
	VERSION = 0x7411 /* 0xABCD = A.BC(.D) */
};

#ifndef MIE_INTEGER_TYPE_DEFINED
#define MIE_INTEGER_TYPE_DEFINED
// for backward compatibility
typedef uint64_t uint64;
typedef int64_t sint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
#endif

#ifndef MIE_ALIGN
	#ifdef _MSC_VER
		#define MIE_ALIGN(x) __declspec(align(x))
	#else
		#define MIE_ALIGN(x) __attribute__((aligned(x)))
	#endif
#endif
#ifndef MIE_PACK // for shufps
	#define MIE_PACK(x, y, z, w) ((x) * 64 + (y) * 16 + (z) * 4 + (w))
#endif

// (error code, error message) pairs. The enum and the string table of ConvertErrorToString() are generated from this list.
#define XBYAK_ERR_LIST(f) \
	f(ERR_NONE, "none") \
	f(ERR_BAD_ADDRESSING, "bad addressing") \
	f(ERR_CODE_IS_TOO_BIG, "code is too big") \
	f(ERR_BAD_SCALE, "bad scale") \
	f(ERR_ESP_CANT_BE_INDEX, "esp can't be index") \
	f(ERR_BAD_COMBINATION, "bad combination") \
	f(ERR_BAD_SIZE_OF_REGISTER, "bad size of register") \
	f(ERR_IMM_IS_TOO_BIG, "imm is too big") \
	f(ERR_BAD_ALIGN, "bad align") \
	f(ERR_LABEL_IS_REDEFINED, "label is redefined") \
	f(ERR_LABEL_IS_TOO_FAR, "label is too far") \
	f(ERR_LABEL_IS_NOT_FOUND, "label is not found") \
	f(ERR_CODE_ISNOT_COPYABLE, "code is not copyable") \
	f(ERR_BAD_PARAMETER, "bad parameter") \
	f(ERR_CANT_PROTECT, "can't protect") \
	f(ERR_CANT_USE_64BIT_DISP, "can't use 64bit disp(use (void*))") \
	f(ERR_OFFSET_IS_TOO_BIG, "offset is too big") \
	f(ERR_MEM_SIZE_IS_NOT_SPECIFIED, "MEM size is not specified") \
	f(ERR_BAD_MEM_SIZE, "bad mem size") \
	f(ERR_BAD_ST_COMBINATION, "bad st combination") \
	f(ERR_OVER_LOCAL_LABEL, "over local label") /* not used */ \
	f(ERR_UNDER_LOCAL_LABEL, "under local label") \
	f(ERR_CANT_ALLOC, "can't alloc") \
	f(ERR_ONLY_T_NEAR_IS_SUPPORTED_IN_AUTO_GROW, "T_SHORT is not supported in AutoGrow") \
	f(ERR_BAD_PROTECT_MODE, "bad protect mode") \
	f(ERR_BAD_PNUM, "bad pNum") \
	f(ERR_BAD_TNUM, "bad tNum") \
	f(ERR_BAD_VSIB_ADDRESSING, "bad vsib addressing") \
	f(ERR_CANT_CONVERT, "can't convert") \
	f(ERR_LABEL_ISNOT_SET_BY_L, "label is not set by L()") \
	f(ERR_LABEL_IS_ALREADY_SET_BY_L, "label is already set by L()") \
	f(ERR_BAD_LABEL_STR, "bad label string") \
	f(ERR_MUNMAP, "err munmap") \
	f(ERR_OPMASK_IS_ALREADY_SET, "opmask is already set") \
	f(ERR_ROUNDING_IS_ALREADY_SET, "rounding is already set") \
	f(ERR_K0_IS_INVALID, "k0 is invalid") \
	f(ERR_EVEX_IS_INVALID, "evex is invalid") \
	f(ERR_SAE_IS_INVALID, "sae(suppress all exceptions) is invalid") \
	f(ERR_ER_IS_INVALID, "er(embedded rounding) is invalid") \
	f(ERR_INVALID_BROADCAST, "invalid broadcast") \
	f(ERR_INVALID_OPMASK_WITH_MEMORY, "invalid opmask with memory") \
	f(ERR_INVALID_ZERO, "invalid zero") \
	f(ERR_INVALID_RIP_IN_AUTO_GROW, "invalid rip in AutoGrow") \
	f(ERR_INVALID_MIB_ADDRESS, "invalid mib address") \
	f(ERR_X2APIC_IS_NOT_SUPPORTED, "x2APIC is not supported") \
	f(ERR_NOT_SUPPORTED, "not supported") \
	f(ERR_SAME_REGS_ARE_INVALID, "same regs are invalid") \
	f(ERR_INVALID_NF, "invalid NF") \
	f(ERR_INVALID_ZU, "invalid ZU") \
	f(ERR_CANT_USE_REX2, "can't use rex2") \
	f(ERR_INVALID_DFV, "invalid dfv") \
	f(ERR_INVALID_REG_IDX, "invalid reg index") \
	f(ERR_BAD_ENCODING_MODE, "bad encoding mode") \
	f(ERR_CANT_USE_ABCDH, "can't use [abcd]h with rex") \
	f(ERR_CANT_INIT_CPUTOPOLOGY, "can't init CpuTopology") \
	f(ERR_INVALID_CPUMASK_INDEX, "invalid cpumask index") \
	f(ERR_INTERNAL, "internal error") /* Put it at last. */

enum {
#define XBYAK_ERR_DEF(name, msg) name,
	XBYAK_ERR_LIST(XBYAK_ERR_DEF)
#undef XBYAK_ERR_DEF
	ERR_MAX_ // sentinel to avoid a trailing comma in C++03 (= ERR_INTERNAL + 1)
};

inline const char *ConvertErrorToString(int err)
{
	static const char *errTbl[] = {
#define XBYAK_ERR_DEF(name, msg) msg,
		XBYAK_ERR_LIST(XBYAK_ERR_DEF)
#undef XBYAK_ERR_DEF
	};
	return err <= ERR_INTERNAL ? errTbl[err] : "unknown err";
}
#undef XBYAK_ERR_LIST

#ifdef XBYAK_NO_EXCEPTION
namespace local {

inline int& GetErrorRef() {
	static XBYAK_TLS int err = 0;
	return err;
}

inline void SetError(int err) {
	if (local::GetErrorRef()) return; // keep the first err code
	local::GetErrorRef() = err;
}

} // local

inline void ClearError() {
	local::GetErrorRef() = 0;
}
inline int GetError() { return Xbyak::local::GetErrorRef(); }

#define XBYAK_THROW(err) { Xbyak::local::SetError(err); return; }
#define XBYAK_THROW_RET(err, r) { Xbyak::local::SetError(err); return r; }

#else
class Error : public std::exception {
	int err_;
public:
	explicit Error(int err) : err_(err)
	{
		if (err_ < 0 || err_ > ERR_INTERNAL) {
			err_ = ERR_INTERNAL;
		}
	}
	operator int() const { return err_; }
	const char *what() const XBYAK_NOEXCEPT XBYAK_OVERRIDE
	{
		return ConvertErrorToString(err_);
	}
};

// dummy functions
inline void ClearError() { }
inline int GetError() { return 0; }

inline const char *ConvertErrorToString(const Error& err)
{
	return err.what();
}

#define XBYAK_THROW(err) { throw Error(err); }
#define XBYAK_THROW_RET(err, r) { throw Error(err); }

#endif

inline void *AlignedMalloc(size_t size, size_t alignment)
{
#ifdef __MINGW32__
	return __mingw_aligned_malloc(size, alignment);
#elif defined(_WIN32)
	return _aligned_malloc(size, alignment);
#else
	void *p;
	int ret = posix_memalign(&p, alignment, size);
	return (ret == 0) ? p : 0;
#endif
}

inline void AlignedFree(void *p)
{
#ifdef __MINGW32__
	__mingw_aligned_free(p);
#elif defined(_MSC_VER)
	_aligned_free(p);
#else
	free(p);
#endif
}

namespace inner {

#ifdef _WIN32
struct SystemInfo {
	SYSTEM_INFO info;
	SystemInfo()
	{
		GetSystemInfo(&info);
	}
};
#endif
//static const size_t ALIGN_PAGE_SIZE = 4096;
inline size_t getPageSize()
{
#ifdef _WIN32
	static const SystemInfo si;
	return si.info.dwPageSize;
#else
#ifdef __GNUC__
	static const long pageSize = sysconf(_SC_PAGESIZE);
	if (pageSize > 0) {
		return (size_t)pageSize;
	}
#endif
	return 4096;
#endif
}

inline bool IsInDisp8(uint32_t x) { return 0xFFFFFF80 <= x || x <= 0x7F; }
inline bool IsInInt32(uint64_t x) { return ~uint64_t(0x7fffffffu) <= x || x <= 0x7FFFFFFFU; }

inline uint32_t VerifyInInt32(uint64_t x)
{
#if defined(XBYAK64) && !defined(__ILP32__)
	if (!IsInInt32(x)) XBYAK_THROW_RET(ERR_OFFSET_IS_TOO_BIG, 0)
#endif
	return static_cast<uint32_t>(x);
}

enum LabelMode {
	LasIs, // as is
	Labs, // absolute address (not used with AutoGrow)
	LaddTop, // (addr + top) for mov(reg, label) with AutoGrow
	LsubTop // (addr - top) for jmp/call to an absolute address with AutoGrow
};

enum AddressMode {
	M_none,
	M_ModRM,
	M_64bitDisp,
	M_rip,
	M_ripAddr
};

} // inner

/*
	custom allocator
	alloc() must allocate the buffer with size rounded up to a multiple of
	the page size because protect() works at page granularity.
*/
struct Allocator {
	explicit Allocator(const std::string& = "") {} // same interface with MmapAllocator
	virtual uint8_t *alloc(size_t size)
	{
		const size_t alignedSizeM1 = inner::getPageSize() - 1;
		size = (size + alignedSizeM1) & ~alignedSizeM1;
		return reinterpret_cast<uint8_t*>(AlignedMalloc(size, inner::getPageSize()));
	}
	virtual void free(uint8_t *p) { AlignedFree(p); }
	virtual ~Allocator() {}
	/* override to return false if you call protect() manually */
	virtual bool useProtect() const { return true; }
};

#ifdef XBYAK_USE_MMAP_ALLOCATOR
#ifdef XBYAK_USE_MAP_JIT
namespace util {

inline int getMacOsVersionPure()
{
	char buf[64];
	size_t size = sizeof(buf);
	int err = sysctlbyname("kern.osrelease", buf, &size, NULL, 0);
	if (err != 0) return 0;
	char *endp;
	int major = strtol(buf, &endp, 10);
	if (*endp != '.') return 0;
	return major;
}

inline int getMacOsVersion()
{
	static const int version = getMacOsVersionPure();
	return version;
}

} // util
#endif
class MmapAllocator : public Allocator {
	struct Allocation {
		uintptr_t addr;
		size_t size;
#if defined(XBYAK_USE_MEMFD)
		// fd_ is only used with XBYAK_USE_MEMFD. We keep the file open
		// during the lifetime of each allocation in order to support
		// checkpoint/restore by unprivileged users.
		int fd;
#endif
	};
	const std::string name_; // only used with XBYAK_USE_MEMFD
	typedef std::vector<Allocation> AllocationList;
	AllocationList allocList_;
public:
	explicit MmapAllocator(const std::string& name = "xbyak") : name_(name) {}
	uint8_t *alloc(size_t size) XBYAK_OVERRIDE
	{
		const size_t alignedSizeM1 = inner::getPageSize() - 1;
		size = (size + alignedSizeM1) & ~alignedSizeM1;
#if defined(MAP_ANONYMOUS)
		int mode = MAP_PRIVATE | MAP_ANONYMOUS;
#elif defined(MAP_ANON)
		int mode = MAP_PRIVATE | MAP_ANON;
#else
		#error "not supported"
#endif
#if defined(XBYAK_USE_MAP_JIT)
		const int mojaveVersion = 18;
		if (util::getMacOsVersion() >= mojaveVersion) mode |= MAP_JIT;
#endif
		int fd = -1;
#if defined(XBYAK_USE_MEMFD)
		fd = memfd_create(name_.c_str(), MFD_CLOEXEC);
		if (fd != -1) {
			mode = MAP_SHARED;
			if (ftruncate(fd, size) != 0) {
				close(fd);
				XBYAK_THROW_RET(ERR_CANT_ALLOC, 0)
			}
		}
#endif
		int prot = PROT_READ | PROT_WRITE;
#ifdef PROT_MPROTECT
		// Some NetBSD systems have this protection turned on by default
		// https://man.netbsd.org/mprotect.2
		prot |= PROT_MPROTECT(PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
		void *p = mmap(NULL, size, prot, mode, fd, 0);
		if (p == MAP_FAILED) {
			if (fd != -1) close(fd);
			XBYAK_THROW_RET(ERR_CANT_ALLOC, 0)
		}
		assert(p);
		Allocation alloc;
		alloc.addr = (uintptr_t)p;
		alloc.size = size;
#if defined(XBYAK_USE_MEMFD)
		alloc.fd = fd;
#endif
		allocList_.push_back(alloc);
		return (uint8_t*)p;
	}
	void free(uint8_t *p) XBYAK_OVERRIDE
	{
		if (p == 0) return;
		for (size_t idx = 0; idx < allocList_.size(); idx++) {
			Allocation& a = allocList_[idx];
			if (a.addr != (uintptr_t)p) continue;
			if (munmap((void*)a.addr, a.size) < 0) XBYAK_THROW(ERR_MUNMAP)
#if defined(XBYAK_USE_MEMFD)
			if (a.fd != -1) close(a.fd);
#endif
			a = allocList_.back();
			allocList_.pop_back();
			return;
		}
		XBYAK_THROW(ERR_BAD_PARAMETER)
	}
};
#else
typedef Allocator MmapAllocator;
#endif

class Address;
class Reg;

struct ApxFlagNF { explicit XBYAK_CONSTEXPR ApxFlagNF(int = 0) {} };
struct ApxFlagZU { explicit XBYAK_CONSTEXPR ApxFlagZU(int = 0) {} };

// dfv (default flags value) is or operation of these flags
static const int T_of = 8;
static const int T_sf = 4;
static const int T_zf = 2;
static const int T_cf = 1;

class Operand {
	static const uint8_t EXT8BIT = 0x20;
	unsigned int idx_:6; // 0..31 + EXT8BIT = 1 if spl/bpl/sil/dil
	unsigned int kind_:11;
	unsigned int bit_:14;
protected:
	unsigned int zero_:1;
	unsigned int mask_:3;
	unsigned int rounding_:3;
	unsigned int NF_:1;
	unsigned int ZU_:1; // ND=ZU
	void setIdx(int idx) { idx_ = idx; }
public:
	enum Kind {
		NONE = 0,
		MEM = 1 << 0,
		REG = 1 << 1,
		MMX = 1 << 2,
		FPU = 1 << 3,
		XMM = 1 << 4,
		YMM = 1 << 5,
		ZMM = 1 << 6,
		OPMASK = 1 << 7,
		BNDREG = 1 << 8,
		TMM = 1 << 9,
		BSR = 1 << 10
	};
	enum Code {
#ifdef XBYAK64
		RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
		R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, R29, R30, R31,
		R8D = 8, R9D, R10D, R11D, R12D, R13D, R14D, R15D,
		R16D, R17D, R18D, R19D, R20D, R21D, R22D, R23D, R24D, R25D, R26D, R27D, R28D, R29D, R30D, R31D,
		R8W = 8, R9W, R10W, R11W, R12W, R13W, R14W, R15W,
		R16W, R17W, R18W, R19W, R20W, R21W, R22W, R23W, R24W, R25W, R26W, R27W, R28W, R29W, R30W, R31W,
		R8B = 8, R9B, R10B, R11B, R12B, R13B, R14B, R15B,
		R16B, R17B, R18B, R19B, R20B, R21B, R22B, R23B, R24B, R25B, R26B, R27B, R28B, R29B, R30B, R31B,
		SPL = 4, BPL, SIL, DIL,
#endif
		EAX = 0, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
		AX = 0, CX, DX, BX, SP, BP, SI, DI,
		AL = 0, CL, DL, BL, AH, CH, DH, BH
	};
	XBYAK_CONSTEXPR Operand() : idx_(0), kind_(0), bit_(0), zero_(0), mask_(0), rounding_(0), NF_(0), ZU_(0) { }
	XBYAK_CONSTEXPR Operand(int idx, Kind kind, int bit, bool ext8bit = 0)
		: idx_(static_cast<uint8_t>(idx | (ext8bit ? EXT8BIT : 0)))
		, kind_(kind)
		, bit_(bit)
		, zero_(0), mask_(0), rounding_(0), NF_(0), ZU_(0)
	{
#ifdef XBYAK32
		if (idx >= 8) XBYAK_THROW(ERR_INVALID_REG_IDX)
#endif
		assert((bit_ & (bit_ - 1)) == 0); // bit must be power of two
	}
	XBYAK_CONSTEXPR Kind getKind() const { return static_cast<Kind>(kind_); }
	XBYAK_CONSTEXPR int getIdx() const { return idx_ & (EXT8BIT - 1); }
	XBYAK_CONSTEXPR bool hasIdxBit(int bit) const { return idx_ & (1<<bit); }
	XBYAK_CONSTEXPR bool isNone() const { return kind_ == 0; }
	XBYAK_CONSTEXPR bool isMMX() const { return is(MMX); }
	XBYAK_CONSTEXPR bool isXMM() const { return is(XMM); }
	XBYAK_CONSTEXPR bool isYMM() const { return is(YMM); }
	XBYAK_CONSTEXPR bool isZMM() const { return is(ZMM); }
	XBYAK_CONSTEXPR bool isSIMD() const { return is(XMM|YMM|ZMM); }
	XBYAK_CONSTEXPR bool isTMM() const { return is(TMM); }
	XBYAK_CONSTEXPR bool isBSR() const { return is(BSR); }
	XBYAK_CONSTEXPR bool isXMEM() const { return is(XMM | MEM); }
	XBYAK_CONSTEXPR bool isYMEM() const { return is(YMM | MEM); }
	XBYAK_CONSTEXPR bool isZMEM() const { return is(ZMM | MEM); }
	XBYAK_CONSTEXPR bool isOPMASK() const { return is(OPMASK); }
	XBYAK_CONSTEXPR bool isBNDREG() const { return is(BNDREG); }
	XBYAK_CONSTEXPR bool isREG(int bit = 0) const { return is(REG, bit); }
	XBYAK_CONSTEXPR bool isMEM(int bit = 0) const { return is(MEM, bit); }
	XBYAK_CONSTEXPR bool isFPU() const { return is(FPU); }
	XBYAK_CONSTEXPR bool isExt8bit() const { return (idx_ & EXT8BIT) != 0; }
	XBYAK_CONSTEXPR bool isExtIdx() const { return (getIdx() & 8) != 0; }
	XBYAK_CONSTEXPR bool isExtIdx2() const { return (getIdx() & 16) != 0; }
	XBYAK_CONSTEXPR bool hasEvex() const { return isZMM() || isExtIdx2() || getOpmaskIdx() || getRounding(); }
	XBYAK_CONSTEXPR bool hasRex() const { return isExt8bit() || isREG(64) || isExtIdx(); }
	XBYAK_CONSTEXPR bool hasRex2() const;
	XBYAK_CONSTEXPR bool hasRex2NF() const { return hasRex2() || NF_; }
	XBYAK_CONSTEXPR bool hasRex2NFZU() const { return hasRex2() || NF_ || ZU_; }
	XBYAK_CONSTEXPR bool hasZero() const { return zero_; }
	XBYAK_CONSTEXPR int getOpmaskIdx() const { return mask_; }
	XBYAK_CONSTEXPR int getRounding() const { return rounding_; }
	void setKind(Kind kind)
	{
		if ((kind & (XMM|YMM|ZMM|TMM)) == 0) return;
		kind_ = kind;
		bit_ = kind == XMM ? 128 : kind == YMM ? 256 : kind == ZMM ? 512 : 8192;
	}
	// err if MMX/FPU/OPMASK/BNDREG
	void setBit(int bit);
	void setOpmaskIdx(int idx, bool /*ignore_idx0*/ = true)
	{
		if (mask_ && (mask_ != unsigned(idx))) XBYAK_THROW(ERR_OPMASK_IS_ALREADY_SET)
		mask_ = idx;
	}
	void setRounding(int idx)
	{
		if (rounding_ && (rounding_ != unsigned(idx))) XBYAK_THROW(ERR_ROUNDING_IS_ALREADY_SET)
		rounding_ = idx;
	}
	void setZero() { zero_ = true; }
	void setNF() { NF_ = true; }
	int getNF() const { return NF_; }
	void setZU() { ZU_ = true; }
	int getZU() const { return ZU_; }
	// ah, ch, dh, bh?
	bool isHigh8bit() const
	{
		if (!isBit(8)) return false;
		if (isExt8bit()) return false;
		const int idx = getIdx();
		return AH <= idx && idx <= BH;
	}
	// any bit is accetable if bit == 0
	XBYAK_CONSTEXPR bool is(int kind, uint32_t bit = 0) const
	{
		return (kind == 0 || (kind_ & kind)) && (bit == 0 || (bit_ & bit)); // cf. you can set (8|16)
	}
	XBYAK_CONSTEXPR bool isBit(uint32_t bit) const { return (bit_ & bit) != 0; }
	XBYAK_CONSTEXPR uint32_t getBit() const { return bit_; }
	const char *toString() const
	{
		if (isBSR()) return "bsr0";
		const int idx = getIdx();
		if (kind_ == REG) {
			if (isExt8bit()) {
				static const char *tbl[4] = { "spl", "bpl", "sil", "dil" };
				return tbl[idx - 4];
			}
			static const char *tbl[4][32] = {
				{ "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh", "r8b", "r9b", "r10b",  "r11b", "r12b", "r13b", "r14b", "r15b",
				 "r16b", "r17b", "r18b", "r19b", "r20b", "r21b", "r22b", "r23b", "r24b", "r25b", "r26b", "r27b", "r28b", "r29b", "r30b", "r31b",
				},
				{ "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8w", "r9w", "r10w",  "r11w", "r12w", "r13w", "r14w", "r15w",
				 "r16w", "r17w", "r18w", "r19w", "r20w", "r21w", "r22w", "r23w", "r24w", "r25w", "r26w", "r27w", "r28w", "r29w", "r30w", "r31w",
				},
				{ "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", "r8d", "r9d", "r10d",  "r11d", "r12d", "r13d", "r14d", "r15d",
				 "r16d", "r17d", "r18d", "r19d", "r20d", "r21d", "r22d", "r23d", "r24d", "r25d", "r26d", "r27d", "r28d", "r29d", "r30d", "r31d",
				},
				{ "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10",  "r11", "r12", "r13", "r14", "r15",
				 "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
				},
			};
			return tbl[bit_ == 8 ? 0 : bit_ == 16 ? 1 : bit_ == 32 ? 2 : 3][idx];
		} else if (isOPMASK()) {
			static const char *tbl[8] = { "k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7" };
			return tbl[idx];
		} else if (isTMM()) {
			static const char *tbl[8] = {
				"tmm0", "tmm1", "tmm2", "tmm3", "tmm4", "tmm5", "tmm6", "tmm7"
			};
			return tbl[idx];
		} else if (isZMM()) {
			static const char *tbl[32] = {
				"zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15",
				"zmm16", "zmm17", "zmm18", "zmm19", "zmm20", "zmm21", "zmm22", "zmm23", "zmm24", "zmm25", "zmm26", "zmm27", "zmm28", "zmm29", "zmm30", "zmm31"
			};
			return tbl[idx];
		} else if (isYMM()) {
			static const char *tbl[32] = {
				"ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
				"ymm16", "ymm17", "ymm18", "ymm19", "ymm20", "ymm21", "ymm22", "ymm23", "ymm24", "ymm25", "ymm26", "ymm27", "ymm28", "ymm29", "ymm30", "ymm31"
			};
			return tbl[idx];
		} else if (isXMM()) {
			static const char *tbl[32] = {
				"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
				"xmm16", "xmm17", "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23", "xmm24", "xmm25", "xmm26", "xmm27", "xmm28", "xmm29", "xmm30", "xmm31"
			};
			return tbl[idx];
		} else if (isMMX()) {
			static const char *tbl[8] = { "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7" };
			return tbl[idx];
		} else if (isFPU()) {
			static const char *tbl[8] = { "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7" };
			return tbl[idx];
		} else if (isBNDREG()) {
			static const char *tbl[4] = { "bnd0", "bnd1", "bnd2", "bnd3" };
			return tbl[idx];
		}
		XBYAK_THROW_RET(ERR_INTERNAL, "");
	}
	bool isEqualIfNotInherited(const Operand& rhs) const { return idx_ == rhs.idx_ && kind_ == rhs.kind_ && bit_ == rhs.bit_ && zero_ == rhs.zero_ && mask_ == rhs.mask_ && rounding_ == rhs.rounding_; }
	bool operator==(const Operand& rhs) const;
	bool operator!=(const Operand& rhs) const { return !operator==(rhs); }
	const Address& getAddress() const;
	Address getAddress(int immSize) const;
	const Reg& getReg() const;
};

inline void Operand::setBit(int bit)
{
	if (bit != 8 && bit != 16 && bit != 32 && bit != 64 && bit != 128 && bit != 256 && bit != 512 && bit != 8192) goto ERR;
	if (isBit(bit)) return;
	if (is(MEM | OPMASK)) {
		bit_ = bit;
		return;
	}
	if (is(REG | XMM | YMM | ZMM | TMM)) {
		int idx = getIdx();
		// err if converting ah, bh, ch, dh
		if (isREG(8) && (4 <= idx && idx < 8) && !isExt8bit()) goto ERR;
		Kind kind = REG;
		switch (bit) {
		case 8:
#ifdef XBYAK32
			if (idx >= 4) goto ERR;
#else
			if (idx >= 32) goto ERR;
			if (4 <= idx && idx < 8) idx |= EXT8BIT;
#endif
			break;
		case 16:
		case 32:
		case 64:
#ifdef XBYAK32
			if (idx >= 16) goto ERR;
#else
			if (idx >= 32) goto ERR;
#endif
			break;
		case 128: kind = XMM; break;
		case 256: kind = YMM; break;
		case 512: kind = ZMM; break;
		case 8192: kind = TMM; break;
		}
		idx_ = idx;
		kind_ = kind;
		bit_ = bit;
		if (bit >= 128) return; // keep mask_ and rounding_
		mask_ = 0;
		rounding_ = 0;
		return;
	}
ERR:
	XBYAK_THROW(ERR_CANT_CONVERT)
}

class Label;

struct Reg8;
struct Reg16;
struct Reg32;
struct Xmm;
struct Ymm;
struct Zmm;
#ifdef XBYAK64
struct Reg64;
#endif
class Reg : public Operand {
public:
	XBYAK_CONSTEXPR Reg() { }
	XBYAK_CONSTEXPR Reg(int idx, Kind kind, int bit = 0, bool ext8bit = false) : Operand(idx, kind, bit, ext8bit) { }
	// convert to Reg8/Reg16/Reg32/Reg64/XMM/YMM/ZMM
	Reg changeBit(int bit) const { Reg r(*this); r.setBit(bit); return r; }
	Reg8 cvt8() const;
	Reg16 cvt16() const;
	Reg32 cvt32() const;
#ifdef XBYAK64
	Reg64 cvt64() const;
#endif
	Xmm cvt128() const;
	Ymm cvt256() const;
	Zmm cvt512() const;
	Reg operator|(const ApxFlagNF&) const { Reg r(*this); r.setNF(); return r; }
	Reg operator|(const ApxFlagZU&) const { Reg r(*this); r.setZU(); return r; }
};

inline const Reg& Operand::getReg() const
{
	assert(!isMEM());
	return static_cast<const Reg&>(*this);
}

struct Reg8 : public Reg {
	explicit XBYAK_CONSTEXPR Reg8(int idx = 0, bool ext8bit = false) : Reg(idx, Operand::REG, 8, ext8bit) { }
};

struct Reg16 : public Reg {
	explicit XBYAK_CONSTEXPR Reg16(int idx = 0) : Reg(idx, Operand::REG, 16) { }
};

struct Mmx : public Reg {
	explicit XBYAK_CONSTEXPR Mmx(int idx = 0, Kind kind = Operand::MMX, int bit = 64) : Reg(idx, kind, bit) { }
};

struct EvexModifierRounding {
	enum {
		T_RN_SAE = 1,
		T_RD_SAE = 2,
		T_RU_SAE = 3,
		T_RZ_SAE = 4,
		T_SAE = 5
	};
	explicit XBYAK_CONSTEXPR EvexModifierRounding(int rounding) : rounding(rounding) {}
	int rounding;
};
struct EvexModifierZero{ explicit XBYAK_CONSTEXPR EvexModifierZero(int = 0) {}};

struct Xmm : public Mmx {
	explicit XBYAK_CONSTEXPR Xmm(int idx = 0, Kind kind = Operand::XMM, int bit = 128) : Mmx(idx, kind, bit) { }
	XBYAK_CONSTEXPR Xmm(Kind kind, int idx) : Mmx(idx, kind, kind == XMM ? 128 : kind == YMM ? 256 : 512) { }
	Xmm operator|(const EvexModifierRounding& emr) const { Xmm r(*this); r.setRounding(emr.rounding); return r; }
	Xmm copyAndSetIdx(int idx) const { Xmm ret(*this); ret.setIdx(idx); return ret; }
	Xmm copyAndSetKind(Operand::Kind kind) const { Xmm ret(*this); ret.setKind(kind); return ret; }
};

struct Ymm : public Xmm {
	explicit XBYAK_CONSTEXPR Ymm(int idx = 0, Kind kind = Operand::YMM, int bit = 256) : Xmm(idx, kind, bit) { }
	Ymm operator|(const EvexModifierRounding& emr) const { Ymm r(*this); r.setRounding(emr.rounding); return r; }
};

struct Zmm : public Ymm {
	explicit XBYAK_CONSTEXPR Zmm(int idx = 0) : Ymm(idx, Operand::ZMM, 512) { }
	Zmm operator|(const EvexModifierRounding& emr) const { Zmm r(*this); r.setRounding(emr.rounding); return r; }
};

#ifdef XBYAK64
struct Tmm : public Reg {
	explicit XBYAK_CONSTEXPR Tmm(int idx = 0, Kind kind = Operand::TMM, int bit = 8192) : Reg(idx, kind, bit) { }
};
// Singleton register (idx always 0): 1024-bit width is its true size, but every memory-capable
// mnemonic that uses it sets T_N1 (not T_N_VL), so evex()'s VL==512 disp8N multiplier check is
// never reached with this width.
struct Bsr : public Reg {
	explicit XBYAK_CONSTEXPR Bsr(int idx = 0) : Reg(idx, Operand::BSR, 1024) { }
};
#endif

struct Opmask : public Reg {
	explicit XBYAK_CONSTEXPR Opmask(int idx = 0) : Reg(idx, Operand::OPMASK, 64) {}
};

struct BoundsReg : public Reg {
	explicit XBYAK_CONSTEXPR BoundsReg(int idx = 0) : Reg(idx, Operand::BNDREG, 128) {}
};

template<class T>T operator|(const T& x, const Opmask& k) { T r(x); r.setOpmaskIdx(k.getIdx()); return r; }
template<class T>T operator|(const T& x, const EvexModifierZero&) { T r(x); r.setZero(); return r; }
template<class T>T operator|(const T& x, const EvexModifierRounding& emr) { T r(x); r.setRounding(emr.rounding); return r; }

struct Fpu : public Reg {
	explicit XBYAK_CONSTEXPR Fpu(int idx = 0) : Reg(idx, Operand::FPU, 32) { }
};

struct Reg32e : public Reg {
	explicit XBYAK_CONSTEXPR Reg32e(int idx, int bit) : Reg(idx, Operand::REG, bit) {}
	Reg32e operator|(const ApxFlagNF&) const { Reg32e r(*this); r.setNF(); return r; }
	Reg32e operator|(const ApxFlagZU&) const { Reg32e r(*this); r.setZU(); return r; }
};
struct Reg32 : public Reg32e {
	explicit XBYAK_CONSTEXPR Reg32(int idx = 0) : Reg32e(idx, 32) {}
};
#ifdef XBYAK64
struct Reg64 : public Reg32e {
	explicit XBYAK_CONSTEXPR Reg64(int idx = 0) : Reg32e(idx, 64) {}
};
struct RegRip {
	explicit XBYAK_CONSTEXPR RegRip(int = 0) {}
};
#endif

inline Reg8 Reg::cvt8() const
{
	Reg r = changeBit(8); return Reg8(r.getIdx(), r.isExt8bit());
}

inline Reg16 Reg::cvt16() const
{
	return Reg16(changeBit(16).getIdx());
}

inline Reg32 Reg::cvt32() const
{
	return Reg32(changeBit(32).getIdx());
}

#ifdef XBYAK64
inline Reg64 Reg::cvt64() const
{
	return Reg64(changeBit(64).getIdx());
}
#endif

inline Xmm Reg::cvt128() const
{
	return Xmm(changeBit(128).getIdx());
}

inline Ymm Reg::cvt256() const
{
	return Ymm(changeBit(256).getIdx());
}

inline Zmm Reg::cvt512() const
{
	return Zmm(changeBit(512).getIdx());
}

#ifndef XBYAK_DISABLE_SEGMENT
// not derived from Reg
class Segment {
	int idx_;
public:
	enum {
		es, cs, ss, ds, fs, gs
	};
	explicit XBYAK_CONSTEXPR Segment(int idx) : idx_(idx) { assert(0 <= idx_ && idx_ < 6); }
	int getIdx() const { return idx_; }
	const char *toString() const
	{
		static const char tbl[][3] = {
			"es", "cs", "ss", "ds", "fs", "gs"
		};
		return tbl[idx_];
	}
};
#endif

/*
	pattern
	[base]? [+index[*scale]]? [+/-disp]* [+label]?
	rip [+/-disp]* [+label]?
	  rip+disp if backward reference then use label.getAddress()
	  rip+label if forward reference
	[&var]?[+/-disp]*
*/
class RegExp {
	friend class Address;
public:
#ifdef XBYAK64
	enum { i32e = 32 | 64 };
#else
	enum { i32e = 32 };
#endif
	XBYAK_CONSTEXPR RegExp() : scale_(0), disp_(0), label_(0), rip_(false), asPtr_(false) { }
	XBYAK_CONSTEXPR RegExp(size_t disp) : scale_(0), disp_(disp), label_(0), rip_(false), asPtr_(false) { }
	XBYAK_CONSTEXPR RegExp(const Reg& r, int scale = 1)
		: scale_(scale)
		, disp_(0)
		, label_(0)
		, rip_(false)
		, asPtr_(false)
	{
		if (!r.isREG(i32e) && !r.is(Reg::XMM|Reg::YMM|Reg::ZMM|Reg::TMM)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		if (scale == 0) return;
		if (scale != 1 && scale != 2 && scale != 4 && scale != 8) XBYAK_THROW(ERR_BAD_SCALE)
		if (r.getBit() >= 128 || scale != 1) { // xmm/ymm is always index
			index_ = r;
		} else {
			base_ = r;
		}
	}
	RegExp(Label& label);

	// can't use constexpr to const void *
	explicit RegExp(const void *addr)
		: scale_(0)
		, disp_(size_t(addr))
		, label_(0)
		, rip_(false)
		, asPtr_(true)
	{
	}
#ifdef XBYAK64
	XBYAK_CONSTEXPR RegExp(const RegRip& /*rip*/)
		: scale_(0)
		, disp_(0)
		, label_(0)
		, rip_(true)
		, asPtr_(false)
	{
	}
#endif
	bool isVsib(int bit = 128 | 256 | 512) const { return index_.isBit(bit); }
	bool operator==(const RegExp& rhs) const
	{
		return base_ == rhs.base_ && index_ == rhs.index_ && disp_ == rhs.disp_ && scale_ == rhs.scale_;
	}
	const Reg& getBase() const { return base_; }
	const Reg& getIndex() const { return index_; }
	const Label *getLabel() const { return label_; }
	bool isOnlyDisp() const { return !base_.getBit() && !index_.getBit(); } // for mov eax
	int getScale() const { return scale_; }
	size_t getDisp() const { return disp_; }
	XBYAK_CONSTEXPR void verify() const
	{
		if (base_.getBit() >= 128) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		if (index_.getBit() && index_.getBit() <= 64) {
			if (index_.getIdx() == Operand::ESP) XBYAK_THROW(ERR_ESP_CANT_BE_INDEX)
			if (base_.getBit() && base_.getBit() != index_.getBit()) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		}
	}
	friend RegExp operator+(const RegExp& a, const RegExp& b);
	friend RegExp operator+(const RegExp& e, unsigned long long disp);
	friend RegExp operator-(const RegExp& e, size_t disp);
private:
	/*
		[base_ + index_ * scale_ + disp_]
		base : Reg32e, index : Reg32e(w/o esp), Xmm, Ymm
	*/
	Reg base_;
	Reg index_;
	int scale_;
	size_t disp_; // absolute address
	Label *label_;
	bool rip_;
	bool asPtr_; // disp_ contains a pointer
};

inline RegExp operator+(const RegExp& a, const RegExp& b)
{
	if (a.index_.getBit() && b.index_.getBit()) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
	if (a.label_ && b.label_) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
	if (b.rip_) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
	if (a.rip_ && !b.isOnlyDisp()) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
	if (a.asPtr_ && b.asPtr_) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
	RegExp ret = a;
	if (ret.label_ == 0) ret.label_ = b.label_;
	if (ret.asPtr_ == 0) ret.asPtr_ = b.asPtr_;
	if (!ret.index_.getBit()) { ret.index_ = b.index_; ret.scale_ = b.scale_; }
	if (b.base_.getBit()) {
		if (ret.base_.getBit()) {
			if (ret.index_.getBit()) XBYAK_THROW_RET(ERR_BAD_ADDRESSING, RegExp())
			// base + base => base + index * 1
			ret.index_ = b.base_;
			// [reg + esp] => [esp + reg]
			if (ret.index_.getIdx() == Operand::ESP) std::swap(ret.base_, ret.index_);
			ret.scale_ = 1;
		} else {
			ret.base_ = b.base_;
		}
	}
	ret.disp_ += b.disp_;
	return ret;
}
inline RegExp operator*(const Reg& r, int scale)
{
	return RegExp(r, scale);
}
inline RegExp operator*(int scale, const Reg& r)
{
	return r * scale;
}

// backward compatibility for eax+&x (pointer address)
inline RegExp operator+(const RegExp& a, const void* b) { return a + RegExp(b); }

// since what size_t is typedef'd to depends on the implementation, use unsigned long long (assume u64) for the implementation.
inline RegExp operator+(const RegExp& e, unsigned long long disp)
{
	RegExp ret = e;
	ret.disp_ += static_cast<size_t>(disp);
	return ret;
}
// overload for integer literals (e.g. eax+0) to avoid ambiguity with the void* overload
inline RegExp operator+(const RegExp& e, int disp) { return e + static_cast<unsigned long long>(disp); }
inline RegExp operator+(const RegExp& e, long disp) { return e + static_cast<unsigned long long>(disp); }
inline RegExp operator+(const RegExp& e, long long disp) { return e + static_cast<unsigned long long>(disp); }
inline RegExp operator+(const RegExp& e, unsigned int disp) { return e + static_cast<unsigned long long>(disp); }
inline RegExp operator+(const RegExp& e, unsigned long disp) { return e + static_cast<unsigned long long>(disp); }

inline RegExp operator-(const RegExp& e, size_t disp)
{
	RegExp ret = e;
	ret.disp_ -= disp;
	return ret;
}

// 2nd parameter for constructor of CodeArray(maxSize, userPtr, alloc)
void *const AutoGrow = (void*)1; //-V566
void *const DontSetProtectRWE = (void*)2; //-V566

class CodeArray {
	enum Type {
		USER_BUF = 1, // use userPtr(non alignment, non protect)
		ALLOC_BUF, // use new(alignment, protect)
		AUTO_GROW // automatically move and grow memory if necessary
	};
	CodeArray(const CodeArray& rhs);
	void operator=(const CodeArray&);
	struct AddrInfo {
		size_t codeOffset; // position to write
		size_t jmpAddr; // value to write
		int jmpSize; // size of jmpAddr
		inner::LabelMode mode;
		AddrInfo(size_t _codeOffset, size_t _jmpAddr, int _jmpSize, inner::LabelMode _mode)
			: codeOffset(_codeOffset), jmpAddr(_jmpAddr), jmpSize(_jmpSize), mode(_mode) {}
		uint64_t getVal(const uint8_t *top) const
		{
			uint64_t disp = (mode == inner::LaddTop) ? jmpAddr + size_t(top) : (mode == inner::LsubTop) ? jmpAddr - size_t(top) : jmpAddr;
			if (jmpSize == 4) disp = inner::VerifyInInt32(disp);
			return disp;
		}
	};
	typedef std::vector<AddrInfo> AddrInfoList;
	AddrInfoList addrInfoList_;
	const Type type_;
#ifdef XBYAK_USE_MMAP_ALLOCATOR
	MmapAllocator defaultAllocator_;
#else
	Allocator defaultAllocator_;
#endif
	Allocator *alloc_;
protected:
	size_t maxSize_;
	uint8_t *top_;
	size_t size_;
	bool isCalledCalcJmpAddress_;

	bool useProtect() const { return alloc_->useProtect(); }
	/*
		allocate new memory and copy old data to the new area
	*/
	void growMemory()
	{
		const size_t newSize = (std::max<size_t>)(DEFAULT_MAX_CODE_SIZE, maxSize_ * 2);
		uint8_t *newTop = alloc_->alloc(newSize);
		if (newTop == 0) XBYAK_THROW(ERR_CANT_ALLOC)
		for (size_t i = 0; i < size_; i++) newTop[i] = top_[i];
		alloc_->free(top_);
		top_ = newTop;
		maxSize_ = newSize;
	}
	// grow memory in advance so that the code of a jmp is not split by growMemory() in AutoGrow mode
	void growMemoryForJmp()
	{
		if (isAutoGrow() && size_ + 16 >= maxSize_) growMemory();
	}
	/*
		calc jmp address for AutoGrow mode
	*/
	void calcJmpAddress()
	{
		if (isCalledCalcJmpAddress_) return;
		for (AddrInfoList::const_iterator i = addrInfoList_.begin(), ie = addrInfoList_.end(); i != ie; ++i) {
			uint64_t disp = i->getVal(top_);
			rewrite(i->codeOffset, disp, i->jmpSize);
		}
		isCalledCalcJmpAddress_ = true;
	}
public:
	enum ProtectMode {
		PROTECT_RW = 0, // read/write
		PROTECT_RWE = 1, // read/write/exec
		PROTECT_RE = 2 // read/exec
	};
protected:
	ProtectMode curMode_;
public:
	explicit CodeArray(size_t maxSize, void *userPtr = 0, Allocator *allocator = 0)
		: type_(userPtr == AutoGrow ? AUTO_GROW : (userPtr == 0 || userPtr == DontSetProtectRWE) ? ALLOC_BUF : USER_BUF)
		, alloc_(allocator ? allocator : (Allocator*)&defaultAllocator_)
		, maxSize_(maxSize)
		, top_(type_ == USER_BUF ? reinterpret_cast<uint8_t*>(userPtr) : alloc_->alloc((std::max<size_t>)(maxSize, 1)))
		, size_(0)
		, isCalledCalcJmpAddress_(false)
		, curMode_(PROTECT_RW)
	{
		if (maxSize_ > 0 && top_ == 0) XBYAK_THROW(ERR_CANT_ALLOC)
		if ((type_ == ALLOC_BUF && userPtr != DontSetProtectRWE && useProtect()) && !setProtectMode(PROTECT_RWE, false)) {
			alloc_->free(top_);
			XBYAK_THROW(ERR_CANT_PROTECT)
		}
	}
	virtual ~CodeArray()
	{
		if (isAllocType()) {
			if (useProtect()) setProtectModeRW(false);
			alloc_->free(top_);
		}
	}
	bool setProtectMode(ProtectMode mode, bool throwException = true)
	{
		bool isOK = protect(top_, maxSize_, mode);
		if (isOK) {
			curMode_ = mode;
			return true;
		}
		if (throwException) XBYAK_THROW_RET(ERR_CANT_PROTECT, false)
		return false;
	}
	bool setProtectModeRE(bool throwException = true) { return setProtectMode(PROTECT_RE, throwException); }
	bool setProtectModeRW(bool throwException = true) { return setProtectMode(PROTECT_RW, throwException); }
	void resetSize()
	{
		size_ = 0;
		addrInfoList_.clear();
		isCalledCalcJmpAddress_ = false;
	}
	void db(int code)
	{
#ifdef XBYAK_NO_EXCEPTION
		if (local::GetErrorRef()) return;
#endif
		if (size_ >= maxSize_) {
			if (type_ == AUTO_GROW) {
				growMemory();
			} else {
				XBYAK_THROW(ERR_CODE_IS_TOO_BIG)
			}
		}
		if (top_ == 0) XBYAK_THROW(ERR_CANT_ALLOC)
		top_[size_++] = static_cast<uint8_t>(code);
	}
	void db(const uint8_t *code, size_t codeSize)
	{
		for (size_t i = 0; i < codeSize; i++) db(code[i]);
	}
	void db(uint64_t code, size_t codeSize)
	{
		if (codeSize > 8) XBYAK_THROW(ERR_BAD_PARAMETER)
		for (size_t i = 0; i < codeSize; i++) db(static_cast<uint8_t>(code >> (i * 8)));
	}
	void dw(uint32_t code) { db(code, 2); }
	void dd(uint32_t code) { db(code, 4); }
	void dq(uint64_t code) { db(code, 8); }
	const uint8_t *getCode() const { return top_; }
	template<class F>
	const F getCode() const { return reinterpret_cast<F>(top_); }
	const uint8_t *getCurr() const { return &top_[size_]; }
	template<class F>
	const F getCurr() const { return reinterpret_cast<F>(&top_[size_]); }
	size_t getSize() const { return size_; }
	void setSize(size_t size)
	{
		if (size > maxSize_) XBYAK_THROW(ERR_OFFSET_IS_TOO_BIG)
		size_ = size;
	}
	void dump() const
	{
		const uint8_t *p = getCode();
		size_t bufSize = getSize();
		size_t remain = bufSize;
		for (int i = 0; i < 4; i++) {
			size_t disp = 16;
			if (remain < 16) {
				disp = remain;
			}
			for (size_t j = 0; j < 16; j++) {
				if (j < disp) {
					printf("%02X", p[i * 16 + j]);
				}
			}
			putchar('\n');
			remain -= disp;
			if (remain == 0) {
				break;
			}
		}
	}
	/*
		@param offset [in] offset from top
		@param disp [in] offset from the next of jmp
		@param size [in] write size(1, 2, 4, 8)
	*/
	void rewrite(size_t offset, uint64_t disp, size_t size)
	{
		if (offset >= maxSize_ || size > maxSize_ - offset) XBYAK_THROW(ERR_OFFSET_IS_TOO_BIG)
		if (size != 1 && size != 2 && size != 4 && size != 8) XBYAK_THROW(ERR_BAD_PARAMETER)
		uint8_t *const data = top_ + offset;
		for (size_t i = 0; i < size; i++) {
			data[i] = static_cast<uint8_t>(disp >> (i * 8));
		}
	}
	void save(size_t offset, size_t val, int size, inner::LabelMode mode)
	{
		addrInfoList_.push_back(AddrInfo(offset, val, size, mode));
	}
	bool isAutoGrow() const { return type_ == AUTO_GROW; }
	bool isAllocType() const { return type_ == ALLOC_BUF || type_ == AUTO_GROW; }
	bool isCalledCalcJmpAddress() const { return isCalledCalcJmpAddress_; }
	/**
		change exec permission of memory
		@param addr [in] buffer address
		@param size [in] buffer size
		@param protectMode [in] mode(RW/RWE/RE)
		@return true(success), false(failure)
	*/
	static inline bool protect(const void *addr, size_t size, int protectMode)
	{
#if defined(_WIN32)
		const DWORD c_rw = PAGE_READWRITE;
		const DWORD c_rwe = PAGE_EXECUTE_READWRITE;
		const DWORD c_re = PAGE_EXECUTE_READ;
		DWORD mode;
#else
		const int c_rw = PROT_READ | PROT_WRITE;
		const int c_rwe = PROT_READ | PROT_WRITE | PROT_EXEC;
		const int c_re = PROT_READ | PROT_EXEC;
		int mode;
#endif
		switch (protectMode) {
		case PROTECT_RW: mode = c_rw; break;
		case PROTECT_RWE: mode = c_rwe; break;
		case PROTECT_RE: mode = c_re; break;
		default:
			return false;
		}
#if defined(_WIN32)
		DWORD oldProtect;
		return VirtualProtect(const_cast<void*>(addr), size, mode, &oldProtect) != 0;
#elif defined(__GNUC__)
		size_t pageSize = inner::getPageSize();
		size_t iaddr = reinterpret_cast<size_t>(addr);
		size_t roundAddr = iaddr & ~(pageSize - static_cast<size_t>(1));
		return mprotect(reinterpret_cast<void*>(roundAddr), size + (iaddr - roundAddr), mode) == 0;
#else
		return true;
#endif
	}
	/**
		get aligned memory pointer
		@param addr [in] address
		@param alignedSize [in] power of two
		@return aligned addr by alingedSize
	*/
	static inline uint8_t *getAlignedAddress(uint8_t *addr, size_t alignedSize = 16)
	{
		return reinterpret_cast<uint8_t*>((reinterpret_cast<size_t>(addr) + alignedSize - 1) & ~(alignedSize - static_cast<size_t>(1)));
	}
};

class Address : public Operand {
public:
	XBYAK_CONSTEXPR Address()
		: Operand(0, MEM, 0), e_(), label_(NULL), mode_(inner::M_ModRM), immSize(0),
		  disp8N(0), permitVsib(false), broadcast_(false), optimized_(false) { }
	XBYAK_CONSTEXPR Address(uint32_t sizeBit, bool broadcast, const RegExp& e)
		: Operand(0, MEM, sizeBit), e_(e), label_(e.label_), mode_(), immSize(0),
		  disp8N(0), permitVsib(false), broadcast_(broadcast), optimized_(false)
	{
		if (e.rip_) {
			mode_ = (e.label_ || e.asPtr_) ? inner::M_ripAddr : inner::M_rip;
		} else {
#ifdef XBYAK64
			uint64_t disp = e.getDisp();
			if (e.isOnlyDisp() && ((0x80000000 <= disp && disp <= 0xffffffff80000000) || e.getLabel())) {
				mode_ = inner::M_64bitDisp;
			} else
#endif
			{
				mode_ = inner::M_ModRM;
			}
		}
		e_.verify();
		// [reg * 2] => [reg + reg] to shorten the encoding (cloneNoOptimize() undoes this)
		if (e_.index_.isBit(RegExp::i32e) && !e_.base_.getBit() && e_.scale_ == 2) {
			e_.base_ = e_.index_;
			e_.scale_ = 1;
			optimized_ = true;
		}
	}
	const RegExp& getRegExp() const { return e_; }
	Address cloneNoOptimize() const
	{
		Address addr = *this;
		if (addr.optimized_) {
			addr.e_.base_ = Reg();
			addr.e_.scale_ = 2;
			addr.optimized_ = false;
		}
		return addr;
	}
	inner::AddressMode getMode() const { return mode_; }
	bool is32bit() const { return e_.getBase().getBit() == 32 || e_.getIndex().getBit() == 32; }
	bool isOnlyDisp() const { return e_.isOnlyDisp(); }
	size_t getDisp() const { return e_.getDisp(); }
	bool is64bitDisp() const { return mode_ == inner::M_64bitDisp; } // for moffset
	bool isBroadcast() const { return broadcast_; }
	bool hasRex2() const { return e_.getBase().hasRex2() || e_.getIndex().hasRex2(); }
	const Label* getLabel() const { return label_; }
	bool operator==(const Address& rhs) const
	{
		return getBit() == rhs.getBit() && e_ == rhs.e_ && label_ == rhs.label_ && mode_ == rhs.mode_ && immSize == rhs.immSize && disp8N == rhs.disp8N && permitVsib == rhs.permitVsib && broadcast_ == rhs.broadcast_ && optimized_ == rhs.optimized_;
	}
	bool operator!=(const Address& rhs) const { return !operator==(rhs); }
	bool isVsib() const { return e_.isVsib(); }
	// change byte to dword etc.
	Address changeBit(int bit) const { Address addr(*this); addr.setBit(bit); return addr; }
private:
	RegExp e_;
	const Label* label_;
	inner::AddressMode mode_;
public:
	int immSize; // the size of immediate value of nmemonics (0, 1, 2, 4)
	int disp8N; // 0(normal), 1(force disp32), disp8N = {2, 4, 8}
	bool permitVsib;
private:
	bool broadcast_;
	bool optimized_; // e_ was rewritten from [reg * 2] to [reg + reg]
};

inline const Address& Operand::getAddress() const
{
	assert(isMEM());
	return static_cast<const Address&>(*this);
}
inline Address Operand::getAddress(int immSize) const
{
	Address addr = getAddress();
	addr.immSize = immSize;
	return addr;
}

inline bool Operand::operator==(const Operand& rhs) const
{
	if (isMEM() && rhs.isMEM()) return this->getAddress() == rhs.getAddress();
	return isEqualIfNotInherited(rhs);
}

inline XBYAK_CONSTEXPR bool Operand::hasRex2() const
{
	return (isREG() && isExtIdx2()) || (isMEM() && static_cast<const Address&>(*this).hasRex2());
}

class AddressFrame {
	void operator=(const AddressFrame&);
	AddressFrame(const AddressFrame&);
public:
	const uint32_t bit_;
	const bool broadcast_;
	explicit XBYAK_CONSTEXPR AddressFrame(uint32_t bit, bool broadcast = false) : bit_(bit), broadcast_(broadcast) { }
	Address operator[](const RegExp& e) const
	{
		return Address(bit_, broadcast_, e);
	}
	Address operator[](const void *addr) const
	{
		return operator[](RegExp(addr));
	}
};

struct JmpLabel {
	size_t endOfJmp; /* offset from top to the end address of jmp */
	int jmpSize;
	inner::LabelMode mode;
	size_t disp; // disp for [rip + disp] or [forward ref label + disp]
	explicit JmpLabel(size_t endOfJmp = 0, int jmpSize = 0, inner::LabelMode mode = inner::LasIs, size_t disp = 0)
		: endOfJmp(endOfJmp), jmpSize(jmpSize), mode(mode), disp(disp)
	{
	}
};

class LabelManager;

class Label {
	mutable LabelManager *mgr;
	mutable int id;
	friend class LabelManager;
public:
	Label() : mgr(0), id(0) {}
	Label(const Label& rhs);
	Label& operator=(const Label& rhs);
	~Label();
	void clear() { mgr = 0; id = 0; }
	int getId() const { return id; }
	bool isDefined() const;
	const uint8_t *getAddress() const;

	// backward compatibility
	static inline std::string toStr(int num)
	{
		char buf[16];
#if defined(_MSC_VER) && (_MSC_VER < 1900)
		_snprintf_s
#else
		snprintf
#endif
		(buf, sizeof(buf), ".%08x", num);
		return buf;
	}
};

inline RegExp::RegExp(Label& label)
	: scale_(1)
	, disp_(0)
	, label_(0)
	, rip_(false)
	, asPtr_(true)
{
	const uint8_t *addr = label.getAddress();
	if (addr) {
		disp_ = size_t(addr);
		label_ = 0;
	} else {
		label_ = &label;
	}
}

class LabelManager {
	// for string label
	struct SlabelVal {
		size_t offset;
		SlabelVal(size_t offset) : offset(offset) {}
	};
	typedef XBYAK_STD_UNORDERED_MAP<std::string, SlabelVal> SlabelDefList;
	typedef XBYAK_STD_UNORDERED_MULTIMAP<std::string, const JmpLabel> SlabelUndefList;
	struct SlabelState {
		SlabelDefList defList;
		SlabelUndefList undefList;
	};
	// SlabelState is cheap to move, so std::vector is preferred over std::list.
	typedef std::vector<SlabelState> StateList;
	// for Label class
	struct ClabelVal {
		ClabelVal(size_t offset = 0) : offset(offset), refCount(1) {}
		size_t offset;
		int refCount;
	};
	typedef XBYAK_STD_UNORDERED_MAP<int, ClabelVal> ClabelDefList;
	typedef XBYAK_STD_UNORDERED_MULTIMAP<int, const JmpLabel> ClabelUndefList;
	typedef XBYAK_STD_UNORDERED_SET<Label*> LabelPtrList;

	CodeArray *base_;
	// global : stateList_.front(), local : stateList_.back()
	StateList stateList_;
	mutable int labelId_;
	ClabelDefList clabelDefList_;
	ClabelUndefList clabelUndefList_;
	LabelPtrList labelPtrList_;

	// assign a new id at the first use of label (forward reference), so Label::id is mutable
	int getOrAssignId(const Label& label) const
	{
		if (label.id == 0) label.id = labelId_++;
		return label.id;
	}
	// label starting with '.' is local (stateList_.back()), otherwise global (stateList_.front())
	const SlabelState& getSlabelState(const std::string& label) const
	{
		return *label.c_str() == '.' ? stateList_.back() : stateList_.front();
	}
	SlabelState& getSlabelState(const std::string& label)
	{
		return *label.c_str() == '.' ? stateList_.back() : stateList_.front();
	}
	template<class DefList, class UndefList, class T>
	void define_inner(DefList& defList, UndefList& undefList, const T& labelId, size_t addrOffset)
	{
		// add label
		typename DefList::value_type item(labelId, addrOffset);
		std::pair<typename DefList::iterator, bool> ret = defList.insert(item);
		if (!ret.second) XBYAK_THROW(ERR_LABEL_IS_REDEFINED)
		// search undefined label
		for (;;) {
			typename UndefList::iterator itr = undefList.find(labelId);
			if (itr == undefList.end()) break;
			const JmpLabel *jmp = &itr->second;
			const size_t offset = jmp->endOfJmp - jmp->jmpSize;
			size_t disp = jmp->disp;
			if (jmp->mode == inner::LaddTop) {
				disp += addrOffset;
			} else if (jmp->mode == inner::Labs) {
				disp += size_t(base_->getCode()) + addrOffset; // assign() defines a label at another offset
			} else {
				disp += addrOffset - jmp->endOfJmp;
#ifdef XBYAK64
				if (jmp->jmpSize <= 4 && !inner::IsInInt32(disp)) XBYAK_THROW(ERR_OFFSET_IS_TOO_BIG)
#endif
				if (jmp->jmpSize == 1 && !inner::IsInDisp8((uint32_t)disp)) XBYAK_THROW(ERR_LABEL_IS_TOO_FAR)
			}
			if (base_->isAutoGrow()) {
				base_->save(offset, disp, jmp->jmpSize, jmp->mode);
			} else {
				base_->rewrite(offset, disp, jmp->jmpSize);
			}
			undefList.erase(itr);
		}
	}
	template<class DefList, class T>
	bool getOffset_inner(const DefList& defList, size_t *offset, const T& label) const
	{
		typename DefList::const_iterator i = defList.find(label);
		if (i == defList.end()) return false;
		*offset = i->second.offset;
		return true;
	}
	friend class Label;
	void incRefCount(int id, Label *label)
	{
		clabelDefList_[id].refCount++;
		labelPtrList_.insert(label);
	}
	void decRefCount(int id, Label *label)
	{
		labelPtrList_.erase(label);
		ClabelDefList::iterator i = clabelDefList_.find(id);
		if (i == clabelDefList_.end()) return;
		if (i->second.refCount == 1) {
			clabelDefList_.erase(id);
		} else {
			--i->second.refCount;
		}
	}
	template<class T>
	bool hasUndefinedLabel_inner(const T& list) const
	{
#ifndef NDEBUG
		for (typename T::const_iterator i = list.begin(); i != list.end(); ++i) {
			std::cerr << "undefined label:" << i->first << std::endl;
		}
#endif
		return !list.empty();
	}
	// detach all labels linked to LabelManager
	void resetLabelPtrList()
	{
		for (LabelPtrList::iterator i = labelPtrList_.begin(), ie = labelPtrList_.end(); i != ie; ++i) {
			(*i)->clear();
		}
		labelPtrList_.clear();
	}
public:
	LabelManager()
	{
		reset();
	}
	~LabelManager()
	{
		resetLabelPtrList();
	}
	void reset()
	{
		base_ = 0;
		labelId_ = 1;
		stateList_.clear();
		stateList_.push_back(SlabelState());
		stateList_.push_back(SlabelState());
		clabelDefList_.clear();
		clabelUndefList_.clear();
		resetLabelPtrList();
	}
	void enterLocal()
	{
		stateList_.push_back(SlabelState());
	}
	void leaveLocal()
	{
		if (stateList_.size() <= 2) XBYAK_THROW(ERR_UNDER_LOCAL_LABEL)
		if (hasUndefinedLabel_inner(stateList_.back().undefList)) XBYAK_THROW(ERR_LABEL_IS_NOT_FOUND)
		stateList_.pop_back();
	}
	void set(CodeArray *base) { base_ = base; }
	void defineSlabel(std::string label)
	{
		if (label == "@b" || label == "@f") XBYAK_THROW(ERR_BAD_LABEL_STR)
		if (label == "@@") {
			SlabelDefList& defList = stateList_.front().defList;
			SlabelDefList::iterator i = defList.find("@f");
			if (i != defList.end()) {
				defList.erase(i);
				label = "@b";
			} else {
				i = defList.find("@b");
				if (i != defList.end()) {
					defList.erase(i);
				}
				label = "@f";
			}
		}
		SlabelState& st = getSlabelState(label);
		define_inner(st.defList, st.undefList, label, base_->getSize());
	}
	void defineClabel(Label& label)
	{
		define_inner(clabelDefList_, clabelUndefList_, getOrAssignId(label), base_->getSize());
		label.mgr = this;
		labelPtrList_.insert(&label);
	}
	void assign(Label& dst, const Label& src)
	{
		ClabelDefList::const_iterator i = clabelDefList_.find(src.id);
		if (i == clabelDefList_.end()) XBYAK_THROW(ERR_LABEL_ISNOT_SET_BY_L)
		define_inner(clabelDefList_, clabelUndefList_, getOrAssignId(dst), i->second.offset);
		dst.mgr = this;
		labelPtrList_.insert(&dst);
	}
	bool getOffset(size_t *offset, std::string& label) const
	{
		const SlabelDefList& defList = stateList_.front().defList;
		if (label == "@b") {
			if (defList.find("@f") != defList.end()) {
				label = "@f";
			} else if (defList.find("@b") == defList.end()) {
				XBYAK_THROW_RET(ERR_LABEL_IS_NOT_FOUND, false)
			}
		} else if (label == "@f") {
			if (defList.find("@f") != defList.end()) {
				label = "@b";
			}
		}
		const SlabelState& st = getSlabelState(label);
		return getOffset_inner(st.defList, offset, label);
	}
	bool getOffset(size_t *offset, const Label& label) const
	{
		return getOffset_inner(clabelDefList_, offset, getOrAssignId(label));
	}
	void addUndefinedLabel(const std::string& label, const JmpLabel& jmp)
	{
		SlabelState& st = getSlabelState(label);
		st.undefList.insert(SlabelUndefList::value_type(label, jmp));
	}
	void addUndefinedLabel(const Label& label, const JmpLabel& jmp)
	{
		clabelUndefList_.insert(ClabelUndefList::value_type(label.id, jmp));
	}
	bool hasUndefSlabel() const
	{
		for (StateList::const_iterator i = stateList_.begin(), ie = stateList_.end(); i != ie; ++i) {
			if (hasUndefinedLabel_inner(i->undefList)) return true;
		}
		return false;
	}
	bool hasUndefClabel() const { return hasUndefinedLabel_inner(clabelUndefList_); }
	const uint8_t *getCode() const { return base_->getCode(); }
	bool isReady() const { return !base_->isAutoGrow() || base_->isCalledCalcJmpAddress(); }
	bool isDefined(const Label& label) const { return clabelDefList_.find(label.id) != clabelDefList_.end(); }
};

inline bool Label::isDefined() const
{
	return mgr && mgr->isDefined(*this);
}
inline Label::Label(const Label& rhs)
{
	id = rhs.id;
	mgr = rhs.mgr;
	if (mgr) mgr->incRefCount(id, this);
}
inline Label& Label::operator=(const Label& rhs)
{
	if (id) XBYAK_THROW_RET(ERR_LABEL_IS_ALREADY_SET_BY_L, *this)
	id = rhs.id;
	mgr = rhs.mgr;
	if (mgr) mgr->incRefCount(id, this);
	return *this;
}
inline Label::~Label()
{
	if (id && mgr) mgr->decRefCount(id, this);
}
inline const uint8_t* Label::getAddress() const
{
	if (mgr == 0 || !mgr->isReady()) return 0;
	size_t offset;
	if (!mgr->getOffset(&offset, *this)) return 0;
	return mgr->getCode() + offset;
}

typedef enum {
	DefaultEncoding,
	VexEncoding,
	EvexEncoding,
	PreAVX10v2Encoding,
	AVX10v2Encoding
} PreferredEncoding;

class CodeGenerator : public CodeArray {
public:
	enum LabelType {
		T_SHORT,
		T_NEAR,
		T_FAR, // far jump
		T_AUTO // T_SHORT if possible
	};
private:
	CodeGenerator operator=(const CodeGenerator&); // don't call
#ifdef XBYAK64
	enum { i32e = 32 | 64, BIT = 64 };
	static const uint64_t dummyAddr = uint64_t(0x1122334455667788ull);
	typedef Reg64 NativeReg;
#else
	enum { i32e = 32, BIT = 32 };
	static const size_t dummyAddr = 0x12345678;
	typedef Reg32 NativeReg;
#endif
	// (XMM, XMM|MEM)
	static inline bool isXMM_XMMorMEM(const Operand& op1, const Operand& op2)
	{
		return op1.isXMM() && (op2.isXMM() || op2.isMEM());
	}
	// (MMX, MMX|MEM) or (XMM, XMM|MEM)
	static inline bool isXMMorMMX_MEM(const Operand& op1, const Operand& op2)
	{
		return (op1.isMMX() && (op2.isMMX() || op2.isMEM())) || isXMM_XMMorMEM(op1, op2);
	}
	// (XMM, MMX|MEM)
	static inline bool isXMM_MMXorMEM(const Operand& op1, const Operand& op2)
	{
		return op1.isXMM() && (op2.isMMX() || op2.isMEM());
	}
	// (MMX, XMM|MEM)
	static inline bool isMMX_XMMorMEM(const Operand& op1, const Operand& op2)
	{
		return op1.isMMX() && (op2.isXMM() || op2.isMEM());
	}
	// (XMM, REG32|MEM)
	static inline bool isXMM_REG32orMEM(const Operand& op1, const Operand& op2)
	{
		return op1.isXMM() && (op2.isREG(i32e) || op2.isMEM());
	}
	// (REG32, XMM|MEM)
	static inline bool isREG32_XMMorMEM(const Operand& op1, const Operand& op2)
	{
		return op1.isREG(i32e) && (op2.isXMM() || op2.isMEM());
	}
	static inline bool isValidSSE(const Operand& op)
	{
		// SSE instructions do not support XMM16 - XMM31
		return !(op.isXMM() && op.getIdx() >= 16);
	}
	void verifySSE(const Operand& op1, const Operand& op2 = Operand()) const
	{
		if (!isValidSSE(op1) || !isValidSSE(op2)) XBYAK_THROW(ERR_NOT_SUPPORTED)
	}
	static inline uint8_t rexRXB(int bit, int bit3, const Reg& r, const Reg& b, const Reg& x = Reg())
	{
		int v = bit3 ? 8 : 0;
		if (r.hasIdxBit(bit)) v |= 4;
		if (x.hasIdxBit(bit)) v |= 2;
		if (b.hasIdxBit(bit)) v |= 1;
		return uint8_t(v);
	}
	void rex2(int bit3, int w, const Reg& r, const Reg& b, const Reg& x = Reg())
	{
		db(0xD5);
		db((rexRXB(4, bit3, r, b, x) << 4) | rexRXB(3, w, r, b, x));
	}
	// emit REX2 or REX prefix for (reg, base, index) and return true if rex2 is selected
	bool setRex(int w, const Reg& r, const Reg& b, const Reg& x, uint64_t type)
	{
		uint8_t rex = rexRXB(3, w, r, b, x);
		if (r.hasRex2() || b.hasRex2() || x.hasRex2()) {
			uint32_t map = getMap(type);
			if (map == 2 || map == 3) XBYAK_THROW_RET(ERR_CANT_USE_REX2, false)
			rex2(map == 1, w, r, b, x);
			return true;
		}
		if (rex || r.isExt8bit() || b.isExt8bit() || x.isExt8bit()) rex |= 0x40;
		if (rex) db(rex);
		return false;
	}
	// return true if rex2 is selected
	bool rex(const Operand& op1, const Operand& op2 = Operand(), uint64_t type = 0)
	{
		if (op1.getNF() | op2.getNF()) XBYAK_THROW_RET(ERR_INVALID_NF, false)
		if (op1.getZU() | op2.getZU()) XBYAK_THROW_RET(ERR_INVALID_ZU, false)
		const Operand *p1 = &op1, *p2 = &op2;
		if (p1->isMEM()) std::swap(p1, p2);
		if (p1->isMEM()) XBYAK_THROW_RET(ERR_BAD_COMBINATION, false)
		// except movsx(16bit, 32/64bit)
		bool p66 = (op1.isBit(16) && !op2.isBit(i32e)) || (op2.isBit(16) && !op1.isBit(i32e));
		if ((type & T_66) || p66) db(0x66);
		if (type & T_F2) {
			db(0xF2);
		}
		if (type & T_F3) {
			db(0xF3);
		}
		if (p2->isMEM()) {
			const Reg& r = *static_cast<const Reg*>(p1);
			const Address& addr = p2->getAddress();
			const RegExp& e = addr.getRegExp();
			if (BIT == 64 && addr.is32bit()) db(0x67);
			return setRex(r.isREG(64), r, e.getBase(), e.getIndex(), type);
		} else {
			const Reg& r1 = static_cast<const Reg&>(op1);
			const Reg& r2 = static_cast<const Reg&>(op2);
			// ModRM(reg, base);
			return setRex(r1.isREG(64) || r2.isREG(64), r2, r1, Reg(), type);
		}
	}
	// @@@begin of avx_type_def.h
	static const uint64_t T_NONE = 0ull;
	// N field (bit0-2) : disp8N = 1 << (value - 1), T_DUP is a sentinel
	static const uint64_t T_N1 = 1ull;
	static const uint64_t T_N2 = 2ull;
	static const uint64_t T_N4 = 3ull;
	static const uint64_t T_N8 = 4ull;
	static const uint64_t T_N16 = 5ull;
	static const uint64_t T_N32 = 6ull;
	static const uint64_t T_NX_MASK = 7ull;
	static const uint64_t T_DUP = T_NX_MASK; // N = (8, 32, 64)
	static const uint64_t T_N_VL = 1ull << 3; // N * (1, 2, 4) for VL
	static const uint64_t T_APX = 1ull << 4;
	// pp : one bit each (not a 2-bit field) because rex() emits 0x66 and 0xF2/0xF3 independently (e.g. crc32 uses T_66|T_F2)
	static const uint64_t T_66 = 1ull << 5; // pp = 1
	static const uint64_t T_F3 = 1ull << 6; // pp = 2
	static const uint64_t T_F2 = 1ull << 7; // pp = 3
	// map field (bit8-10) : the value is the same as the EVEX mmm field
	static const uint64_t T_0F = 1ull << 8;
	static const uint64_t T_0F38 = 2ull << 8;
	static const uint64_t T_0F3A = 3ull << 8;
	static const uint64_t T_MAP5 = 5ull << 8;
	static const uint64_t T_MAP6 = 6ull << 8;
	static const uint64_t T_MAP_MASK = 7ull << 8;
	// er/sae field (bit11-13) : an insn has at most one of these
	static const uint64_t T_ER_X = 1ull << 11; // xmm{er}
	static const uint64_t T_ER_Y = 2ull << 11; // ymm{er}
	static const uint64_t T_ER_Z = 3ull << 11; // zmm{er}
	static const uint64_t T_ER_R = 4ull << 11; // reg{er}
	static const uint64_t T_SAE_X = 5ull << 11; // xmm{sae}
	static const uint64_t T_SAE_Y = 6ull << 11; // ymm{sae}
	static const uint64_t T_SAE_Z = 7ull << 11; // zmm{sae}
	static const uint64_t T_ER_SAE_MASK = 7ull << 11;
	static const uint64_t T_W0 = 1ull << 14; // T_EW0 = T_W0
	static const uint64_t T_W1 = 1ull << 15; // for VEX
	static const uint64_t T_EW1 = 1ull << 16; // for EVEX
	static const uint64_t T_L1 = 1ull << 17;
	static const uint64_t T_YMM = 1ull << 18; // support YMM, ZMM
	// evex field (bit19-20) : which encodings the insn has
	static const uint64_t T_EVEX = 1ull << 19; // both VEX and EVEX
	static const uint64_t T_MUST_EVEX = 2ull << 19; // EVEX only
	static const uint64_t T_EVEX_IF_MEM = 3ull << 19; // both, but the mem operand form exists only in EVEX
	static const uint64_t T_EVEX_MASK = 3ull << 19;
	// broadcast field (bit21-22)
	static const uint64_t T_B32 = 1ull << 21; // m32bcst
	static const uint64_t T_B64 = 2ull << 21; // m64bcst
	static const uint64_t T_B16 = T_B32 | T_B64; // m16bcst
	static const uint64_t T_M_K = 1ull << 23; // mem{k}
	static const uint64_t T_VSIB = 1ull << 24;
	static const uint64_t T_NF = 1ull << 25; // T_nf
	static const uint64_t T_OP_W1 = 1ull << 26; // opcode bit0 is the w (operand-size) bit; code|=1 unless the operand is 8-bit
	static const uint64_t T_ND1 = 1ull << 27; // ND=1
	static const uint64_t T_ZU = 1ull << 28; // ND=ZU
	static const uint64_t T_ALLOW_DIFF_SIZE = 1ull << 29; // allow difference reg size
	static const uint64_t T_ALLOW_ABCDH = 1ull << 30; // allow [abcd]h reg
	// T_66 = 1, T_F3 = 2, T_F2 = 3
	static inline uint32_t getPP(uint64_t type) { return (type & T_66) ? 1 : (type & T_F3) ? 2 : (type & T_F2) ? 3 : 0; }
	// @@@end of avx_type_def.h
	static inline uint32_t getMap(uint64_t type) { return uint32_t((type & T_MAP_MASK) >> 8); }
	void vex(const Reg& reg, const Reg& base, const Operand *v, uint64_t type, int code, bool x = false)
	{
		int w = (type & T_W1) ? 1 : 0;
		bool is256 = (type & T_L1) ? true : reg.isYMM();
		bool r = reg.isExtIdx();
		bool b = base.isExtIdx();
		int idx = v ? v->getIdx() : 0;
		if ((idx | reg.getIdx() | base.getIdx()) >= 16) XBYAK_THROW(ERR_BAD_COMBINATION)
		uint32_t pp = getPP(type);
		uint32_t vvvv = (((~idx) & 15) << 3) | (is256 ? 4 : 0) | pp;
		if (!b && !x && !w && getMap(type) == 1) {
			db(0xC5); db((r ? 0 : 0x80) | vvvv);
		} else {
			uint32_t mmmm = getMap(type);
			db(0xC4); db((r ? 0 : 0x80) | (x ? 0 : 0x40) | (b ? 0 : 0x20) | mmmm); db((w << 7) | vvvv);
		}
		db(code);
	}
	void verifySAE(const Reg& r, uint64_t type) const
	{
		uint64_t v = type & T_ER_SAE_MASK;
		if ((v == T_SAE_X && r.isXMM()) || (v == T_SAE_Y && r.isYMM()) || (v == T_SAE_Z && r.isZMM())) return;
		XBYAK_THROW(ERR_SAE_IS_INVALID)
	}
	void verifyER(const Reg& r, uint64_t type) const
	{
		uint64_t v = type & T_ER_SAE_MASK;
		if (v == T_ER_R && r.isREG(32|64)) return;
		if ((v == T_ER_X && r.isXMM()) || (v == T_ER_Y && r.isYMM()) || (v == T_ER_Z && r.isZMM())) return;
		XBYAK_THROW(ERR_ER_IS_INVALID)
	}
	// (a, b, c) contains non zero two or three values then err
	int verifyDuplicate(int a, int b, int c, int err)
	{
		int v = a | b | c;
		if ((a > 0 && a != v) + (b > 0 && b != v) + (c > 0 && c != v) > 0) XBYAK_THROW_RET(err, 0)
		return v;
	}
	int evex(const Reg& reg, const Reg& base, const Operand *v, uint64_t type, int code, const Address *addr = 0)
	{
		const Reg *x = addr ? &addr->getRegExp().getIndex() : 0;
		int aaa = addr ? addr->getOpmaskIdx() : 0;
		if (aaa && !(type & T_M_K)) XBYAK_THROW_RET(ERR_INVALID_OPMASK_WITH_MEMORY, 0)
		bool b = false;
		if (addr && addr->isBroadcast()) {
			if (!(type & (T_B32 | T_B64))) XBYAK_THROW_RET(ERR_INVALID_BROADCAST, 0)
			b = true;
		}
		if (!(type & T_EVEX_MASK)) XBYAK_THROW_RET(ERR_EVEX_IS_INVALID, 0)
		int w = (type & T_EW1) ? 1 : 0;
		uint32_t mmm = getMap(type);
		uint32_t pp = getPP(type);
		int idx = v ? v->getIdx() : 0;
		uint32_t vvvv = ~idx;

		bool R = reg.isExtIdx();
		bool X3 = (x && x->isExtIdx()) || (base.isSIMD() && base.isExtIdx2());
		uint8_t B4 = (base.isREG() && base.isExtIdx2()) ? 8 : 0;
		uint8_t U = (x && (x->isREG() && x->isExtIdx2())) ? 0 : 4;
		bool B = base.isExtIdx();
		bool Rp = reg.isExtIdx2();
		int LL;
		int rounding = verifyDuplicate(reg.getRounding(), base.getRounding(), v ? v->getRounding() : 0, ERR_ROUNDING_IS_ALREADY_SET);
		int disp8N = 1;
		if (rounding) {
			if (rounding == EvexModifierRounding::T_SAE) {
				verifySAE(base, type); LL = 0;
			} else {
				verifyER(base, type); LL = rounding - 1;
			}
			b = true;
		} else {
			uint32_t VL = (x && x->isSIMD()) ? x->getBit() : 0; // vsib
			if (v) VL = (std::max)(VL, v->getBit());
			VL = (std::max)((std::max)(reg.getBit(), base.getBit()), VL);
			LL = (VL >= 512 /* tmm */) ? 2 : (VL == 256) ? 1 : 0;
			if (b) {
				disp8N = ((type & T_B16) == T_B16) ? 2 : (type & T_B32) ? 4 : 8;
			} else if ((type & T_NX_MASK) == T_DUP) {
				disp8N = VL == 128 ? 8 : VL == 256 ? 32 : 64;
			} else {
				if ((type & (T_NX_MASK | T_N_VL)) == 0) {
					type |= T_N16 | T_N_VL; // default
				}
				int low = type & T_NX_MASK;
				if (low > 0) {
					disp8N = 1 << (low - 1);
					if (type & T_N_VL) disp8N *= (VL == 512 ? 4 : VL == 256 ? 2 : 1);
				}
			}
		}
		bool V4 = (v && v->isExtIdx2()) || (x && x->isSIMD() && x->isExtIdx2());
		bool z = reg.hasZero() || base.hasZero() || (v ? v->hasZero() : false);
		if (aaa == 0) aaa = verifyDuplicate(base.getOpmaskIdx(), reg.getOpmaskIdx(), (v ? v->getOpmaskIdx() : 0), ERR_OPMASK_IS_ALREADY_SET);
		if (aaa == 0) z = 0; // clear T_z if mask is not set
		db(0x62);
		db((R ? 0 : 0x80) | (X3 ? 0 : 0x40) | (B ? 0 : 0x20) | (Rp ? 0 : 0x10) | B4 | mmm);
		db((w == 1 ? 0x80 : 0) | ((vvvv & 15) << 3) | U | (pp & 3));
		db((z ? 0x80 : 0) | ((LL & 3) << 5) | (b ? 0x10 : 0) | (V4 ? 0 : 8) | (aaa & 7));
		db(code);
		return disp8N;
	}
	// evex of Legacy
	void evexLeg(const Reg& r, const Reg& b, const Reg& x, const Reg& v, uint64_t type, int sc = NONE)
	{
		int M = getMap(type); if (M == 0) M = 4; // legacy
		int R3 = !r.isExtIdx();
		int X3 = !x.isExtIdx();
		int B3 = b.isExtIdx() ? 0 : 0x20;
		int R4 = r.isExtIdx2() ? 0 : 0x10;
		int B4 = b.isExtIdx2() ? 0x08 : 0;
		int w = (type & T_W0) ? 0 : (r.isBit(64) || v.isBit(64) || (type & T_W1));
		int V = (~v.getIdx() & 15) << 3;
		int X4 = x.isExtIdx2() ? 0 : 0x04;
		int pp = (type & (T_F2|T_F3|T_66)) ? getPP(type) : (r.isBit(16) || v.isBit(16));
		int V4 = !v.isExtIdx2();
		int ND = (type & T_ZU) ? (r.getZU() || b.getZU()) : (type & T_ND1) ? 1 : (type & T_APX) ? 0 : v.isREG();
		int NF = r.getNF() | b.getNF() | x.getNF() | v.getNF();
		int L = 0;
		if ((type & T_NF) == 0 && NF) XBYAK_THROW(ERR_INVALID_NF)
		if ((type & T_ZU) == 0 && r.getZU()) XBYAK_THROW(ERR_INVALID_ZU)
		db(0x62);
		db((R3<<7) | (X3<<6) | B3 | R4 | B4 | M);
		db((w<<7) | V | X4 | pp);
		if (sc != NONE) {
			db((L<<5) | (ND<<4) | sc);
		} else {
			db((L<<5) | (ND<<4) | (V4<<3) | (NF<<2));
		}
	}
	void setModRM(int mod, int r1, int r2)
	{
		db(static_cast<uint8_t>((mod << 6) | ((r1 & 7) << 3) | (r2 & 7)));
	}
	void setSIB(const Address& addr, int reg)
	{
		const RegExp& e = addr.getRegExp();
		const Label *label = e.getLabel();
		int disp8N = addr.disp8N;
		uint64_t disp64 = e.getDisp();
#if defined(XBYAK64) && !defined(__ILP32__)
#ifdef XBYAK_OLD_DISP_CHECK
		// treat 0xffffffff as 0xffffffffffffffff
		uint64_t high = disp64 >> 32;
		if (high != 0 && high != 0xFFFFFFFF) XBYAK_THROW(ERR_OFFSET_IS_TOO_BIG)
#else
		// displacement should be a signed 32-bit value, so also check sign bit
		uint64_t high = disp64 >> 31;
		if (high != 0 && high != 0x1FFFFFFFF) XBYAK_THROW(ERR_OFFSET_IS_TOO_BIG)
#endif
#endif
		uint32_t disp = static_cast<uint32_t>(disp64);
		const Reg& base = e.getBase();
		const Reg& index = e.getIndex();
		const int baseIdx = base.getIdx();
		const int baseBit = base.getBit();
		const int indexBit = index.getBit();
		enum {
			mod00 = 0, mod01 = 1, mod10 = 2
		};
		int mod = mod10; // disp32
		if (!baseBit || ((baseIdx & 7) != Operand::EBP && (label == 0 && disp == 0))) {
			mod = mod00;
		} else if (label) {
			// always disp32
		} else {
			if (disp8N == 0) {
				if (inner::IsInDisp8(disp)) {
					mod = mod01;
				}
			} else {
				// disp must be casted to signed
				uint32_t t = static_cast<uint32_t>(static_cast<int>(disp) / disp8N);
				if ((disp % disp8N) == 0 && inner::IsInDisp8(t)) {
					disp = t;
					mod = mod01;
				}
			}
		}
		const int newBaseIdx = baseBit ? (baseIdx & 7) : Operand::EBP;
		/* ModR/M = [2:3:3] = [Mod:reg/code:R/M] */
		bool hasSIB = indexBit || (baseIdx & 7) == Operand::ESP;
#ifdef XBYAK64
		if (!baseBit && !indexBit) hasSIB = true;
#endif
		if (hasSIB) {
			setModRM(mod, reg, Operand::ESP);
			/* SIB = [2:3:3] = [SS:index:base(=rm)] */
			const int idx = indexBit ? (index.getIdx() & 7) : Operand::ESP;
			const int scale = e.getScale();
			const int SS = (scale == 8) ? 3 : (scale == 4) ? 2 : (scale == 2) ? 1 : 0;
			setModRM(SS, idx, newBaseIdx);
		} else {
			setModRM(mod, reg, newBaseIdx);
		}
		if (mod == mod01) {
			db(disp);
		} else if (mod == mod10 || (mod == mod00 && !baseBit)) {
			if (label) {
				putL_inner(*label, inner::Labs, e.getDisp(), 4);
			} else {
				dd(disp);
			}
		}
	}
	LabelManager labelMgr_;
	// r is used only to determine the w bit (code|=1 unless r is 8-bit).
	// opROO passes d here, which is often Reg() (bit=0), assuming that !r.isBit(8) is true then.
	void writeCode(uint64_t type, const Reg& r, int code, bool rex2 = false)
	{
		if (!(type&T_APX || rex2)) {
			switch (getMap(type)) {
			case 1: db(0x0F); break;
			case 2: db(0x0F); db(0x38); break;
			case 3: db(0x0F); db(0x3A); break;
			default: break;
			}
		}
		db(code | (((type & T_OP_W1) != 0) && !r.isBit(8)));
	}
	void opRR(const Reg& r1, const Reg& r2, uint64_t type, int code)
	{
		if (!(type & T_ALLOW_DIFF_SIZE) && r1.isREG() && r2.isREG() && r1.getBit() != r2.getBit()) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		if (!(type & T_ALLOW_ABCDH) && (isBadCombination(r1, r2) || isBadCombination(r2, r1))) XBYAK_THROW(ERR_CANT_USE_ABCDH)
		bool rex2 = rex(r2, r1, type);
		writeCode(type, r1, code, rex2);
		setModRM(3, r1.getIdx(), r2.getIdx());
	}
	void opMR(const Address& addr, const Reg& r, uint64_t type, int code, uint64_t type2 = 0, int code2 = NONE)
	{
		if (code2 == NONE) code2 = code;
		if (type2 && opROO(Reg(), addr, r, type2, code2)) return;
		if (addr.is64bitDisp()) XBYAK_THROW(ERR_CANT_USE_64BIT_DISP)
#if XBYAK_STRICT_CHECK_MEM_REG_SIZE == 1
		if (!(type & T_ALLOW_DIFF_SIZE) && r.getBit() <= BIT && addr.getBit() > 0 && addr.getBit() != r.getBit()) XBYAK_THROW(ERR_BAD_MEM_SIZE)
#endif
		bool rex2 = rex(addr, r, type);
		writeCode(type, r, code, rex2);
		opAddr(addr, r.getIdx());
	}
	void opLoadSeg(const Address& addr, const Reg& reg, uint64_t type, int code)
	{
		if (reg.isBit(8)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		opMR(addr, reg, type, code);
	}
	// for only MPX(bnd*)
	void opMIB(const Address& addr, const Reg& reg, uint64_t type, int code)
	{
		if (addr.getMode() != inner::M_ModRM) XBYAK_THROW(ERR_INVALID_MIB_ADDRESS)
		opMR(addr.cloneNoOptimize(), reg, type, code);
	}
	void makeJmp(uint32_t disp, LabelType type, uint8_t shortCode, uint8_t longCode, uint8_t longPref)
	{
		const int shortJmpSize = 2;
		const int longHeaderSize = longPref ? 2 : 1;
		const int longJmpSize = longHeaderSize + 4;
		if (type != T_NEAR && inner::IsInDisp8(disp - shortJmpSize)) {
			db(shortCode); db(disp - shortJmpSize);
		} else {
			if (type == T_SHORT) XBYAK_THROW(ERR_LABEL_IS_TOO_FAR)
			if (longPref) db(longPref);
			db(longCode); dd(disp - longJmpSize);
		}
	}
	bool isNEAR(LabelType type) const { return type == T_NEAR || (type == T_AUTO && isDefaultJmpNEAR_); }
	template<class T>
	void opJmp(T& label, LabelType type, uint8_t shortCode, uint8_t longCode, uint8_t longPref)
	{
		if (type == T_FAR) XBYAK_THROW(ERR_NOT_SUPPORTED)
		growMemoryForJmp();
		size_t offset = 0;
		if (labelMgr_.getOffset(&offset, label)) { /* label exists */
			makeJmp(inner::VerifyInInt32(offset - size_), type, shortCode, longCode, longPref);
		} else {
			int jmpSize = 0;
			if (isNEAR(type)) {
				jmpSize = 4;
				if (longPref) db(longPref);
				db(longCode); dd(0);
			} else {
				jmpSize = 1;
				db(shortCode); db(0);
			}
			JmpLabel jmp(size_, jmpSize, inner::LasIs);
			labelMgr_.addUndefinedLabel(label, jmp);
		}
	}
	void opJmpAbs(const void *addr, LabelType type, uint8_t shortCode, uint8_t longCode, uint8_t longPref = 0)
	{
		if (type == T_FAR) XBYAK_THROW(ERR_NOT_SUPPORTED)
		if (isAutoGrow()) {
			if (!isNEAR(type)) XBYAK_THROW(ERR_ONLY_T_NEAR_IS_SUPPORTED_IN_AUTO_GROW)
			growMemoryForJmp();
			if (longPref) db(longPref);
			db(longCode);
			dd(0);
			save(size_ - 4, size_t(addr) - size_, 4, inner::LsubTop);
		} else {
			makeJmp(inner::VerifyInInt32(reinterpret_cast<const uint8_t*>(addr) - getCurr()), type, shortCode, longCode, longPref);
		}

	}
	void opJmpOp(const Operand& op, LabelType type, int ext)
	{
		const int bit = 16|i32e;
		if (type == T_FAR) {
			if (!op.isMEM(bit)) XBYAK_THROW(ERR_NOT_SUPPORTED)
			opRext(op, bit, ext + 1, 0, 0xFF, false);
		} else {
			opRext(op, bit, ext, 0, 0xFF, true);
		}
	}
	// reg is reg field of ModRM
	// immSize is the size for immediate value
	void opAddr(const Address &addr, int reg)
	{
		if (!addr.permitVsib && addr.isVsib()) XBYAK_THROW(ERR_BAD_VSIB_ADDRESSING)
		if (addr.getMode() == inner::M_ModRM) {
			setSIB(addr, reg);
		} else if (addr.getMode() == inner::M_rip || addr.getMode() == inner::M_ripAddr) {
			setModRM(0, reg, 5);
			if (addr.getLabel()) { // [rip + Label]
				putL_inner(*addr.getLabel(), inner::LasIs, addr.getDisp() - addr.immSize, 4);
			} else {
				size_t disp = addr.getDisp();
				if (addr.getMode() == inner::M_ripAddr) {
					if (isAutoGrow()) XBYAK_THROW(ERR_INVALID_RIP_IN_AUTO_GROW)
					// compute the relative offset to the pointer address
					disp -= (size_t)getCurr() + 4 + addr.immSize;
				}
				dd(inner::VerifyInInt32(disp));
			}
		}
	}
	void opSSE(const Reg& r, const Operand& op, uint64_t type, int code, bool isValid(const Operand&, const Operand&) = 0, int imm8 = NONE)
	{
		if (isValid && !isValid(r, op)) XBYAK_THROW(ERR_BAD_COMBINATION)
		verifySSE(r, op);
		opRO(r, op, type, code, true, (imm8 != NONE) ? 1 : 0);
		if (imm8 != NONE) db(imm8);
	}
	void opMMX_IMM(const Mmx& mmx, int imm8, int code, int ext)
	{
		verifySSE(mmx);
		uint64_t type = T_0F;
		if (mmx.isXMM()) type |= T_66;
		opRR(Reg32(ext), mmx, type, code);
		db(imm8);
	}
	void opMMX(const Mmx& mmx, const Operand& op, int code, uint64_t type = T_0F, uint64_t pref = T_66, int imm8 = NONE)
	{
		if (mmx.isXMM()) type |= pref;
		opSSE(mmx, op, type, code, isXMMorMMX_MEM, imm8);
	}
	void opMovXMM(const Operand& op1, const Operand& op2, uint64_t type, int code)
	{
		verifySSE(op1, op2);
		if (op1.isXMM() && op2.isMEM()) {
			opMR(op2.getAddress(), op1.getReg(), type, code);
		} else if (op1.isMEM() && op2.isXMM()) {
			opMR(op1.getAddress(), op2.getReg(), type, code | 1);
		} else {
			XBYAK_THROW(ERR_BAD_COMBINATION)
		}
	}
	// pextr{w,b,d}, extractps
	void opExt(const Operand& op, const Mmx& mmx, int code, int imm, bool hasMMX2 = false)
	{
		verifySSE(op, mmx);
		if (hasMMX2 && op.isREG(i32e)) { /* pextrw is special */
			if (mmx.isXMM()) db(0x66);
			opRR(op.getReg(), mmx, T_0F, 0xC5); db(imm);
		} else {
			opSSE(mmx, op, T_66 | T_0F3A, code, isXMM_REG32orMEM, imm);
		}
	}
	// r1 is [abcd]h and r2 is reg with rex
	bool isBadCombination(const Reg& r1, const Reg& r2) const
	{
		if (!r1.isHigh8bit()) return false;
		if (r2.isExt8bit() || r2.getIdx() >= 8) return true;
		return false;
	}
	// (r, r, m) or (r, m, r)
	bool opROO(const Reg& d, const Operand& op1, const Operand& op2, uint64_t type, int code, int immSize = 0, int sc = NONE)
	{
		if ((type & T_EVEX_MASK) != T_MUST_EVEX && !d.isREG() && !(d.hasRex2NFZU() || op1.hasRex2NFZU() || op2.hasRex2NFZU())) return false;
		const Operand *p1 = &op1, *p2 = &op2;
		if (p1->isMEM()) { std::swap(p1, p2); } else { if (p2->isMEM()) code |= 2; }
		if (p1->isMEM()) XBYAK_THROW_RET(ERR_BAD_COMBINATION, false)
		if (p2->isMEM()) {
			const Reg& r = *static_cast<const Reg*>(p1);
			Address addr = p2->getAddress();
			const RegExp& e = addr.getRegExp();
			evexLeg(r, e.getBase(), e.getIndex(), d, type, sc);
			writeCode(type, d, code);
			addr.immSize = immSize;
			opAddr(addr, r.getIdx());
		} else {
			evexLeg(static_cast<const Reg&>(op2), static_cast<const Reg&>(op1), Reg(), d, type, sc);
			writeCode(type, d, code);
			setModRM(3, op2.getIdx(), op1.getIdx());
		}
		return true;
	}
	void opRext(const Operand& op, int bit, int ext, uint64_t type, int code, bool disableRex = false, int immSize = 0, const Reg *d = 0)
	{
		int opBit = op.getBit();
		if (disableRex && opBit == 64) opBit = 32;
		const Reg r(ext, Operand::REG, opBit);
		// EVEX is required only for ND/NF/ZU; a plain EGPR is encodable with the shorter REX2
		if ((type & T_APX) && (d != 0 || op.getNF() || op.getZU()) && opROO(d ? *d : Reg(0, Operand::REG, opBit), op, r, type, code)) return;
		if (op.isMEM()) {
			opMR(op.getAddress(immSize), r, type, code);
		} else if (op.isREG(bit)) {
			opRR(r, op.getReg().changeBit(opBit), type | T_ALLOW_ABCDH, code);
		} else {
			XBYAK_THROW(ERR_BAD_COMBINATION)
		}
	}
	void opSetCC(const Operand& op, int ext)
	{
		// EVEX (opcode 0x40|ext) is required only for ZU; a plain EGPR is encodable with the shorter REX2
		if (op.getZU() && opROO(Reg(), op, Reg(), T_APX|T_ZU|T_F2, 0x40 | ext)) return;
		opRext(op, 8, 0, T_0F, 0x90 | ext);
	}
	void opShiftCore(const Operand& op, int ext, const Reg *d, int code, int immSize)
	{
		if (d && op.getBit() != 0 && d->getBit() != op.getBit()) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		uint64_t type = T_APX|T_OP_W1; if (ext & 8) type |= T_NF; if (d) type |= T_ND1;
		opRext(op, 0, ext&7, type, code, false, immSize, d);
	}
	void opShift(const Operand& op, int imm, int ext, const Reg *d = 0)
	{
		if (d == 0) verifyMemHasSize(op);
		opShiftCore(op, ext, d, (0xC0 | ((imm == 1 ? 1 : 0) << 4)), (imm != 1) ? 1 : 0);
		if (imm != 1) db(imm);
	}
	void opShift(const Operand& op, const Reg8& _cl, int ext, const Reg *d = 0)
	{
		if (_cl.getIdx() != Operand::CL) XBYAK_THROW(ERR_BAD_COMBINATION)
		opShiftCore(op, ext, d, 0xD2, 0);
	}
	// condR assumes that op.isREG() is true
	void opRO(const Reg& r, const Operand& op, uint64_t type, int code, bool condR = true, int immSize = 0)
	{
		if (op.isMEM()) {
			opMR(op.getAddress(immSize), r, type, code);
		} else if (condR) {
			opRR(r, op.getReg(), type, code);
		} else {
			XBYAK_THROW(ERR_BAD_COMBINATION)
		}
	}
	void opShxd(const Reg& d, const Operand& op, const Reg& reg, uint8_t imm, int code, int code2, const Reg8 *_cl = 0)
	{
		if (_cl && _cl->getIdx() != Operand::CL) XBYAK_THROW(ERR_BAD_COMBINATION)
		if (!reg.isREG(16|i32e)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		int immSize = _cl ? 0 : 1;
		if (_cl) code |= 1;
		uint64_t type = T_APX | T_NF;
		if (d.isREG()) type |= T_ND1;
		if (!opROO(d, op, reg, type, _cl ? code : code2, immSize)) {
			opRO(reg, op, T_0F, code, true, immSize);
		}
		if (!_cl) db(imm);
	}
	// (REG, REG|MEM), (MEM, REG)
	void opRO_MR(const Operand& op1, const Operand& op2, int code)
	{
		if (op2.isMEM()) {
			if (!op1.isREG()) XBYAK_THROW(ERR_BAD_COMBINATION)
			opMR(op2.getAddress(), op1.getReg(), T_OP_W1, code | 2);
		} else {
			opRO(static_cast<const Reg&>(op2), op1, T_OP_W1, code, op1.getKind() == op2.getKind());
		}
	}
	// allow add(ax, 0x8000);
	bool isInDisp16relaxed(uint32_t x) const { uint32_t v = x & 0xffff0000; return v == 0 || v == 0xffff0000; }
	// size of imm to be encoded for op (8, 16 or 32)
	uint32_t getImmBit(const Operand& op, uint32_t imm)
	{
		verifyMemHasSize(op);
		if (op.isBit(8)) return 8;
		uint32_t immBit = inner::IsInDisp8(imm) ? 8 : isInDisp16relaxed(imm) ? 16 : 32;
		if (op.getBit() < immBit) XBYAK_THROW_RET(ERR_IMM_IS_TOO_BIG, 0)
		if (immBit == 16 && op.isBit(32|64)) immBit = 32; // don't use imm16 for 32/64-bit ops
		return immBit;
	}
	// s bit of the 0x80 group : 0x83 = r/m, imm8 (sign-extended)
	int getSbit(const Operand& op, uint32_t immBit) const { return (immBit == 8 && !op.isBit(8)) ? 2 : 0; }
	// (REG|MEM, IMM)
	void opOI(const Operand& op, uint32_t imm, int code, int ext)
	{
		uint32_t immBit = getImmBit(op, imm);
		if (op.isREG() && op.getIdx() == 0 && immBit == (op.isBit(64) ? 32U : op.getBit())) { // short form for al/ax/eax/rax
			rex(op);
			db(code | 4 | (immBit == 8 ? 0 : 1));
		} else {
			opRext(op, 0, ext, T_OP_W1, 0x80 | getSbit(op, immBit), false, immBit / 8);
		}
		db(imm, immBit / 8);
	}
	// (r, r/m, imm)
	void opROI(const Reg& d, const Operand& op, uint32_t imm, uint64_t type, int ext, int sc = NONE)
	{
		uint32_t immBit = getImmBit(d, imm);
		opROO(d, op, Reg(ext, Operand::REG, d.getBit()), type, 0x80 | getSbit(d, immBit), immBit / 8, sc);
		db(imm, immBit / 8);
	}
	void opIncDec(const Reg& d, const Operand& op, int ext)
	{
#ifdef XBYAK64
		if (d.isREG()) {
			opROO(d, op, Reg(ext, Operand::REG, d.getBit()), T_APX|T_NF|T_ND1|T_OP_W1, 0xFE);
			return;
		}
#else
		(void)d;
#endif
		verifyMemHasSize(op);
#ifndef XBYAK64
		if (op.isREG() && !op.isBit(8)) {
			rex(op); db((ext ? 0x48 : 0x40) | op.getIdx());
			return;
		}
#endif
		opRext(op, op.getBit(), ext, T_OP_W1, 0xFE);
	}
	void opPushPop(const Operand& op, int code, int ext, int alt)
	{
		int bit = op.getBit();
		if (bit != 16 && bit != BIT) XBYAK_THROW(ERR_BAD_COMBINATION)
		if (bit == 16) db(0x66);
		if (op.isREG()) {
			setRex(0, Reg(), op.getReg(), Reg(), 0); // 0x41 or REX2 if necessary
			db(alt | (op.getIdx() & 7));
		} else if (op.isMEM()) {
			opMR(op.getAddress(), Reg(ext, Operand::REG, 32), T_ALLOW_DIFF_SIZE, code);
		} else {
			XBYAK_THROW(ERR_BAD_COMBINATION)
		}
	}
#ifdef XBYAK64
	// PUSHP/POPP : REX2 is mandatory (carries the W=1 PPX hint),
	// unlike ordinary push/pop where REX2 is only emitted for R16-31.
	void opPushPopP(const Reg64& r, int alt)
	{
		rex2(0, 1, Reg(), r);
		db(alt | (r.getIdx() & 7));
	}
#endif
	void verifyMemHasSize(const Operand& op) const
	{
		if (op.isMEM() && op.getBit() == 0) XBYAK_THROW(ERR_MEM_SIZE_IS_NOT_SPECIFIED)
	}
	/*
		mov(r, imm) = db(imm, mov_imm(r, imm))
	*/
	int mov_imm(const Reg& reg, uint64_t imm)
	{
		int bit = reg.getBit();
		const int idx = reg.getIdx();
		int code = 0xB0 | ((bit == 8 ? 0 : 1) << 3);
		if (bit == 64 && (imm & ~uint64_t(0xffffffffu)) == 0) {
			rex(Reg32(idx));
			bit = 32;
		} else {
			rex(reg);
			if (bit == 64 && inner::IsInInt32(imm)) {
				db(0xC7);
				code = 0xC0;
				bit = 32;
			}
		}
		db(code | (idx & 7));
		return bit / 8;
	}
	/*
		write (label + disp) as jmpSize bytes
		mode : LasIs (rip-relative offset), Labs (absolute address; replaced by LaddTop in AutoGrow mode)
	*/
	template<class T>
	void putL_inner(T& label, inner::LabelMode mode, size_t disp, int jmpSize)
	{
		growMemoryForJmp();
		if (mode == inner::Labs && isAutoGrow()) mode = inner::LaddTop;
		size_t offset = 0;
		if (labelMgr_.getOffset(&offset, label)) {
			offset += disp;
			if (mode == inner::LasIs) {
				db(inner::VerifyInInt32(offset - size_ - jmpSize), jmpSize);
			} else if (mode == inner::LaddTop) {
				db(uint64_t(0), jmpSize);
				save(size_ - jmpSize, offset, jmpSize, mode);
			} else {
				db(size_t(top_) + offset, jmpSize);
			}
			return;
		}
		db(uint64_t(0), jmpSize);
		labelMgr_.addUndefinedLabel(label, JmpLabel(size_, jmpSize, mode, disp));
	}
	void opMovxx(const Reg& reg, const Operand& op, uint8_t code)
	{
		if (op.isBit(32)) XBYAK_THROW(ERR_BAD_COMBINATION)
		int w = op.isBit(16);
		if (!(reg.isREG() && (reg.getBit() > op.getBit()))) XBYAK_THROW(ERR_BAD_COMBINATION)
		opRO(reg, op, T_0F | T_ALLOW_DIFF_SIZE, code | w);
	}
	void opFpuMem(const Address& addr, uint8_t m16, uint8_t m32, uint8_t m64, uint8_t ext, uint8_t m64ext)
	{
		if (addr.is64bitDisp()) XBYAK_THROW(ERR_CANT_USE_64BIT_DISP)
		uint8_t code = addr.isBit(16) ? m16 : addr.isBit(32) ? m32 : addr.isBit(64) ? m64 : 0;
		if (!code) XBYAK_THROW(ERR_BAD_MEM_SIZE)
		if (m64ext && addr.isBit(64)) ext = m64ext;
		rex(addr, st0);
		db(code);
		opAddr(addr, ext);
	}
	// use code1 if reg1 == st0
	// use code2 if reg1 != st0 && reg2 == st0
	void opFpuFpu(const Fpu& reg1, const Fpu& reg2, uint32_t code1, uint32_t code2)
	{
		uint32_t code = reg1.getIdx() == 0 ? code1 : reg2.getIdx() == 0 ? code2 : 0;
		if (!code) XBYAK_THROW(ERR_BAD_ST_COMBINATION)
		db(uint8_t(code >> 8));
		db(uint8_t(code | (reg1.getIdx() | reg2.getIdx())));
	}
	void opFpu(const Fpu& reg, uint8_t code1, uint8_t code2)
	{
		db(code1); db(code2 | reg.getIdx());
	}
	void opVex(const Reg& r, const Operand *p1, const Operand& op2, uint64_t type, int code, int imm8 = NONE)
	{
		const bool useEvex = (type & T_EVEX_MASK) == T_MUST_EVEX || r.hasEvex() || (p1 && p1->hasEvex());
		if (op2.isMEM()) {
			// zeroing-masking has no meaning when the destination is memory
			if ((type & T_M_K) && (r.hasZero() || (p1 && p1->hasZero()) || op2.hasZero())) XBYAK_THROW(ERR_INVALID_ZERO)
			Address addr = op2.getAddress();
			const RegExp& regExp = addr.getRegExp();
			const Reg& base = regExp.getBase();
			const Reg& index = regExp.getIndex();
			if (BIT == 64 && addr.is32bit()) db(0x67);
			if (useEvex || (type & T_EVEX_MASK) == T_EVEX_IF_MEM || addr.isBroadcast() || addr.getOpmaskIdx() || addr.hasRex2()) {
				addr.disp8N = evex(r, base, p1, type, code, &addr);
			} else {
				vex(r, base, p1, type, code, index.isExtIdx());
			}
			if (type & T_VSIB) addr.permitVsib = true;
			if (imm8 != NONE) addr.immSize = 1;
			opAddr(addr, r.getIdx());
		} else {
			const Reg& base = op2.getReg();
			if (useEvex || base.hasEvex()) {
				evex(r, base, p1, type, code);
			} else {
				vex(r, base, p1, type, code);
			}
			setModRM(3, r.getIdx(), base.getIdx());
		}
		if (imm8 != NONE) db(imm8);
	}
	// (r, r, r/m)
	// opRRO(a, b, c) == opROO(b, c, a)
	void opRRO(const Reg& d, const Reg& r1, const Operand& op2, uint64_t type, uint8_t code, int imm8 = NONE)
	{
		const unsigned int bit = d.getBit();
		if (r1.getBit() != bit || (op2.isREG() && op2.getBit() != bit)) XBYAK_THROW(ERR_BAD_COMBINATION)
		type |= (bit == 64) ? T_W1 : T_W0;
		if (d.hasRex2() || r1.hasRex2() || op2.hasRex2() || d.getNF()) {
			opROO(r1, op2, d, type, code);
			if (imm8 != NONE) db(imm8);
		} else {
			opVex(d, &r1, op2, type, code, imm8);
		}
	}
	void opAVX_X_X_XM(const Xmm& x1, const Operand& op1, const Operand& op2, uint64_t type, int code, int imm8 = NONE)
	{
		const Xmm *x2 = static_cast<const Xmm*>(&op1);
		const Operand *op = &op2;
		if (op2.isNone()) { // (x1, op1) -> (x1, x1, op1)
			x2 = &x1;
			op = &op1;
		}
		// (x1, x2, op)
		if (!((x1.isXMM() && x2->isXMM()) || ((type & T_YMM) && ((x1.isYMM() && x2->isYMM()) || (x1.isZMM() && x2->isZMM()))))) XBYAK_THROW(ERR_BAD_COMBINATION)
		opVex(x1, x2, *op, type, code, imm8);
	}
	void opAVX_K_X_XM(const Opmask& k, const Xmm& x2, const Operand& op3, uint64_t type, int code, int imm8 = NONE)
	{
		if (!op3.isMEM() && (x2.getKind() != op3.getKind())) XBYAK_THROW(ERR_BAD_COMBINATION)
		opVex(k, &x2, op3, type, code, imm8);
	}
	void opCvt(const Xmm& x, const Operand& op, uint64_t type, int code)
	{
		Operand::Kind kind = x.isXMM() ? (op.isBit(256) ? Operand::YMM : Operand::XMM) : Operand::ZMM;
		opVex(x.copyAndSetKind(kind), &xm0, op, type, code);
	}
	// xx_xy_xz
	void opX_XM(const Operand& op, const Xmm& x, uint64_t type, uint8_t code)
	{
		if (!op.isMEM() && !op.isXMM()) XBYAK_THROW(ERR_BAD_COMBINATION)
		opVex(x, 0, op, type, code);
	}
	// opCvt_ab_cd_ef lists the accepted (r/m, reg) kind pairs with x/y/z = XMM/YMM/ZMM
	// (x, x/m), (y, x/m256), (z, y/m) : e.g. vcvtdq2pd, vpmovdw
	void opCvt_xx_xy_yz(const Xmm& x, const Operand& op, uint64_t type, int code, int imm8 = NONE)
	{
		if (!op.isMEM() && !(x.is(Operand::XMM | Operand::YMM) && op.isXMM()) && !(x.isZMM() && op.isYMM())) XBYAK_THROW(ERR_BAD_COMBINATION)
		opVex(x, 0, op, type, code, imm8);
	}
	// (x, x/m), (x, y/m256), (y, z/m) : e.g. vcvtpd2dq
	void opCvt_xx_yx_zy(const Xmm& x, const Operand& op, uint64_t type, int code)
	{
		if (!(x.isXMM() && op.is(Operand::XMM | Operand::YMM | Operand::MEM)) && !(x.isYMM() && op.is(Operand::ZMM | Operand::MEM))) XBYAK_THROW(ERR_BAD_COMBINATION)
		opCvt(x, op, type, code);
	}
	// (x, x, r32/r64/m) : vcvt(u)si2ss/sd/sh (scalar int to XMM)
	// type is or-merged with type64/type32, so a packed field (N, map, er/sae, broadcast) must not have
	// different non-zero values on both sides (checked by checkTypeMergeable() in the generator)
	void opCvtSi2X(const Xmm& x1, const Xmm& x2, const Operand& op, uint64_t type, uint64_t type64, uint64_t type32, uint8_t code)
	{
		if (!(x1.isXMM() && x2.isXMM() && (op.isREG(i32e) || op.isMEM()))) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		opVex(x1, &x2, op, type | (op.isBit(64) ? type64 : type32), code);
	}
	// (x, x/y/xword/yword), (y, z/m) : opCvt_xx_yx_zy with an explicitly sized mem for an xmm dst, e.g. vcvtdq2ph
	void opCvt_xx_yx_zy_sized(const Xmm& x, const Operand& op, uint64_t type, int code)
	{
		if (x.isXMM() && !op.isBit(128|256)) XBYAK_THROW(ERR_BAD_COMBINATION)
		opCvt_xx_yx_zy(x, op, type, code);
	}
	// (x, x/y/z/xword/yword/zword) : e.g. vcvtpd2ph
	void opCvt_xx_yx_zx(const Xmm& x, const Operand& op, uint64_t type, int code)
	{
		if (!(x.isXMM() && op.isBit(128|256|512))) XBYAK_THROW(ERR_BAD_COMBINATION)
		Operand::Kind kind = op.isBit(128) ? Operand::XMM : op.isBit(256) ? Operand::YMM : Operand::ZMM;
		opVex(x.copyAndSetKind(kind), &xm0, op, type, code);
	}
	// (x, x, x/m), (x, y, y/m), (y, z, z/m) : vcvtbias*
	// dstXMM = true : dst is fixed XMM regardless of VL : (x, x, x/m), (x, y, y/m), (x, z, z/m)
	void opCvtBias(const Xmm& x1, const Xmm& x2, const Operand& op, uint64_t type, int code, bool dstXMM = false)
	{
		uint32_t b2 = x2.getBit();
		uint32_t dstBit = (!dstXMM && b2 == 512) ? 256 : 128;
		if (!(x1.getBit() == dstBit && (op.isMEM() || op.getBit() == b2))) XBYAK_THROW(ERR_BAD_COMBINATION)
		opVex(x1, &x2, op, type, code);
	}
	// (r32/r64, x/m) : vcvt*2(u)si (XMM to scalar int), EVEX.W is set if r is 64-bit
	void opCvtX2Si(const Reg& r, const Operand& op, uint64_t type, int code)
	{
		opVex(r, &xm0, op, type | (r.isREG(64) ? T_EW1 : T_W0), code);
	}
	const Xmm& cvtIdx0(const Operand& x) const
	{
		return x.isZMM() ? zm0 : x.isYMM() ? ym0 : xm0;
	}
	// support (x, x/m, imm), (y, y/m, imm)
	void opAVX_X_XM_IMM(const Xmm& x, const Operand& op, uint64_t type, int code, int imm8 = NONE)
	{
		opAVX_X_X_XM(x, cvtIdx0(x), op, type, code, imm8);
	}
	void opCnt(const Reg& reg, const Operand& op, uint8_t code)
	{
		if (reg.isBit(8)) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		bool is16bit = reg.isREG(16) && (op.isREG(16) || op.isMEM());
		if (!is16bit && !(reg.isREG(i32e) && (op.isREG(reg.getBit()) || op.isMEM()))) XBYAK_THROW(ERR_BAD_COMBINATION)
		opRO(reg, op, T_F3 | T_0F, code);
	}
	void opGather(const Xmm& x1, const Address& addr, const Xmm& x2, uint64_t type, uint8_t code, int mode)
	{
		const RegExp& regExp = addr.getRegExp();
		if (!regExp.isVsib(128 | 256)) XBYAK_THROW(ERR_BAD_VSIB_ADDRESSING)
		const int y_vx_y = 0;
		const int y_vy_y = 1;
//		const int x_vy_x = 2;
		const bool isAddrYMM = regExp.getIndex().getBit() == 256;
		if (!x1.isXMM() || isAddrYMM || !x2.isXMM()) {
			bool isOK = false;
			if (mode == y_vx_y) {
				isOK = x1.isYMM() && !isAddrYMM && x2.isYMM();
			} else if (mode == y_vy_y) {
				isOK = x1.isYMM() && isAddrYMM && x2.isYMM();
			} else { // x_vy_x
				isOK = !x1.isYMM() && isAddrYMM && !x2.isYMM();
			}
			if (!isOK) XBYAK_THROW(ERR_BAD_VSIB_ADDRESSING)
		}
		int i1 = x1.getIdx();
		int i2 = regExp.getIndex().getIdx();
		int i3 = x2.getIdx();
		if (i1 == i2 || i1 == i3 || i2 == i3) XBYAK_THROW(ERR_SAME_REGS_ARE_INVALID);
		opAVX_X_X_XM(isAddrYMM ? Ymm(i1) : x1, isAddrYMM ? Ymm(i3) : x2, addr, type, code);
	}
	enum {
		xx_yy_zz = 0,
		xx_yx_zy = 1,
		xx_xy_yz = 2
	};
	void checkGather2(const Xmm& x1, const Reg& x2, int mode) const
	{
		if (x1.isXMM() && x2.isXMM()) return;
		switch (mode) {
		case xx_yy_zz: if ((x1.isYMM() && x2.isYMM()) || (x1.isZMM() && x2.isZMM())) return;
			break;
		case xx_yx_zy: if ((x1.isYMM() && x2.isXMM()) || (x1.isZMM() && x2.isYMM())) return;
			break;
		case xx_xy_yz: if ((x1.isXMM() && x2.isYMM()) || (x1.isYMM() && x2.isZMM())) return;
			break;
		}
		XBYAK_THROW(ERR_BAD_VSIB_ADDRESSING)
	}
	void opGather2(const Xmm& x, const Address& addr, uint64_t type, uint8_t code, int mode)
	{
		if (x.hasZero()) XBYAK_THROW(ERR_INVALID_ZERO)
		const RegExp& regExp = addr.getRegExp();
		checkGather2(x, regExp.getIndex(), mode);
		int maskIdx = x.getOpmaskIdx();
		if ((type & T_M_K) && addr.getOpmaskIdx()) maskIdx = addr.getOpmaskIdx();
		if (maskIdx == 0) XBYAK_THROW(ERR_K0_IS_INVALID);
		if (!(type & T_M_K) && x.getIdx() == regExp.getIndex().getIdx()) XBYAK_THROW(ERR_SAME_REGS_ARE_INVALID);
		opVex(x, 0, addr, type, code);
	}
	void opGatherFetch(const Address& addr, const Xmm& x, uint64_t type, uint8_t code, Operand::Kind kind)
	{
		if (addr.hasZero()) XBYAK_THROW(ERR_INVALID_ZERO)
		if (addr.getRegExp().getIndex().getKind() != kind) XBYAK_THROW(ERR_BAD_VSIB_ADDRESSING)
		opVex(x, 0, addr, type, code);
	}
	// type is or-merged with typeVex/typeEvex, so a packed field (N, map, er/sae, broadcast) must not have
	// different non-zero values on both sides (checked by checkTypeMergeable() in the generator)
	void opEncoding(const Xmm& x1, const Xmm& x2, const Operand& op, uint64_t type, int code, PreferredEncoding enc, int imm = NONE, uint64_t typeVex = 0, uint64_t typeEvex = 0, int sel = 0)
	{
		opAVX_X_X_XM(x1, x2, op, type | orEvexIf(enc, typeVex, typeEvex, sel), code, imm);
	}
	PreferredEncoding getEncoding(PreferredEncoding enc, int sel) const
	{
		if (enc == DefaultEncoding) {
			enc = defaultEncoding_[sel];
		}
		if ((sel == 0 && enc != VexEncoding && enc != EvexEncoding) || (sel == 1 && enc != PreAVX10v2Encoding && enc != AVX10v2Encoding)) XBYAK_THROW_RET(ERR_BAD_ENCODING_MODE, VexEncoding)
#ifdef XBYAK_DISABLE_AVX512
		if (enc == EvexEncoding || enc == AVX10v2Encoding) XBYAK_THROW_RET(ERR_EVEX_IS_INVALID, VexEncoding)
#endif
		return enc;
	}
	uint64_t orEvexIf(PreferredEncoding enc, uint64_t typeVex, uint64_t typeEvex, int sel) {
		enc = getEncoding(enc, sel);
		return ((sel == 0 && enc == VexEncoding) || (sel == 1 && enc != AVX10v2Encoding)) ? typeVex : (T_MUST_EVEX | typeEvex);
	}
	void opInOut(const Reg& a, uint8_t code)
	{
		switch (a.getBit()) {
		case 8: db(code); return;
		case 16: db(0x66); db(code + 1); return;
		case 32: db(code + 1); return;
		}
		XBYAK_THROW(ERR_BAD_COMBINATION)
	}
	void opInOut(const Reg& a, const Reg& d, uint8_t code)
	{
		if (!(a.getIdx() == Operand::AL && d.getIdx() == Operand::DX && d.getBit() == 16)) XBYAK_THROW(ERR_BAD_COMBINATION)
		opInOut(a, code);
	}
	void opInOut(const Reg& a, uint8_t code, uint8_t v)
	{
		if (a.getIdx() != Operand::AL) XBYAK_THROW(ERR_BAD_COMBINATION)
		opInOut(a, code);
		db(v);
	}
	void verifyDfv(int dfv) const
	{
		if (dfv < 0 || 15 < dfv) XBYAK_THROW(ERR_INVALID_DFV)
	}
	void opCcmp(const Operand& op1, const Operand& op2, int dfv, int code, int sc) // cmp = 0x38, test = 0x84
	{
		verifyDfv(dfv);
		opROO(Reg(15 - dfv, Operand::REG, (op1.getBit() | op2.getBit())), op1, op2, T_APX|T_OP_W1, code, 0, sc);
	}
	void opCcmpi(const Operand& op, int imm, int dfv, int sc)
	{
		verifyDfv(dfv);
		verifyMemHasSize(op);
		opROI(Reg(15 - dfv, Operand::REG, op.getBit()), op, imm, T_APX|T_OP_W1, 15, sc);
	}
	void opTesti(const Operand& op, int imm, int dfv, int sc)
	{
		verifyDfv(dfv);
		uint32_t opBit = op.getBit();
		if (opBit == 0) XBYAK_THROW(ERR_MEM_SIZE_IS_NOT_SPECIFIED);
		int immBit = (std::min)(opBit, 32U);
		opROO(Reg(15 - dfv, Operand::REG, opBit), op, Reg(0, Operand::REG, opBit), T_APX|T_OP_W1, 0xF6, immBit / 8, sc);
		db(imm, immBit / 8);
	}
	void opCfcmov(const Reg& d, const Operand& op1, const Operand& op2, int code)
	{
		const int dBit = d.getBit();
		const int op2Bit = op2.getBit();
		if (dBit > 0 && op2Bit > 0 && dBit != op2Bit) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		if (op1.isBit(8) || op2Bit == 8) XBYAK_THROW(ERR_BAD_SIZE_OF_REGISTER)
		if (op2.isMEM()) {
			if (op1.isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION)
			uint64_t type = dBit > 0 ? (T_MUST_EVEX|T_NF) : T_MUST_EVEX;
			opROO(d, op2, op1, type, code);
		} else {
			opROO(d, op1, static_cast<const Reg&>(op2)|T_nf, T_MUST_EVEX|T_NF, code);
		}
	}
#ifdef XBYAK64
	void opAMX(const Tmm& t1, const Address& addr, uint64_t type, int code)
	{
		Address addr2 = addr.cloneNoOptimize();
		// require both base and index for all but opcode 0x49 (ldtilecfg/sttilecfg)
		if (code != 0x49) {
			const RegExp& exp = addr2.getRegExp();
			if (exp.getBase().getBit() == 0 || exp.getIndex().getBit() == 0) XBYAK_THROW(ERR_NOT_SUPPORTED)
		}
		if (opROO(Reg(), addr2, t1, T_APX|type, code)) return;
		opVex(t1, &tmm0, addr2, type, code);
	}
#endif
	// (reg32e/mem, k) if rev else (k, k/mem/reg32e)
	// size = 8, 16, 32, 64
	void opKmov(const Opmask& k, const Operand& op, bool rev, int size)
	{
		int code = 0;
		bool isReg = op.isREG(size < 64 ? 32 : 64);
		if (rev) {
			code = isReg ? 0x93 : op.isMEM() ? 0x91 : 0;
		} else {
			code = op.isOPMASK() || op.isMEM() ? 0x90 : isReg ? 0x92 : 0;
		}
		if (code == 0) XBYAK_THROW(ERR_BAD_COMBINATION)
		uint64_t type = T_0F;
		switch (size) {
		case 8:  type |= T_W0|T_66; break;
		case 16: type |= T_W0; break;
		case 32: type |= isReg ? T_W0|T_F2 : T_W1|T_66; break;
		case 64: type |= isReg ? T_W1|T_F2 : T_W1; break;
		}
		const Operand *p1 = &k, *p2 = &op;
		if (code == 0x93) { std::swap(p1, p2); }
		if (opROO(Reg(), *p2, *p1, T_APX|type, code)) return;
		opVex(static_cast<const Reg&>(*p1), 0, *p2, type, code);
	}
	// AVX10 zero-extending for vmovd, vmovw
	void opAVX10ZeroExt(const Operand& op1, const Operand& op2, const uint64_t typeTbl[4], const int codeTbl[4], PreferredEncoding enc, int bit)
	{
		const Operand *p1 = &op1;
		const Operand *p2 = &op2;
		bool rev = false;
		if (p1->isMEM()) {
			std::swap(p1, p2);
			rev = true;
		}
		if (p1->isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION)
		if (p1->isXMM()) {
			std::swap(p1, p2);
			rev = !rev;
		}
		enc = getEncoding(enc, 1);
		int sel = -1;
		if (p1->isXMM() || (p1->isMEM() && enc == AVX10v2Encoding)) {
			sel = 2 + int(rev);
		} else if (p1->isREG(bit) || p1->isMEM()) {
			sel = int(rev);
		}
		if (sel == -1) XBYAK_THROW(ERR_BAD_COMBINATION)
		opAVX_X_X_XM(*static_cast<const Xmm*>(p2), xm0, *p1, typeTbl[sel], codeTbl[sel]);
	}
public:
	unsigned int getVersion() const { return VERSION; }
	using CodeArray::db;
#ifdef XBYAK_USE_CONSTEXPR_REGISTERS
	#define XBYAK_DEFINE_REGISTER(Type, name, ...) static constexpr Type name{__VA_ARGS__};
#else
	#define XBYAK_DEFINE_REGISTER(Type, name, ...) const Type name;
#endif
	XBYAK_FOR_EACH_REGISTER(XBYAK_DEFINE_REGISTER)
	XBYAK_FOR_EACH_CONVENIENCE_ALL(XBYAK_DEFINE_REGISTER)
	#undef XBYAK_DEFINE_REGISTER
private:
	bool isDefaultJmpNEAR_;
	PreferredEncoding defaultEncoding_[2]; // 0:vnni, 1:vmpsadbw
public:
	void L(const std::string& label) { labelMgr_.defineSlabel(label); }
	void L(Label& label) { labelMgr_.defineClabel(label); }
	Label L() { Label label; L(label); return label; }
	void inLocalLabel() { labelMgr_.enterLocal(); }
	void outLocalLabel() { labelMgr_.leaveLocal(); }
	/*
		assign src to dst
		require
		dst : does not used by L()
		src : used by L()
	*/
	void assignL(Label& dst, const Label& src) { labelMgr_.assign(dst, src); }
	/*
		put address of label to buffer
		@note the put size is 4(32-bit), 8(64-bit)
	*/
	void putL(std::string label) { putL_inner(label, inner::Labs, 0, (int)sizeof(size_t)); }
	void putL(const Label& label) { putL_inner(label, inner::Labs, 0, (int)sizeof(size_t)); }

	// set default type of `jmp` of undefined label to T_NEAR
	void setDefaultJmpNEAR(bool isNear) { isDefaultJmpNEAR_ = isNear; }
	void jmp(const Operand& op, LabelType type = T_AUTO) { opJmpOp(op, type, 4); }
	void jmp(std::string label, LabelType type = T_AUTO) { opJmp(label, type, 0xEB, 0xE9, 0); }
	void jmp(const char *label, LabelType type = T_AUTO) { jmp(std::string(label), type); }
	void jmp(const Label& label, LabelType type = T_AUTO) { opJmp(label, type, 0xEB, 0xE9, 0); }
	void jmp(const void *addr, LabelType type = T_AUTO) { opJmpAbs(addr, type, 0xEB, 0xE9); }

	void call(const Operand& op, LabelType type = T_AUTO) { opJmpOp(op, type, 2); }
	// call(string label), not const std::string&
	void call(std::string label) { opJmp(label, T_NEAR, 0, 0xE8, 0); }
	void call(const char *label) { call(std::string(label)); }
	void call(const Label& label) { opJmp(label, T_NEAR, 0, 0xE8, 0); }
	// call(function pointer)
#ifdef XBYAK_VARIADIC_TEMPLATE
	template<class Ret, class... Params>
	void call(Ret(*func)(Params...)) { call(reinterpret_cast<const void*>(func)); }
#endif
	void call(const void *addr) { opJmpAbs(addr, T_NEAR, 0, 0xE8); }

	void test(const Operand& op, const Reg& reg)
	{
		opRO(reg, op, T_OP_W1, 0x84, op.getKind() == reg.getKind());
	}
	void test(const Operand& op, uint32_t imm)
	{
		verifyMemHasSize(op);
		int immSize = (std::min)(op.getBit() / 8, 4U);
		if (op.isREG() && op.getIdx() == 0) { // al, ax, eax
			rex(op);
			db(0xA8 | (op.isBit(8) ? 0 : 1));
		} else {
			opRext(op, 0, 0, T_OP_W1, 0xF6, false, immSize);
		}
		db(imm, immSize);
	}
	void imul(const Reg& reg, const Operand& op, int imm)
	{
		int s = inner::IsInDisp8(imm) ? 1 : 0;
		int immSize = s ? 1 : reg.isREG(16) ? 2 : 4;
		uint8_t code = uint8_t(0x69 | (s << 1));
		if (!opROO(Reg(), op, reg, T_APX|T_NF|T_ZU, code, immSize)) {
			opRO(reg, op, 0, code, reg.getKind() == op.getKind(), immSize);
		}
		db(imm, immSize);
	}
	void push(const Operand& op) { opPushPop(op, 0xFF, 6, 0x50); }
	void pop(const Operand& op) { opPushPop(op, 0x8F, 0, 0x58); }
	void push(const AddressFrame& af, uint32_t imm)
	{
		if (af.bit_ == 8) {
			db(0x6A); db(imm);
		} else if (af.bit_ == 16) {
			db(0x66); db(0x68); dw(imm);
		} else {
			db(0x68); dd(imm);
		}
	}
	/* use "push(word, 4)" if you want "push word 4" */
	void push(uint32_t imm)
	{
		if (inner::IsInDisp8(imm)) {
			push(byte, imm);
		} else {
			push(dword, imm);
		}
	}
	void mov(const Operand& op1, const Operand& op2)
	{
		const Reg *reg = 0;
		const Address *addr = 0;
		uint8_t code = 0;
		if (op1.isREG() && op1.getIdx() == 0 && op2.isMEM()) { // mov eax|ax|al, [disp]
			reg = &op1.getReg();
			addr= &op2.getAddress();
			code = 0xA0;
		} else
		if (op1.isMEM() && op2.isREG() && op2.getIdx() == 0) { // mov [disp], eax|ax|al
			reg = &op2.getReg();
			addr= &op1.getAddress();
			code = 0xA2;
		}
#ifdef XBYAK64
		if (addr && addr->is64bitDisp()) {
			if (code) {
				rex(*reg);
				db(op1.isREG(8) ? 0xA0 : op1.isREG() ? 0xA1 : op2.isREG(8) ? 0xA2 : 0xA3);
				if (addr->getLabel()) {
					putL_inner(*addr->getLabel(), inner::Labs, addr->getDisp(), 8);
				} else {
					db(addr->getDisp(), 8);
				}
			} else {
				XBYAK_THROW(ERR_BAD_COMBINATION)
			}
		} else
#else
		if (code && addr->isOnlyDisp()) {
			rex(*reg, *addr);
			db(code | (reg->isBit(8) ? 0 : 1));
			if (addr->getLabel()) {
				putL_inner(*addr->getLabel(), inner::Labs, addr->getDisp(), 4);
			} else {
				dd(static_cast<uint32_t>(addr->getDisp()));
			}
		} else
#endif
		{
			opRO_MR(op1, op2, 0x88);
		}
	}
	void mov(const Operand& op, uint64_t imm)
	{
		if (op.isREG()) {
			const int size = mov_imm(op.getReg(), imm);
			db(imm, size);
		} else if (op.isMEM()) {
			verifyMemHasSize(op);
			int immSize = op.getBit() / 8;
			if (immSize <= 4) {
				int64_t s = int64_t(imm) >> (immSize * 8);
				if (s != 0 && s != -1) XBYAK_THROW(ERR_IMM_IS_TOO_BIG)
			} else {
				if (!inner::IsInInt32(imm)) XBYAK_THROW(ERR_IMM_IS_TOO_BIG)
				immSize = 4;
			}
			opMR(op.getAddress(immSize), Reg(0, Operand::REG, op.getBit()), T_OP_W1, 0xC6);
			db(static_cast<uint32_t>(imm), immSize);
		} else {
			XBYAK_THROW(ERR_BAD_COMBINATION)
		}
	}

	// The template is used to avoid ambiguity when the 2nd argument is 0.
	// When the 2nd argument is 0 the call goes to
	// `void mov(const Operand& op, uint64_t imm)`.
	template <typename T1, typename T2>
	void mov(const T1&, const T2 *) { T1::unexpected; }
	void mov(const NativeReg& reg, const Label& label)
	{
		mov_imm(reg, dummyAddr);
		putL(label);
	}
	void xchg(const Operand& op1, const Operand& op2)
	{
		const Operand *p1 = &op1, *p2 = &op2;
		if (p1->isMEM() || (p2->isREG(16 | i32e) && p2->getIdx() == 0)) {
			p1 = &op2; p2 = &op1;
		}
		if (p1->isMEM()) XBYAK_THROW(ERR_BAD_COMBINATION)
		if (p2->isREG() && (p1->isREG(16 | i32e) && p1->getIdx() == 0)
#ifdef XBYAK64
			&& (p2->getIdx() != 0 || !p1->isREG(32))
#endif
		) {
			rex(*p2, *p1); db(0x90 | (p2->getIdx() & 7));
			return;
		}
		if (p1->isREG() && p2->isREG()) std::swap(p1, p2); // adapt to NASM 2.16.03 behavior to pass tests
		opRO(static_cast<const Reg&>(*p1), *p2, 0, 0x86 | (p1->isBit(8) ? 0 : 1), (p1->isREG() && (p1->getBit() == p2->getBit())));
	}

#ifndef XBYAK_DISABLE_SEGMENT
	void push(const Segment& seg)
	{
		switch (seg.getIdx()) {
		case Segment::es: db(0x06); break;
		case Segment::cs: db(0x0E); break;
		case Segment::ss: db(0x16); break;
		case Segment::ds: db(0x1E); break;
		case Segment::fs: db(0x0F); db(0xA0); break;
		case Segment::gs: db(0x0F); db(0xA8); break;
		default:
			assert(0);
		}
	}
	void pop(const Segment& seg)
	{
		switch (seg.getIdx()) {
		case Segment::es: db(0x07); break;
		case Segment::cs: XBYAK_THROW(ERR_BAD_COMBINATION)
		case Segment::ss: db(0x17); break;
		case Segment::ds: db(0x1F); break;
		case Segment::fs: db(0x0F); db(0xA1); break;
		case Segment::gs: db(0x0F); db(0xA9); break;
		default:
			assert(0);
		}
	}
	void putSeg(const Segment& seg)
	{
		switch (seg.getIdx()) {
		case Segment::es: db(0x26); break;
		case Segment::cs: db(0x2E); break;
		case Segment::ss: db(0x36); break;
		case Segment::ds: db(0x3E); break;
		case Segment::fs: db(0x64); break;
		case Segment::gs: db(0x65); break;
		default:
			assert(0);
		}
	}
	void mov(const Operand& op, const Segment& seg)
	{
		opRO(Reg8(seg.getIdx()), op, T_ALLOW_DIFF_SIZE | T_ALLOW_ABCDH, 0x8C, op.isREG(16|i32e));
	}
	void mov(const Segment& seg, const Operand& op)
	{
		opRO(Reg8(seg.getIdx()), op.isREG(16|i32e) ? static_cast<const Operand&>(op.getReg().cvt32()) : op, T_ALLOW_DIFF_SIZE | T_ALLOW_ABCDH, 0x8E, op.isREG(16|i32e));
	}
#endif

	enum { NONE = 256 };
	// constructor
	CodeGenerator(size_t maxSize = DEFAULT_MAX_CODE_SIZE, void *userPtr = 0, Allocator *allocator = 0)
		: CodeArray(maxSize, userPtr, allocator)
#ifndef XBYAK_USE_CONSTEXPR_REGISTERS
		#define XBYAK_INIT_REGISTER(Type, name, ...) , name(__VA_ARGS__)
		XBYAK_FOR_EACH_REGISTER(XBYAK_INIT_REGISTER)
		XBYAK_FOR_EACH_CONVENIENCE_ALL(XBYAK_INIT_REGISTER)
		#undef XBYAK_INIT_REGISTER
#endif
		, isDefaultJmpNEAR_(false)
	{
		setDefaultEncoding();
		setDefaultEncodingAVX10();
		labelMgr_.set(this);
	}
	void reset()
	{
		ClearError();
		resetSize();
		labelMgr_.reset();
		labelMgr_.set(this);
		if (isAllocType() && useProtect() && curMode_ == PROTECT_RE) setProtectModeRW();
	}
	bool hasUndefinedLabel() const { return labelMgr_.hasUndefSlabel() || labelMgr_.hasUndefClabel(); }
	/*
		MUST call ready() to complete generating code if you use AutoGrow mode.
		It is not necessary for the other mode if hasUndefinedLabel() is true.
	*/
	void ready(ProtectMode mode = PROTECT_RWE)
	{
		if (hasUndefinedLabel()) XBYAK_THROW(ERR_LABEL_IS_NOT_FOUND)
		if (isAutoGrow()) {
			calcJmpAddress();
			if (useProtect()) setProtectMode(mode);
		}
	}
	// set read/exec
	void readyRE() { return ready(PROTECT_RE); }
#ifdef XBYAK_TEST
	void dump(bool doClear = true)
	{
		CodeArray::dump();
		if (doClear) size_ = 0;
	}
#endif

#ifdef XBYAK_UNDEF_JNL
	#undef jnl
#endif

	// set default encoding of VNNI
	// EvexEncoding : AVX512_VNNI, VexEncoding : AVX-VNNI
	void setDefaultEncoding(PreferredEncoding enc = EvexEncoding)
	{
		if (enc != VexEncoding && enc != EvexEncoding) XBYAK_THROW(ERR_BAD_ENCODING_MODE)
		defaultEncoding_[0] = enc;
	}
	// default : PreferredEncoding : AVX-VNNI-INT8/AVX512-FP16
	void setDefaultEncodingAVX10(PreferredEncoding enc = PreAVX10v2Encoding)
	{
		if (enc != PreAVX10v2Encoding && enc != AVX10v2Encoding) XBYAK_THROW(ERR_BAD_ENCODING_MODE)
		defaultEncoding_[1] = enc;
	}

	void bswap(const Reg32e& r)
	{
		int idx = r.getIdx();
		uint8_t rex = (r.isREG(64) ? 8 : 0) | ((idx & 8) ? 1 : 0);
		if (idx >= 16) {
			db(0xD5); db((1<<7) | (idx & 16) | rex);
		} else {
			if (rex) db(0x40 | rex);
			db(0x0F);
		}
		db(0xC8 + (idx & 7));
	}
	void vmovd(const Operand& op1, const Operand& op2, PreferredEncoding enc = DefaultEncoding)
	{
		const uint64_t typeTbl[] = {
			T_EVEX|T_66|T_0F|T_W0|T_N4, T_EVEX|T_66|T_0F|T_W0|T_N4, // legacy, avx, avx512
			T_MUST_EVEX|T_66|T_0F|T_N4, T_MUST_EVEX|T_F3|T_0F|T_N4, // avx10.2
		};
		const int codeTbl[] = { 0x7E, 0x6E, 0xD6, 0x7E };
		opAVX10ZeroExt(op1, op2, typeTbl, codeTbl, enc, 32);
	}
	void vmovw(const Operand& op1, const Operand& op2, PreferredEncoding enc = DefaultEncoding)
	{
		const uint64_t typeTbl[] = {
			T_MUST_EVEX|T_66|T_MAP5|T_N2, T_MUST_EVEX|T_66|T_MAP5|T_N2, // avx512-fp16
			T_MUST_EVEX|T_F3|T_MAP5|T_N2, T_MUST_EVEX|T_F3|T_MAP5|T_N2, // avx10.2
		};
		const int codeTbl[] = { 0x7E, 0x6E, 0x7E, 0x6E };
		opAVX10ZeroExt(op1, op2, typeTbl, codeTbl, enc, 16|32|64);
	}
	/*
		useMultiByteNop
		= 0: use only single byte nop
		= 1: recommended multi-byte
		= 2: better for newer CPUs
	*/
	void nop(size_t size = 1, int useMultiByteNop = 2)
	{
		if (useMultiByteNop == 0) {
			for (size_t i = 0; i < size; i++) {
				db(0x90);
			}
			return;
		}
		/*
			Intel Architectures Software Developer's Manual Volume 2
			recommended multi-byte sequence of NOP instruction
			AMD and Intel seem to agree on the same sequences for up to 9 bytes:
			https://support.amd.com/TechDocs/55723_SOG_Fam_17h_Processors_3.00.pdf
			10~15 byte nop in Software Optimization Guide for the AMD Zen4 Microarchitecture No. 57647
		*/
		static const uint8_t nopTbl[][15] = {
			{0x90},
			{0x66, 0x90},
			{0x0F, 0x1F, 0x00},
			{0x0F, 0x1F, 0x40, 0x00},
			{0x0F, 0x1F, 0x44, 0x00, 0x00},
			{0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00},
			{0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00},
			{0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}, // 9
			{0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}, // 11
			{0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
		};
		const size_t n = useMultiByteNop == 2 ? sizeof(nopTbl) / sizeof(nopTbl[0]) : 9;
		while (size > 0) {
			size_t len = (std::min)(n, size);
			const uint8_t *seq = nopTbl[len - 1];
			db(seq, len);
			size -= len;
		}
	}
#ifndef XBYAK_DONT_READ_LIST
#include "xbyak_mnemonic.h"
	/*
		use single byte nop if useMultiByteNop = 0
	*/
	void align(size_t x = 16, int useMultiByteNop = 2)
	{
		if (x == 1) return;
		if (x < 1 || (x & (x - 1))) XBYAK_THROW(ERR_BAD_ALIGN)
		if (isAutoGrow() && inner::getPageSize() % x != 0) XBYAK_THROW(ERR_BAD_ALIGN)
		size_t remain = size_t(getCurr()) % x;
		if (remain) {
			nop(x - remain, useMultiByteNop);
		}
	}
#endif
};

template <>
inline void CodeGenerator::mov(const NativeReg& reg, const char *label) // can't use std::string
{
	assert(label);
	mov_imm(reg, dummyAddr);
	putL(label);
}

namespace util {
#define XBYAK_DEFINE_UTIL_REGISTER(Type, name, ...) static const XBYAK_CONSTEXPR Type name(__VA_ARGS__);
XBYAK_FOR_EACH_REGISTER(XBYAK_DEFINE_UTIL_REGISTER)
#undef XBYAK_DEFINE_UTIL_REGISTER
} // util

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif

} // end of namespace

#endif // XBYAK_XBYAK_H_
