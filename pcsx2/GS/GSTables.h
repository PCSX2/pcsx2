/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

/// Table for storing swizzling of blocks within a page
struct alignas(64) GSBlockSwizzleTable
{
	// Some swizzles are 4x8 and others are 8x4.  An 8x8 table can store either at the cost of 2x size
	u8 value[8][8];

	constexpr u8 lookup(int x, int y) const
	{
		return value[y & 7][x & 7];
	}
};

/// Adds sizes to GSBlockSwizzleTable for to feel better about not making mistakes
template <int Height, int Width>
struct GSSizedBlockSwizzleTable : public GSBlockSwizzleTable
{
};

/// Table for storing offsets of x = 0 pixels from the beginning of the page
/// Add values from a GSPixelRowOffsetTable to get the pixels for x != 0
template <int Height>
struct alignas(128) GSPixelColOffsetTable
{
	int value[Height] = {};

	int operator[](int y) const
	{
		return value[y % Height];
	}
};

/// Table for storing offsets of x != 0 pixels from the pixel at the same y where x = 0
/// Unlike ColOffsets, this table stretches to the maximum size of a texture so no masking is needed
struct alignas(128) GSPixelRowOffsetTable
{
	int value[4096] = {};

	int operator[](size_t x) const
	{
		ASSERT(x < 4096);
		return value[x];
	}
};

/// Adds size to GSPixelRowOffsetTable to feel better about not making mistakes
template <int PageWidth>
struct GSSizedPixelRowOffsetTable : public GSPixelRowOffsetTable
{
};

/// List of row offset tables
/// Some swizzlings (PSMT8 and PSMT4) have different row offsets depending on which column they're a part of
/// The ones that do use an a a b b b b a a pattern that repeats every 8 rows.
/// You can always look up the correct row in this list with y & 7, but if you use y & Mask where Mask is known at compile time, the compiler should be able to optimize better
template <int PageWidth, int Mask>
struct alignas(sizeof(void*) * 8) GSPixelRowOffsetTableList
{
	const GSPixelRowOffsetTable* rows[8];

	const GSPixelRowOffsetTable& operator[](int y) const
	{
		return *rows[y & Mask];
	}
};

/// Full pixel offset table
/// Template values are for objects constructing from one of these tables
template <int PageHeight, int PageWidth, int BlockHeight, int BlockWidth, int RowMask>
struct GSSwizzleTableList
{
	const GSSizedBlockSwizzleTable<BlockHeight, BlockWidth>& block;
	const GSPixelColOffsetTable<PageHeight>& col;
	const GSPixelRowOffsetTableList<PageWidth, RowMask>& row;
};

/// List of all tables for a given swizzle for easy setup
template <int PageHeight, int PageWidth, int BlockHeight, int BlockWidth, int RowMask>
constexpr GSSwizzleTableList<PageHeight, PageWidth, BlockHeight, BlockWidth, RowMask>
makeSwizzleTableList(
	const GSSizedBlockSwizzleTable<BlockHeight, BlockWidth>& block,
	const GSPixelColOffsetTable<PageHeight>& col,
	const GSPixelRowOffsetTableList<PageWidth, RowMask>& row)
{
	return {block, col, row};
}

extern const GSSizedBlockSwizzleTable<4, 8> blockTable32;
extern const GSSizedBlockSwizzleTable<4, 8> blockTable32Z;
extern const GSSizedBlockSwizzleTable<8, 4> blockTable16;
extern const GSSizedBlockSwizzleTable<8, 4> blockTable16S;
extern const GSSizedBlockSwizzleTable<8, 4> blockTable16Z;
extern const GSSizedBlockSwizzleTable<8, 4> blockTable16SZ;
extern const GSSizedBlockSwizzleTable<4, 8> blockTable8;
extern const GSSizedBlockSwizzleTable<8, 4> blockTable4;
extern const u8 columnTable32[8][8];
extern const u8 columnTable16[8][16];
extern const u8 columnTable8[16][16];
extern const u16 columnTable4[16][32];
extern const u8 clutTableT32I8[128];
extern const u8 clutTableT32I4[16];
extern const u8 clutTableT16I8[32];
extern const u8 clutTableT16I4[16];
extern const GSPixelColOffsetTable< 32> pixelColOffset32;
extern const GSPixelColOffsetTable< 32> pixelColOffset32Z;
extern const GSPixelColOffsetTable< 64> pixelColOffset16;
extern const GSPixelColOffsetTable< 64> pixelColOffset16S;
extern const GSPixelColOffsetTable< 64> pixelColOffset16Z;
extern const GSPixelColOffsetTable< 64> pixelColOffset16SZ;
extern const GSPixelColOffsetTable< 64> pixelColOffset8;
extern const GSPixelColOffsetTable<128> pixelColOffset4;

