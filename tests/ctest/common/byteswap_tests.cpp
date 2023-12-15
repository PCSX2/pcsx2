/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2023 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
