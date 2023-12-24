// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Pcsx2Defs.h"
#include "common/ByteSwap.h"
#include <gtest/gtest.h>

TEST(ByteSwap, ByteSwap)
{
	ASSERT_EQ(ByteSwap(static_cast<u16>(0xabcd)), 0xcdabu);
	ASSERT_EQ(ByteSwap(static_cast<u32>(0xabcdef01)), 0x01efcdabu);
	ASSERT_EQ(ByteSwap(static_cast<u64>(0xabcdef0123456789ULL)), 0x8967452301efcdabu);
	ASSERT_EQ(ByteSwap(static_cast<s32>(0x80123456)), 0x56341280);
}
