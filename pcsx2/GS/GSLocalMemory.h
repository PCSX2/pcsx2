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

#include "GS.h"
#include "GSTables.h"
#include "GSVector.h"
#include "GSBlock.h"
#include "GSClut.h"

class GSOffset : public GSAlignedClass<32>
{
public:
	struct alignas(32) Block
	{
		short row[256]; // yn (n = 0 8 16 ...)
		short* col; // blockOffset*
	};

	struct alignas(32) Pixel
	{
		int row[4096]; // yn (n = 0 1 2 ...) NOTE: this wraps around above 2048, only transfers should address the upper half (dark cloud 2 inventing)
		int* col[8]; // rowOffset*
	};

	union { uint32 hash; struct { uint32 bp:14, bw:6, psm:6; }; };

	Block block;
	Pixel pixel;

	std::array<uint32*, 256> pages_as_bit; // texture page coverage based on the texture size. Lazy allocated

	GSOffset(uint32 bp, uint32 bw, uint32 psm);
	virtual ~GSOffset();

	enum { EOP = 0xffffffff };

	uint32* GetPages(const GSVector4i& rect, uint32* pages = NULL, GSVector4i* bbox = NULL);
	void* GetPagesAsBits(const GSVector4i& rect, void* pages);
	uint32* GetPagesAsBits(const GIFRegTEX0& TEX0);
};

struct GSPixelOffset
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn
	GSVector2i col[2048]; // f xn | z xn
	uint32 hash;
	uint32 fbp, zbp, fpsm, zpsm, bw;
};

struct GSPixelOffset4
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn (n = 0 1 2 ...)
	GSVector2i col[512]; // f xn | z xn (n = 0 4 8 ...)
	uint32 hash;
	uint32 fbp, zbp, fpsm, zpsm, bw;
};

class GSSwizzleInfo
{
	/// Table for storing swizzling of blocks within a page
	const GSBlockSwizzleTable* m_blockSwizzle;
	/// Table for storing swizzling of pixels within a page
	const uint32* m_pixelSwizzle;
	GSVector2i m_pageMask;  ///< Mask for getting the offset of a pixel that's within a page (may also be used as page dimensions - 1)
	GSVector2i m_blockMask; ///< Mask for getting the offset of a pixel that's within a block (may also be used as block dimensions - 1)
	uint8 m_pageShiftX;  ///< Amount to rshift x value by to get page offset
	uint8 m_pageShiftY;  ///< Amount to rshift y value by to get page offset
	uint8 m_blockShiftX; ///< Amount to rshift x value by to get offset in block
	uint8 m_blockShiftY; ///< Amount to rshift y value by to get offset in block
	static constexpr uint8 ilog2(uint32 i) { return i < 2 ? 0 : 1 + ilog2(i>>1); }
public:
	GSSwizzleInfo() = default;

	/// @param PageWidth Width of page in pixels
	/// @param PageHeight Height of page in pixels
	/// @param blockSize Size of block in pixels
	template <int PageWidth, int PageHeight>
	constexpr GSSwizzleInfo(GSVector2i blockSize, const GSBlockSwizzleTable* blockSwizzle, const uint32 (&pxSwizzle)[32][PageHeight][PageWidth])
		: m_blockSwizzle(blockSwizzle)
		, m_pixelSwizzle(&pxSwizzle[0][0][0])
		, m_pageMask{PageWidth - 1, PageHeight - 1}
		, m_blockMask{blockSize.x - 1, blockSize.y - 1}
		, m_pageShiftX(ilog2(PageWidth)), m_pageShiftY(ilog2(PageHeight))
		, m_blockShiftX(ilog2(blockSize.x)), m_blockShiftY(ilog2(blockSize.y))
	{
		static_assert(1 << ilog2(PageWidth) == PageWidth, "PageWidth must be a power of 2");
		static_assert(1 << ilog2(PageHeight) == PageHeight, "PageHeight must be a power of 2");
	}

