// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "pcsx2/GS/Renderers/Common/GSDevice.h"

#include <cstdint>
#include <gtest/gtest.h>

TEST(GSDevice, SortMultiStretchRectsSortsByTextureThenLinear)
{
	// The sort key only compares and batches source textures, it never dereferences them.
	GSTexture* const low_texture = reinterpret_cast<GSTexture*>(static_cast<uintptr_t>(0x1000));
	GSTexture* const high_texture = reinterpret_cast<GSTexture*>(static_cast<uintptr_t>(0x2000));
	const GSDevice::MultiStretchRect expected[] = {
		{GSVector4(), GSVector4(), low_texture, true, 0xf},
		{GSVector4(), GSVector4(), high_texture, false, 0xf},
	};
	GSDevice::MultiStretchRect rects[][2] = {
		{
			expected[0],
			expected[1],
		},
		{
			expected[1],
			expected[0],
		},
	};

	for (GSDevice::MultiStretchRect(&rect_pair)[2] : rects)
	{
		GSDevice::SortMultiStretchRects(rect_pair, std::size(rect_pair));

		EXPECT_EQ(rect_pair[0].src, expected[0].src);
		EXPECT_EQ(rect_pair[0].linear, expected[0].linear);
		EXPECT_EQ(rect_pair[1].src, expected[1].src);
		EXPECT_EQ(rect_pair[1].linear, expected[1].linear);
	}
}

TEST(GSDevice, SortMultiStretchRectsGroupsBatchableRectsTogether)
{
	// The sort key only compares and batches source textures, it never dereferences them.
	GSTexture* const low_texture = reinterpret_cast<GSTexture*>(static_cast<uintptr_t>(0x1000));
	GSTexture* const high_texture = reinterpret_cast<GSTexture*>(static_cast<uintptr_t>(0x2000));
	const GSDevice::MultiStretchRect a = {GSVector4(), GSVector4(), low_texture, true, 0xf};
	const GSDevice::MultiStretchRect b = {GSVector4(), GSVector4(), high_texture, false, 0xf};
	const GSDevice::MultiStretchRect expected[] = {
		a,
		a,
		a,
		a,
		b,
		b,
		b,
		b,
	};
	GSDevice::MultiStretchRect rects[] = {
		a,
		b,
		a,
		b,
		a,
		b,
		a,
		b,
	};

	GSDevice::SortMultiStretchRects(rects, std::size(rects));

	for (u32 i = 0; i < std::size(rects); i++)
	{
		EXPECT_EQ(rects[i].src, expected[i].src);
		EXPECT_EQ(rects[i].linear, expected[i].linear);
	}
}
