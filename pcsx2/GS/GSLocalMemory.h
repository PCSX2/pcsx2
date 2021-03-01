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

class GSSwizzleInfo;

class GSOffset
{
	/// Table for storing swizzling of blocks within a page
	const GSBlockSwizzleTable* m_blockSwizzle;
	/// Table for storing swizzling of pixels within a page (size: uint32[PageHeight][PageWidth])
	const uint32* m_pixelSwizzle;
	GSVector2i m_pageMask;  ///< Mask for getting the offset of a pixel that's within a page (may also be used as page dimensions - 1)
	GSVector2i m_blockMask; ///< Mask for getting the offset of a pixel that's within a block (may also be used as block dimensions - 1)
	uint8 m_pageShiftX;  ///< Amount to rshift x value by to get page x offset
	uint8 m_pageShiftY;  ///< Amount to rshift y value by to get page y offset
	uint8 m_blockShiftX; ///< Amount to rshift x value by to get block x offset
	uint8 m_blockShiftY; ///< Amount to rshift y value by to get block y offset
	int m_bp;   ///< Offset's base pointer (same measurement as GS)
	int m_bwPg; ///< Offset's buffer width in pages (not equal to bw in GS for 8 and 4-bit textures)
	int m_psm;  ///< Offset's pixel storage mode (just for storage, not used by any of the GSOffset algorithms)
public:
	GSOffset() = default;
	constexpr GSOffset(const GSSwizzleInfo& swz, uint32 bp, uint32 bw, uint32 psm);
	/// Help the optimizer by using this method instead of GSLocalMemory::GetOffset when the PSM is known
	constexpr static GSOffset fromKnownPSM(uint32 bp, uint32 bw, GS_PSM psm);

	uint32 bp()  const { return m_bp; }
	uint32 bw()  const { return m_bwPg << (m_pageShiftX - 6); }
	uint32 psm() const { return m_psm; }
	int blockShiftX() const { return m_blockShiftX; }
	int blockShiftY() const { return m_blockShiftY; }

	/// Helper class for efficiently getting the numbers of multiple blocks in a scanning pattern (increment x then y)
	class BNHelper
	{
		const GSBlockSwizzleTable* m_blockSwizzle; ///< Block swizzle table from GSOffset
		int m_baseBP;    ///< bp for start of current row (to return to the origin x when advancing y)
		int m_bp;        ///< bp for current position
		int m_baseBlkX;  ///< x of origin in blocks (to return to the origin x when advancing y)
		int m_blkX;      ///< x of current position in blocks
		int m_blkY;      ///< y of current position in blocks
		int m_pageMaskX; ///< mask for x value of block coordinate to get position within page (to detect page crossing)
		int m_pageMaskY; ///< mask for y value of block coordinate to get position within page (to detect page crossing)
		int m_addY;      ///< Amount to add to bp to advance one page in y direction
	public:
		BNHelper(const GSOffset& off, int x, int y)
		{
			m_blockSwizzle = off.m_blockSwizzle;
			int yAmt = ((y >> (off.m_pageShiftY - 5)) & ~0x1f) * off.m_bwPg;
			int xAmt = ((x >> (off.m_pageShiftX - 5)) & ~0x1f);
			m_baseBP = m_bp = off.m_bp + yAmt + xAmt;
			m_baseBlkX = m_blkX = x >> off.m_blockShiftX;
			m_blkY = y >> off.m_blockShiftY;
			m_pageMaskX = (1 << (off.m_pageShiftX - off.m_blockShiftX)) - 1;
			m_pageMaskY = (1 << (off.m_pageShiftY - off.m_blockShiftY)) - 1;
			m_addY = 32 * off.m_bwPg;
		}

		/// Get the current x position as an offset in blocks
		int blkX() const { return m_blkX; }
		/// Get the current y position as an offset in blocks
		int blkY() const { return m_blkY; }

		/// Advance one block in the x direction
		void nextBlockX()
		{
			m_blkX++;
			if (!(m_blkX & m_pageMaskX))
				m_bp += 32;
		}