template <int PageWidth>
constexpr GSPixelRowOffsetTableList<PageWidth, 0> makeRowOffsetTableList(
	const GSSizedPixelRowOffsetTable<PageWidth>* a)
{
	return {{a, a, a, a, a, a, a, a}};
}

template <int PageWidth>
constexpr GSPixelRowOffsetTableList<PageWidth, 7> makeRowOffsetTableList(
	const GSSizedPixelRowOffsetTable<PageWidth>* a,
	const GSSizedPixelRowOffsetTable<PageWidth>* b)
{
	return {{a, a, b, b, b, b, a, a}};
}

/// Just here to force external linkage so we don't end up with multiple copies of pixelRowOffset*
struct GSTables
{
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset32;
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset32Z;
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset16;
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset16S;
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset16Z;
	static const GSSizedPixelRowOffsetTable< 64> _pixelRowOffset16SZ;
	static const GSSizedPixelRowOffsetTable<128> _pixelRowOffset8[2];
	static const GSSizedPixelRowOffsetTable<128> _pixelRowOffset4[2];

	static constexpr auto pixelRowOffset32   = makeRowOffsetTableList(&_pixelRowOffset32);
	static constexpr auto pixelRowOffset32Z  = makeRowOffsetTableList(&_pixelRowOffset32Z);
	static constexpr auto pixelRowOffset16   = makeRowOffsetTableList(&_pixelRowOffset16);
	static constexpr auto pixelRowOffset16S  = makeRowOffsetTableList(&_pixelRowOffset16S);
	static constexpr auto pixelRowOffset16Z  = makeRowOffsetTableList(&_pixelRowOffset16Z);
	static constexpr auto pixelRowOffset16SZ = makeRowOffsetTableList(&_pixelRowOffset16SZ);
	static constexpr auto pixelRowOffset8 = makeRowOffsetTableList(&_pixelRowOffset8[0], &_pixelRowOffset8[1]);
	static constexpr auto pixelRowOffset4 = makeRowOffsetTableList(&_pixelRowOffset4[0], &_pixelRowOffset4[1]);
};

constexpr auto swizzleTables32   = makeSwizzleTableList(blockTable32,   pixelColOffset32,   GSTables::pixelRowOffset32  );
constexpr auto swizzleTables32Z  = makeSwizzleTableList(blockTable32Z,  pixelColOffset32Z,  GSTables::pixelRowOffset32Z );
constexpr auto swizzleTables16   = makeSwizzleTableList(blockTable16,   pixelColOffset16,   GSTables::pixelRowOffset16  );
constexpr auto swizzleTables16Z  = makeSwizzleTableList(blockTable16Z,  pixelColOffset16Z,  GSTables::pixelRowOffset16Z );
constexpr auto swizzleTables16S  = makeSwizzleTableList(blockTable16S,  pixelColOffset16S,  GSTables::pixelRowOffset16S );
constexpr auto swizzleTables16SZ = makeSwizzleTableList(blockTable16SZ, pixelColOffset16SZ, GSTables::pixelRowOffset16SZ);
constexpr auto swizzleTables8    = makeSwizzleTableList(blockTable8,    pixelColOffset8,    GSTables::pixelRowOffset8   );
constexpr auto swizzleTables4    = makeSwizzleTableList(blockTable4,    pixelColOffset4,    GSTables::pixelRowOffset4   );
