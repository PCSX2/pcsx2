// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Defs.h"
#include <cstdint>
#include <cstddef>

struct fastjmp_buf
{
#if defined(_WIN32)
	static constexpr std::size_t BUF_SIZE = 240;
#elif defined(_M_ARM64)
	static constexpr std::size_t BUF_SIZE = 168;
#else
	static constexpr std::size_t BUF_SIZE = 64;
#endif

	alignas(16) std::uint8_t buf[BUF_SIZE];
};

extern "C" {
int fastjmp_set(fastjmp_buf* buf);
__noreturn void fastjmp_jmp(const fastjmp_buf* buf, int ret);
}