		/// Advance one block in the y direction and reset x to the origin
		void nextBlockY()
		{
			m_blkY++;
			if (!(m_blkY & m_pageMaskY))
				m_baseBP += m_addY;

			m_blkX = m_baseBlkX;
			m_bp = m_baseBP;
		}

		/// Get the current block number without wrapping at MAX_BLOCKS
		uint32 valueNoWrap() const
		{
			return m_bp + m_blockSwizzle->lookup(m_blkX, m_blkY);
		}

		/// Get the current block number
		uint32 value() const
		{
			return valueNoWrap() % MAX_BLOCKS;
		}
	};

	/// Get the block number of the given pixel
	uint32 bn(int x, int y) const
	{
		return BNHelper(*this, x, y).value();
	}

	/// Get a helper class for efficiently calculating multiple block numbers
	BNHelper bnMulti(int x, int y) const
	{
		return BNHelper(*this, x, y);
	}

	static bool isAligned(const GSVector4i& r, const GSVector2i& mask)
	{
		return r.width() > mask.x && r.height() > mask.y && !(r.left & mask.x) && !(r.top & mask.y) && !(r.right & mask.x) && !(r.bottom & mask.y);
	}

	bool isBlockAligned(const GSVector4i& r) const { return isAligned(r, m_blockMask); }
	bool isPageAligned(const GSVector4i& r) const { return isAligned(r, m_pageMask); }

	/// Loop over all the blocks in the given rect, calling `fn` on each
	template <typename Fn>
	void loopBlocks(const GSVector4i& rect, Fn&& fn) const
	{
		BNHelper bn = bnMulti(rect.left, rect.top);
		int right = (rect.right + m_blockMask.x) >> m_blockShiftX;
		int bottom = (rect.bottom + m_blockMask.y) >> m_blockShiftY;

		for (; bn.blkY() < bottom; bn.nextBlockY())
			for (; bn.blkX() < right; bn.nextBlockX())
				fn(bn.value());
	}

	/// Helper class for efficiently getting the addresses of multiple pixels in a line (along the x axis)
	class PAHelper
	{
		/// Pixel swizzle array offset to the beginning of the current line
		const uint32* m_pixelSwizzle;
		int m_pageMaskX;  ///< Mask for getting offset within a page
		int m_pageBase;   ///< Page number for origin x
		int m_x;          ///< Current x position
		int m_pageShiftX; ///< Amount to rshift x value to get page offset
		int m_shift;      ///< Amount to lshift page number to get element offset for the start of that page
	public:
		PAHelper() = default;
		PAHelper(const GSOffset& off, int x, int y)
		{
			m_pixelSwizzle = off.m_pixelSwizzle + ((y & off.m_pageMask.y) << off.m_pageShiftX);
			m_pageBase = (off.m_bp >> 5) + (y >> off.m_pageShiftY) * off.m_bwPg;
			m_pageMaskX = off.m_pageMask.x;
			m_x = x;
			m_pageShiftX = off.m_pageShiftX;
			m_shift = off.m_pageShiftX + off.m_pageShiftY;
		}

		/// Get current x value
		int x() const { return m_x; }
		/// Increment x value
		void incX() { m_x++; }
		/// Decrement x value
		void decX() { m_x--; }
		/// Get current pixel address
		uint32 value() const
		{
			int page = (m_pageBase + (m_x >> m_pageShiftX)) % MAX_PAGES;
			return (page << m_shift) + m_pixelSwizzle[m_x & m_pageMaskX];
		}
	};

	/// Get the address of the given pixel
	uint32 pa(int x, int y) const
	{
		return PAHelper(*this, x, y).value();
	}

	/// Get a helper class for efficiently calculating multiple pixel addresses in a line (along the x axis)
	PAHelper paMulti(int x, int y) const
	{
		return PAHelper(*this, x, y);
	}