	/// Get the block number of the given pixel
	uint32 bn(int x, int y, uint32 bp, uint32 bw) const
	{
		int yAmt = ((y >> (m_pageShiftY - 5)) & ~0x1f) * (bw >> (m_pageShiftX - 6));
		int xAmt = ((x >> (m_pageShiftX - 5)) & ~0x1f);
		return bp + yAmt + xAmt + m_blockSwizzle->lookup(x >> m_blockShiftX, y >> m_blockShiftY);
	}

	/// Get the address of the given pixel
	uint32 pa(int x, int y, uint32 bp, uint32 bw) const
	{
		int shift = m_pageShiftX + m_pageShiftY;
		uint32 page = ((bp >> 5) + (y >> m_pageShiftY) * (bw >> (m_pageShiftX - 6)) + (x >> m_pageShiftX)) % MAX_PAGES;
		// equivalent of pageOffset[bp & 0x1f][y & pageMaskY][x & pageMaskX]
		uint32 offsetIdx = ((bp & 0x1f) << shift) + ((y & m_pageMask.y) << m_pageShiftX) + (x & m_pageMask.x);
		uint32 word = (page << shift) + m_pixelSwizzle[offsetIdx];
		return word;
	}

	/// Loop over all pages in the given rect, calling `fn` on each
	/// Requires bp to be page-aligned (bp % 0x20 == 0)
	template <typename Fn>
	void loopAlignedPages(const GSVector4i& rect, uint32 bp, uint32 bw, Fn fn) const
	{
		int shiftX = m_pageShiftX;
		int shiftY = m_pageShiftY;
		int bwPages = bw >> (shiftX - 6);
		int pgXStart = (rect.x >> shiftX);
		int pgXEnd = (rect.z + m_pageMask.x) >> shiftX;
		// For non-last rows, pages > this will be covered by the next row
		int trimmedXEnd = pgXStart + std::min(pgXEnd - pgXStart, bwPages);
		int pgYStart = (rect.y >> shiftY);
		int pgYEnd = (rect.w + m_pageMask.y) >> shiftY;
		int bpPg = bp >> 5;

		auto call = [&](int xyOff)
		{
			fn((bpPg + xyOff) % MAX_PAGES);
		};

		int yOff;
		for (yOff = pgYStart * bwPages; yOff < (pgYEnd - 1) * bwPages; yOff += bwPages)
			for (int x = pgXStart; x < trimmedXEnd; x++)
				call(x + yOff);
		// Last row needs full x
		for (int x = pgXStart; x < pgXEnd; x++)
			call(x + yOff);
	}

private:
	struct alignas(16) TextureAligned
	{
		int ox1, oy1, ox2, oy2; ///< Block-aligned outer rect (smallest rectangle containing the original that is block-aligned)
		int ix1, iy1, ix2, iy2; ///< Page-aligned inner rect (largest rectangle inside original that is page-aligned)
	};

	/// Helper for loop functions
	TextureAligned _align(const GSVector4i& rect) const
	{
		GSVector4i outer = rect.ralign_presub<Align_Outside>(m_blockMask);
		GSVector4i inner = outer.ralign_presub<Align_Inside>(m_pageMask);
#if _M_SSE >= 0x501
		GSVector4i shift = GSVector4i(m_blockShiftX, m_blockShiftY).xyxy();
		outer = outer.srav32(shift);
		inner = inner.srav32(shift);
		return {
			outer.x,
			outer.y,
			outer.z,
			outer.w,
			inner.x,
			inner.y,
			inner.z,
			inner.w,
		};
#else
		GSVector4i outerX = outer.sra32(m_blockShiftX);
		GSVector4i outerY = outer.sra32(m_blockShiftY);
		GSVector4i innerX = inner.sra32(m_blockShiftX);
		GSVector4i innerY = inner.sra32(m_blockShiftY);
		return {
			outerX.x,
			outerY.y,
			outerX.z,
			outerY.w,
			innerX.x,
			innerY.y,
			innerX.z,
			innerY.w,
		};
#endif
	}
public:

	/// Loop over all the pages in the given rect, calling `fn` on each
	template <typename Fn>
	void loopPages(const GSVector4i& rect, uint32 bp, uint32 bw, Fn fn) const
	{
		const int blockOff = bp & 0x1f;
		const int invBlockOff = 32 - blockOff;
		const int bwPages = bw >> (m_pageShiftX - 6);
		const int bpPg = bp >> 5;
		if (!blockOff) // Aligned?
		{
			loopAlignedPages(rect, bp, bw, fn);
			return;
		}

		TextureAligned a = _align(rect);

		const int shiftX = m_pageShiftX - m_blockShiftX;
		const int shiftY = m_pageShiftY - m_blockShiftY;
		const int blkW = 1 << shiftX;
		const int blkH = 1 << shiftY;

		const int pgXStart = a.ox1 >> shiftX;
		const int pgXEnd = (a.ox2 >> shiftX) + 1;

		/// A texture page can span two GS pages if bp is not page-aligned
		/// This function checks if a rect in the texture touches the given page
		auto rectUsesPage = [&](int x1, int x2, int y1, int y2, bool lowPage) -> bool
		{
			for (int y = y1; y < y2; y++)
				for (int x = x1; x < x2; x++)
					if ((m_blockSwizzle->lookup(x, y) < invBlockOff) == lowPage)
						return true;
			return false;
		};

		/// Do the given coordinates stay within the boundaries of one page?
		auto staysWithinOnePage = [](int o1, int o2, int i1, int i2) -> bool
		{
			// Inner rect being inside out indicates staying within one page
			if (i2 < i1)
				return true;
			// If there's no inner rect, stays in one page if only one side of the page line is used
			if (i2 == i1)
				return o1 == i1 || o2 == i1;
			return false;
		};

		const bool onePageX = staysWithinOnePage(a.ox1, a.ox2, a.ix1, a.ix2);
		/// Adjusts start/end values for lines that don't touch their first/last page
		/// (e.g. if the texture only touches the bottom-left corner of its top-right page, depending on the bp, it may not have any pixels that actually use the last page in the row)
		auto adjustStartEnd = [&](int& start, int& end, int y1, int y2)
		{
			int startAdj1, startAdj2, endAdj1, endAdj2;
			if (onePageX)
			{
				startAdj1 = endAdj1 = a.ox1;
				startAdj2 = endAdj2 = a.ox2;
			}
			else
			{
				startAdj1 = (a.ox1 == a.ix1) ? 0    : a.ox1;
				startAdj2 = (a.ox1 == a.ix1) ? blkW : a.ix1;
				endAdj1   = (a.ox2 == a.ix2) ? 0    : a.ix2;
				endAdj2   = (a.ox2 == a.ix2) ? blkW : a.ox2;
			}

			if (!rectUsesPage(startAdj1, startAdj2, y1, y2, true))
				start++;
			if (!rectUsesPage(endAdj1, endAdj2, y1, y2, false))
				end--;
		};

		if (staysWithinOnePage(a.oy1, a.oy2, a.iy1, a.iy2))
		{
			adjustStartEnd(pgXStart, pgXEnd, a.oy1, a.oy2);
			for (int x = pgXStart; x <= pgXEnd; x++)
			{
				fn((bpPg + x) % MAX_PAGES);
			}
			return;
		}

		int midRowPgXStart = pgXStart, midRowPgXEnd = pgXEnd;
		adjustStartEnd(midRowPgXStart, midRowPgXEnd, 0, blkH);

		int lineBP = bpPg + (a.oy1 >> shiftY) * bwPages;
		int nextMin = 0;
		auto doLine = [&](int startOff, int endOff)
		{
			int start = std::max(nextMin, lineBP + startOff);
			int end = lineBP + endOff;
			nextMin = end + 1;
			lineBP += bwPages;
			for (int pos = start; pos <= end; pos++)
				fn(pos % MAX_PAGES);
		};
		if (a.oy1 != a.iy1)
		{
			int firstRowPgXStart = pgXStart, firstRowPgXEnd = pgXEnd;
			adjustStartEnd(firstRowPgXStart, firstRowPgXEnd, a.oy1, a.iy1);
			doLine(firstRowPgXStart, firstRowPgXEnd);
		}
		for (int y = a.iy1; y < a.iy2; y += blkH)
			doLine(midRowPgXStart, midRowPgXEnd);
		if (a.oy2 != a.iy2)
		{
			int lastRowPgXStart = pgXStart, lastRowPgXEnd = pgXEnd;
			adjustStartEnd(lastRowPgXStart, lastRowPgXEnd, a.iy2, a.oy2);
			doLine(lastRowPgXStart, lastRowPgXEnd);
		}
	}

