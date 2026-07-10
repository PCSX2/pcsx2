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

extern "C" {
// returns_twice is load-bearing: without it the optimizer may pop the
// caller's frame and tail-call the code reached after fastjmp_set returns
// (observed with clang LTO inlining execI into intStep), leaving the armed
// jmp_buf's saved SP pointing into a successor's live frame — fastjmp_jmp
// then resumes on a clobbered stack and the caller returns into garbage.
// The attribute (same contract as setjmp) pins the frame and disables
// tail-call/value-caching transforms across the call. MSVC has no
// equivalent, but also doesn't apply the offending transform to extern
// .asm functions.
#ifdef _MSC_VER
int fastjmp_set(fastjmp_buf* buf);
#else
__attribute__((returns_twice)) int fastjmp_set(fastjmp_buf* buf);
#endif
__noreturn void fastjmp_jmp(const fastjmp_buf* buf, int ret);
}