	/// Loop over the pixels in the given rectangle
	/// Fn should be void(*)(VM*, Src*)
	template <typename VM, typename Src, typename Fn>
	void loopPixels(const GSVector4i& r, VM* RESTRICT vm, Src* RESTRICT px, int pitch, Fn&& fn) const
	{
		px -= r.left;

		for (int y = r.top; y < r.bottom; y++, px = reinterpret_cast<Src*>(reinterpret_cast<uint8*>(px) + pitch))
		{
			PAHelper pa = paMulti(r.left, y);
			while (pa.x() < r.right)
			{
				fn(vm + pa.value(), px + pa.x());
				pa.incX();
			}
		}
	}

	/// Helper class for looping over the pages in a rect
	/// Create with GSOffset::pageLooperForRect
	class PageLooper
	{
		int firstRowPgXStart, firstRowPgXEnd; ///< Offset of start/end pages of the first line from x=0 page (only line for textures that don't cross page boundaries)
		int   midRowPgXStart,   midRowPgXEnd; ///< Offset of start/end pages of inner lines (which always are always the height of the full page) from y=0 page
		int  lastRowPgXStart,  lastRowPgXEnd; ///< Offset of start/end pages of the last line from x=0 page
		int bp;   ///< Page offset of y=top x=0
		int yInc; ///< Amount to add to bp when increasing y by one page
		int yCnt; ///< Number of pages the rect covers in the y direction

		friend class GSOffset;

	public:
		/// Loop over pages, fn can return `false` to break the loop
		/// Fn: bool(*)(uint32)
		template <typename Fn>
		void loopPagesWithBreak(Fn&& fn) const
		{
			int lineBP = bp;
			int nextMin = 0;

			int startOff = firstRowPgXStart;
			int endOff   = firstRowPgXEnd;
			int yCnt = this->yCnt;
			for (int y = 0; y < yCnt; y++)
			{
				int start = std::max(nextMin, lineBP + startOff);
				int end = lineBP + endOff;
				nextMin = end;
				lineBP += yInc;
				for (int pos = start; pos < end; pos++)
					if (!fn(pos % MAX_PAGES))
						return;

				if (y < yCnt - 1)
				{
					startOff = midRowPgXStart;
					endOff   = midRowPgXEnd;
				}
				else
				{
					startOff = lastRowPgXStart;
					endOff   = lastRowPgXEnd;
				}
			}
		}

		/// Loop over pages, calling `fn` on each one with no option to break
		/// Fn: void(*)(uint32)
		template <typename Fn>
		void loopPages(Fn&& fn) const
		{
			loopPagesWithBreak([fn = std::forward<Fn>(fn)](uint32 page) { fn(page); return true; });
		}
	};

	/// Get an object for looping over the pages in the given rect
	PageLooper pageLooperForRect(const GSVector4i& rect) const;

	/// Loop over all the pages in the given rect, calling `fn` on each
	template <typename Fn>
	void loopPages(const GSVector4i& rect, Fn&& fn) const
	{
		pageLooperForRect(rect).loopPages(std::forward<Fn>(fn));
	}
};

class GSSwizzleInfo
{
	friend class GSOffset;
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
		return GSOffset(*this, bp, bw, 0).bn(x, y);
	}

	/// Get the address of the given pixel
	uint32 pa(int x, int y, uint32 bp, uint32 bw) const
	{
		return GSOffset(*this, bp, bw, 0).pa(x, y);
	}

	/// Loop over all the pages in the given rect, calling `fn` on each
	template <typename Fn>
	void loopPages(const GSVector4i& rect, uint32 bp, uint32 bw, Fn&& fn) const
	{
		GSOffset(*this, bp, bw, 0).loopPages(rect, std::forward<Fn>(fn));
	}

	/// Loop over all the blocks in the given rect, calling `fn` on each
	template <typename Fn>
	void loopBlocks(const GSVector4i& rect, uint32 bp, uint32 bw, Fn&& fn) const
	{
		GSOffset(*this, bp, bw, 0).loopBlocks(rect, std::forward<Fn>(fn));
	}
};