	/// Loop over all the blocks in the given rect, calling `fn` on each
	template <typename Fn>
	void loopBlocks(const GSVector4i& rect, uint32 bp, uint32 bw, Fn fn) const
	{
		TextureAligned a = _align(rect);
		int bwPagesx32 = 32 * (bw >> (m_pageShiftX - 6));

		int shiftX = m_pageShiftX - m_blockShiftX;
		int shiftY = m_pageShiftY - m_blockShiftY;
		int blkW = 1 << shiftX;
		int blkH = 1 << shiftY;
		int ox1PageOff = (a.ox1 >> shiftX) * 32;

		auto partialPage = [&](uint32 bp, int x1, int x2, int y1, int y2)
		{
			for (int y = y1; y < y2; y++)
				for (int x = x1; x < x2; x++)
					fn((bp + m_blockSwizzle->lookup(x, y)) % MAX_BLOCKS);
		};
		auto fullPage = [&](uint32 bp)
		{
			for (int i = 0; i < 32; i++)
				fn((bp + i) % MAX_BLOCKS);
		};

		/// For use with dimensions big enough to touch/cross page boundaries
		auto lineHelper = [&](uint32 page, uint32 pageAdd, int add, int o1, int o2, int i1, int i2, auto partial, auto full)
		{
			if (o1 != i1)
			{
				partial(page, o1, i1);
				page += pageAdd;
			}
			for (int i = i1; i < i2; i += add)
			{
				full(page, i, i + add);
				page += pageAdd;
			}
			if (o2 != i2)
				partial(page, i2, o2);
		};

		/// [ox1, ox2) touches/crosses page boundaries, [y1, y2) is not the height of a full page
		auto partialLineLargeX = [&](uint32 bp, int y1, int y2)
		{
			auto partial = [&](uint32 page, int x1, int x2)
			{
				partialPage(page, x1, x2, y1, y2);
			};
			lineHelper(bp + ox1PageOff, 32, blkW, a.ox1, a.ox2, a.ix1, a.ix2, partial, partial);
		};
		/// [ox1, ox2) touches/crosses page boundaries, [y1, y2) is the height of a full page
		auto fullLineLargeX = [&](uint32 bp, int y1, int y2)
		{
			lineHelper(bp + ox1PageOff, 32, blkW, a.ox1, a.ox2, a.ix1, a.ix2,
				[&](uint32 page, int x1, int x2)
				{
					partialPage(page, x1, x2, y1, y2);
				},
				[&](uint32 page, int, int)
				{
					fullPage(page);
				});
		};
		/// [ox1, ox2) stayes within a page
		auto lineSmallX = [&](uint32 bp, int y1, int y2)
		{
			partialPage(bp + ox1PageOff, a.ox1, a.ox2, y1, y2);
		};

		/// [oy1, oy2) touches/crosses page boundaries
		auto mainLargeY = [&](auto partialLine, auto fullLine)
		{
			lineHelper(bp, bwPagesx32, blkH, a.oy1, a.oy2, a.iy1, a.iy2, partialLine, fullLine);
		};

		// Note: inner rectangle can end up inside-out (x2 < x1) when input rectangle was all within one page
		if (a.iy2 < a.iy1)
		{
			if (a.ix2 < a.ix1)
				lineSmallX(bp, a.oy1, a.oy2);
			else
				partialLineLargeX(bp, a.oy1, a.oy2);
		}
		else
		{
			if (a.ix2 < a.ix1)
				mainLargeY(lineSmallX, lineSmallX);
			else
				mainLargeY(partialLineLargeX, fullLineLargeX);
		}
	}
};

