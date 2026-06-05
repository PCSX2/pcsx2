// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "Pcsx2Defs.h"
#include <cstdint>
#include <cstddef>

struct fastjmp_buf
{
#if defined(_WIN32)
	static constexpr std::size_t BUF_SIZE = 240;
#elif defined(ARCH_ARM64)
	static constexpr std::size_t BUF_SIZE = 168;
#else
	static constexpr std::size_t BUF_SIZE = 64;
#endif

	alignas(16) std::uint8_t buf[BUF_SIZE];
};

// fastjmp_set can "return twice" (once normally with 0, and again via fastjmp_jmp with the
// passed value), exactly like setjmp. It MUST be marked returns_twice so the compiler does
// not optimize the calling function assuming the code after the call runs only once — most
// importantly, so it does not tail-call-optimize a subsequent call (which would deallocate
// the caller's frame that fastjmp_set captured the SP of, leaving fastjmp_jmp to restore a
// frame whose saved registers have since been clobbered).
#if defined(__GNUC__) || defined(__clang__)
#define FASTJMP_RETURNS_TWICE __attribute__((returns_twice))
#else
#define FASTJMP_RETURNS_TWICE
#endif

extern "C" {
FASTJMP_RETURNS_TWICE int fastjmp_set(fastjmp_buf* buf);
__noreturn void fastjmp_jmp(const fastjmp_buf* buf, int ret);
}