constexpr inline GSOffset::GSOffset(const GSSwizzleInfo& swz, uint32 bp, uint32 bw, uint32 psm)
	: m_blockSwizzle(swz.m_blockSwizzle)
	, m_pixelSwizzle(swz.m_pixelSwizzle + ((bp & 0x1f) << (swz.m_pageShiftX + swz.m_pageShiftY)))
	, m_pageMask(swz.m_pageMask), m_blockMask(swz.m_blockMask)
	, m_pageShiftX(swz.m_pageShiftX), m_pageShiftY(swz.m_pageShiftY)
	, m_blockShiftX(swz.m_blockShiftX), m_blockShiftY(swz.m_blockShiftY)
	, m_bp(bp)
	, m_bwPg(bw >> (m_pageShiftX - 6))
	, m_psm(psm)
{
}

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
	typedef void (GSLocalMemory::*readTexture)(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
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

	std::unordered_map<uint32, GSPixelOffset*> m_pomap;
	std::unordered_map<uint32, GSPixelOffset4*> m_po4map;
	std::unordered_map<uint64, std::vector<GSVector2i>*> m_p2tmap;

public:
	GSLocalMemory();
	virtual ~GSLocalMemory();

	GSOffset GetOffset(uint32 bp, uint32 bw, uint32 psm);
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

	void WritePixel32(uint8* RESTRICT src, uint32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, m_vm32, (uint32*)src, pitch, [&](uint32* dst, uint32* src) { *dst = *src; });
	}

	void WritePixel24(uint8* RESTRICT src, uint32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, m_vm32, (uint32*)src, pitch,
		[&](uint32* dst, uint32* src)
		{
			*dst = (*dst & 0xff000000) | (*src & 0x00ffffff);
		});
	}

	void WritePixel16(uint8* RESTRICT src, uint32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, m_vm16, (uint16*)src, pitch, [&](uint16* dst, uint16* src) { *dst = *src; });
	}

	void WriteFrame16(uint8* RESTRICT src, uint32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, m_vm16, (uint32*)src, pitch,
		[&](uint16* dst, uint32* src)
		{
			uint32 rb = *src & 0x00f800f8;
			uint32 ga = *src & 0x8000f800;

			*dst = (uint16)((ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
		});
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

	void ReadTexture32(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTextureGPU24(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture24(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture16(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8H(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HL(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HH(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTexture(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock32(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock24(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock16(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8H(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HL(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HH(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	// pal ? 8 : 32

	void ReadTexture8P(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4P(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture8HP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HLP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	void ReadTexture4HHP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	void ReadTextureBlock8P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock8HP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HLP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;
	void ReadTextureBlock4HHP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const;

	//

	template <typename T>
	void ReadTexture(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	//

	void SaveBMP(const std::string& fn, uint32 bp, uint32 bw, uint32 psm, int w, int h);
};

constexpr inline GSOffset GSOffset::fromKnownPSM(uint32 bp, uint32 bw, GS_PSM psm)
{
	switch (psm)
	{
		case PSM_PSMCT32:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSM_PSMCT24:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSM_PSMCT16:  return GSOffset(GSLocalMemory::swizzle16,   bp, bw, psm);
		case PSM_PSMCT16S: return GSOffset(GSLocalMemory::swizzle16S,  bp, bw, psm);
		case PSM_PSGPU24:  return GSOffset(GSLocalMemory::swizzle16,   bp, bw, psm);
		case PSM_PSMT8:    return GSOffset(GSLocalMemory::swizzle8,    bp, bw, psm);
		case PSM_PSMT4:    return GSOffset(GSLocalMemory::swizzle4,    bp, bw, psm);
		case PSM_PSMT8H:   return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSM_PSMT4HL:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSM_PSMT4HH:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSM_PSMZ32:   return GSOffset(GSLocalMemory::swizzle32Z,  bp, bw, psm);
		case PSM_PSMZ24:   return GSOffset(GSLocalMemory::swizzle32Z,  bp, bw, psm);
		case PSM_PSMZ16:   return GSOffset(GSLocalMemory::swizzle16Z,  bp, bw, psm);
		case PSM_PSMZ16S:  return GSOffset(GSLocalMemory::swizzle16SZ, bp, bw, psm);
	}
	return GSOffset(GSLocalMemory::swizzle32, bp, bw, psm);
}