class GSLocalMemory : public GSAlignedClass<32>
{
public:
	typedef uint32 (*pixelAddress)(int x, int y, uint32 bp, uint32 bw);
	typedef void (GSLocalMemory::*writePixel)(int x, int y, uint32 c, uint32 bp, uint32 bw);
	typedef void (GSLocalMemory::*writeFrame)(int x, int y, uint32 c, uint32 bp, uint32 bw);
	typedef uint32 (GSLocalMemory::*readPixel)(int x, int y, uint32 bp, uint32 bw) const;
	typedef uint32 (GSLocalMemory::*readTexel)(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const;
	typedef void (GSLocalMemory::*writePixelAddr)(uint32 addr, uint32 c);
	typedef void (GSLocalMemory::*writeFrameAddr)(uint32 addr, uint32 c);
	typedef uint32 (GSLocalMemory::*readPixelAddr)(uint32 addr) const;
	typedef uint32 (GSLocalMemory::*readTexelAddr)(uint32 addr, const GIFRegTEXA& TEXA) const;
	typedef void (GSLocalMemory::*writeImage)(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	typedef void (GSLocalMemory::*readImage)(int& tx, int& ty, uint8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const;
	typedef void (GSLocalMemory::*readTexture)(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	typedef void (GSLocalMemory::*readTextureBlock)(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	struct alignas(128) psm_t
	{
		GSSwizzleInfo info;
		readPixel rp;
		readPixelAddr rpa;
		writePixel wp;
		writePixelAddr wpa;
		readTexel rt;
		readTexelAddr rta;
		writeFrameAddr wfa;
		writeImage wi;
		readImage ri;
		readTexture rtx, rtxP;
		readTextureBlock rtxb, rtxbP;
		uint16 bpp, trbpp, pal, fmt;
		GSVector2i bs, pgs;
		int* rowOffset[8];
		short* blockOffset;
		uint8 msk, depth;
	};

	static psm_t m_psm[64];

	static const int m_vmsize = 1024 * 1024 * 4;

	uint8* m_vm8;
	uint16* m_vm16;
	uint32* m_vm32;

	GSClut m_clut;

protected:
	bool m_use_fifo_alloc;

	static uint32 pageOffset32[32][32][64];
	static uint32 pageOffset32Z[32][32][64];
	static uint32 pageOffset16[32][64][64];
	static uint32 pageOffset16S[32][64][64];
	static uint32 pageOffset16Z[32][64][64];
	static uint32 pageOffset16SZ[32][64][64];
	static uint32 pageOffset8[32][64][128];
	static uint32 pageOffset4[32][128][128];

	static int rowOffset32[4096];
	static int rowOffset32Z[4096];
	static int rowOffset16[4096];
	static int rowOffset16S[4096];
	static int rowOffset16Z[4096];
	static int rowOffset16SZ[4096];
	static int rowOffset8[2][4096];
	static int rowOffset4[2][4096];

	static short blockOffset32[256];
	static short blockOffset32Z[256];
	static short blockOffset16[256];
	static short blockOffset16S[256];
	static short blockOffset16Z[256];
	static short blockOffset16SZ[256];
	static short blockOffset8[256];
	static short blockOffset4[256];

public:
	static constexpr GSSwizzleInfo swizzle32{{8, 8}, &blockTable32, pageOffset32};
	static constexpr GSSwizzleInfo swizzle32Z{{8, 8}, &blockTable32Z, pageOffset32Z};
	static constexpr GSSwizzleInfo swizzle16{{16, 8}, &blockTable16, pageOffset16};
	static constexpr GSSwizzleInfo swizzle16S{{16, 8}, &blockTable16S, pageOffset16S};
	static constexpr GSSwizzleInfo swizzle16Z{{16, 8}, &blockTable16Z, pageOffset16Z};
	static constexpr GSSwizzleInfo swizzle16SZ{{16, 8}, &blockTable16SZ, pageOffset16SZ};
	static constexpr GSSwizzleInfo swizzle8{{16, 16}, &blockTable8, pageOffset8};
	static constexpr GSSwizzleInfo swizzle4{{32, 16}, &blockTable4, pageOffset4};

protected:
	__forceinline static uint32 Expand24To32(uint32 c, const GIFRegTEXA& TEXA)
	{
		return (((!TEXA.AEM | (c & 0xffffff)) ? TEXA.TA0 : 0) << 24) | (c & 0xffffff);
	}

	__forceinline static uint32 Expand16To32(uint16 c, const GIFRegTEXA& TEXA)
	{
		return (((c & 0x8000) ? TEXA.TA1 : (!TEXA.AEM | c) ? TEXA.TA0 : 0) << 24)
			| ((c & 0x7c00) << 9)
			| ((c & 0x03e0) << 6)
			| ((c & 0x001f) << 3);
	}

	// TODO

	friend class GSClut;

	//

	std::unordered_map<uint32, GSOffset*> m_omap;
	std::unordered_map<uint32, GSPixelOffset*> m_pomap;
	std::unordered_map<uint32, GSPixelOffset4*> m_po4map;
	std::unordered_map<uint64, std::vector<GSVector2i>*> m_p2tmap;

public:
	GSLocalMemory();
	virtual ~GSLocalMemory();

	GSOffset* GetOffset(uint32 bp, uint32 bw, uint32 psm);
	GSPixelOffset* GetPixelOffset(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	GSPixelOffset4* GetPixelOffset4(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	std::vector<GSVector2i>* GetPage2TileMap(const GIFRegTEX0& TEX0);

	// address

	static uint32 BlockNumber32(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle32.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber16(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber16S(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16S.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber8(int x, int y, uint32 bp, uint32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle8.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber4(int x, int y, uint32 bp, uint32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle4.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber32Z(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle32Z.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber16Z(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16Z.bn(x, y, bp, bw);
	}

	static uint32 BlockNumber16SZ(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16SZ.bn(x, y, bp, bw);
	}

	uint8* BlockPtr(uint32 bp) const
	{
		return &m_vm8[(bp % MAX_BLOCKS) << 8];
	}

	uint8* BlockPtr32(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber32(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr16(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber16(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr16S(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber16S(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr8(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber8(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr4(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber4(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr32Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber32Z(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr16Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber16Z(x, y, bp, bw) << 8];
	}

	uint8* BlockPtr16SZ(int x, int y, uint32 bp, uint32 bw) const
	{
		return &m_vm8[BlockNumber16SZ(x, y, bp, bw) << 8];
	}

	static uint32 PixelAddressOrg32(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber32(x, y, bp, bw) << 6) + columnTable32[y & 7][x & 7];
	}

	static uint32 PixelAddressOrg16(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber16(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static uint32 PixelAddressOrg16S(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber16S(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static uint32 PixelAddressOrg8(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber8(x, y, bp, bw) << 8) + columnTable8[y & 15][x & 15];
	}

	static uint32 PixelAddressOrg4(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber4(x, y, bp, bw) << 9) + columnTable4[y & 15][x & 31];
	}

	static uint32 PixelAddressOrg32Z(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber32Z(x, y, bp, bw) << 6) + columnTable32[y & 7][x & 7];
	}

	static uint32 PixelAddressOrg16Z(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber16Z(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static uint32 PixelAddressOrg16SZ(int x, int y, uint32 bp, uint32 bw)
	{
		return (BlockNumber16SZ(x, y, bp, bw) << 7) + columnTable16[y & 7][x & 15];
	}

	static __forceinline uint32 PixelAddress32(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle32.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress16(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress16S(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16S.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress8(int x, int y, uint32 bp, uint32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle8.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress4(int x, int y, uint32 bp, uint32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle4.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress32Z(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle32Z.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress16Z(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16Z.pa(x, y, bp, bw);
	}

	static __forceinline uint32 PixelAddress16SZ(int x, int y, uint32 bp, uint32 bw)
	{
		return swizzle16SZ.pa(x, y, bp, bw);
	}

	// pixel R/W

	__forceinline uint32 ReadPixel32(uint32 addr) const
	{
		return m_vm32[addr];
	}

	__forceinline uint32 ReadPixel24(uint32 addr) const
	{
		return m_vm32[addr] & 0x00ffffff;
	}

	__forceinline uint32 ReadPixel16(uint32 addr) const
	{
		return (uint32)m_vm16[addr];
	}

	__forceinline uint32 ReadPixel8(uint32 addr) const
	{
		return (uint32)m_vm8[addr];
	}

	__forceinline uint32 ReadPixel4(uint32 addr) const
	{
		return (m_vm8[addr >> 1] >> ((addr & 1) << 2)) & 0x0f;
	}

	__forceinline uint32 ReadPixel8H(uint32 addr) const
	{
		return m_vm32[addr] >> 24;
	}

	__forceinline uint32 ReadPixel4HL(uint32 addr) const
	{
		return (m_vm32[addr] >> 24) & 0x0f;
	}

	__forceinline uint32 ReadPixel4HH(uint32 addr) const
	{
		return (m_vm32[addr] >> 28) & 0x0f;
	}

	__forceinline uint32 ReadFrame24(uint32 addr) const
	{
		return 0x80000000 | (m_vm32[addr] & 0xffffff);
	}

	__forceinline uint32 ReadFrame16(uint32 addr) const
	{
		uint32 c = (uint32)m_vm16[addr];

		return ((c & 0x8000) << 16) | ((c & 0x7c00) << 9) | ((c & 0x03e0) << 6) | ((c & 0x001f) << 3);
	}

	__forceinline uint32 ReadPixel32(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel32(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel24(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel24(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel16(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel16(PixelAddress16(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel16S(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel16(PixelAddress16S(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel8(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel8(PixelAddress8(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel4(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel4(PixelAddress4(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel8H(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel8H(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel4HL(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel4HL(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel4HH(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel4HH(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel32Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel32(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel24Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel24(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel16Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel16(PixelAddress16Z(x, y, bp, bw));
	}

	__forceinline uint32 ReadPixel16SZ(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadPixel16(PixelAddress16SZ(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame24(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame24(PixelAddress32(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame16(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame16(PixelAddress16(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame16S(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame16(PixelAddress16S(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame24Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame24(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame16Z(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame16(PixelAddress16Z(x, y, bp, bw));
	}

	__forceinline uint32 ReadFrame16SZ(int x, int y, uint32 bp, uint32 bw) const
	{
		return ReadFrame16(PixelAddress16SZ(x, y, bp, bw));
	}

	__forceinline void WritePixel32(uint32 addr, uint32 c)
	{
		m_vm32[addr] = c;
	}

	__forceinline void WritePixel24(uint32 addr, uint32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0xff000000) | (c & 0x00ffffff);
	}

	__forceinline void WritePixel16(uint32 addr, uint32 c)
	{
		m_vm16[addr] = (uint16)c;
	}

	__forceinline void WritePixel8(uint32 addr, uint32 c)
	{
		m_vm8[addr] = (uint8)c;
	}

	__forceinline void WritePixel4(uint32 addr, uint32 c)
	{
		int shift = (addr & 1) << 2;
		addr >>= 1;

		m_vm8[addr] = (uint8)((m_vm8[addr] & (0xf0 >> shift)) | ((c & 0x0f) << shift));
	}

	__forceinline void WritePixel8H(uint32 addr, uint32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0x00ffffff) | (c << 24);
	}

	__forceinline void WritePixel4HL(uint32 addr, uint32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0xf0ffffff) | ((c & 0x0f) << 24);
	}

	__forceinline void WritePixel4HH(uint32 addr, uint32 c)
	{
		m_vm32[addr] = (m_vm32[addr] & 0x0fffffff) | ((c & 0x0f) << 28);
	}

	__forceinline void WriteFrame16(uint32 addr, uint32 c)
	{
		uint32 rb = c & 0x00f800f8;
		uint32 ga = c & 0x8000f800;

		WritePixel16(addr, (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
	}

	__forceinline void WritePixel32(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel32(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel24(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel24(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel16(PixelAddress16(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16S(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel16(PixelAddress16S(x, y, bp, bw), c);
	}

	__forceinline void WritePixel8(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel8(PixelAddress8(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel4(PixelAddress4(x, y, bp, bw), c);
	}

	__forceinline void WritePixel8H(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel8H(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4HL(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel4HL(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4HH(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel4HH(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel32Z(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel32(PixelAddress32Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel24Z(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel24(PixelAddress32Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16Z(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel16(PixelAddress16Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16SZ(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WritePixel16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WriteFrame16(PixelAddress16(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16S(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WriteFrame16(PixelAddress16S(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16Z(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WriteFrame16(PixelAddress16Z(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16SZ(int x, int y, uint32 c, uint32 bp, uint32 bw)
	{
		WriteFrame16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	__forceinline void WritePixel32(uint8* RESTRICT src, uint32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(uint32);

		for (int y = r.top; y < r.bottom; y++, src += pitch)
		{
			uint32* RESTRICT s = (uint32*)src;
			uint32* RESTRICT d = &m_vm32[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for (int x = r.left; x < r.right; x++)
			{
				d[col[x]] = s[x];
			}
		}
	}

	__forceinline void WritePixel24(uint8* RESTRICT src, uint32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(uint32);

		for (int y = r.top; y < r.bottom; y++, src += pitch)
		{
			uint32* RESTRICT s = (uint32*)src;
			uint32* RESTRICT d = &m_vm32[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for (int x = r.left; x < r.right; x++)
			{
				d[col[x]] = (d[col[x]] & 0xff000000) | (s[x] & 0x00ffffff);
			}
		}
	}

	__forceinline void WritePixel16(uint8* RESTRICT src, uint32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(uint16);

		for (int y = r.top; y < r.bottom; y++, src += pitch)
		{
			uint16* RESTRICT s = (uint16*)src;
			uint16* RESTRICT d = &m_vm16[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for (int x = r.left; x < r.right; x++)
			{
				d[col[x]] = s[x];
			}
		}
	}

	__forceinline void WriteFrame16(uint8* RESTRICT src, uint32 pitch, GSOffset* off, const GSVector4i& r)
	{
		src -= r.left * sizeof(uint32);

		for (int y = r.top; y < r.bottom; y++, src += pitch)
		{
			uint32* RESTRICT s = (uint32*)src;
			uint16* RESTRICT d = &m_vm16[off->pixel.row[y]];
			int* RESTRICT col = off->pixel.col[0];

			for (int x = r.left; x < r.right; x++)
			{
				uint32 rb = s[x] & 0x00f800f8;
				uint32 ga = s[x] & 0x8000f800;

				d[col[x]] = (uint16)((ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
			}
		}
	}

	__forceinline uint32 ReadTexel32(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_vm32[addr];
	}

	__forceinline uint32 ReadTexel24(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand24To32(m_vm32[addr], TEXA);
	}

	__forceinline uint32 ReadTexel16(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand16To32(m_vm16[addr], TEXA);
	}

	__forceinline uint32 ReadTexel8(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8(addr)];
	}

	__forceinline uint32 ReadTexel4(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4(addr)];
	}

	__forceinline uint32 ReadTexel8H(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8H(addr)];
	}

	__forceinline uint32 ReadTexel4HL(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HL(addr)];
	}

	__forceinline uint32 ReadTexel4HH(uint32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HH(addr)];
	}

	__forceinline uint32 ReadTexel32(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel24(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel16(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel16S(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16S(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel8(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8(PixelAddress8(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel4(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4(PixelAddress4(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel8H(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8H(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel4HL(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HL(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel4HH(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HH(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel32Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel24Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel16Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline uint32 ReadTexel16SZ(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16SZ(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	//

	template <int psm, int bsx, int bsy, int alignment>
	void WriteImageColumn(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int alignment>
	void WriteImageBlock(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy>
	void WriteImageLeftRight(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int trbpp>
	void WriteImageTopBottom(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int trbpp>
	void WriteImage(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	void WriteImage24(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage8H(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage4HL(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage4HH(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImage24Z(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	void WriteImageX(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	// TODO: ReadImage32/24/...

	void ReadImageX(int& tx, int& ty, uint8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const;

	// * => 32

	void ReadTexture32(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTextureGPU24(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture24(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture16(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8H(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HL(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HH(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTexture(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock32(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock24(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock16(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8H(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HL(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HH(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	// pal ? 8 : 32

	void ReadTexture8P(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4P(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8HP(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HLP(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HHP(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock8P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8HP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HLP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HHP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	//

	template <typename T>
	void ReadTexture(const GSOffset* RESTRICT off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	//

	void SaveBMP(const std::string& fn, uint32 bp, uint32 bw, uint32 psm, int w, int h);
};
