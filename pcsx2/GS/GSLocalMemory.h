// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GSTables.h"
#include "GSVector.h"
#include "GSClut.h"
#include "MultiISA.h"

#include "common/Assertions.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct GSPixelOffset
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn
	GSVector2i col[2048]; // f xn | z xn
	u32 hash;
	u32 fbp, zbp, fpsm, zpsm, bw;
};

struct GSPixelOffset4
{
	// 16 bit offsets (m_vm16[...])

	GSVector2i row[2048]; // f yn | z yn (n = 0 1 2 ...)
	GSVector2i col[512]; // f xn | z xn (n = 0 4 8 ...)
	u32 hash;
	u32 fbp, zbp, fpsm, zpsm, bw;
};

class GSOffset;

class GSSwizzleInfo
{
	friend class GSOffset;
	/// Table for storing swizzling of blocks within a page
	const GSBlockSwizzleTable* m_blockSwizzle;
	/// Table for storing swizzling of pixels within a page in the y dimension
	const int* m_pixelSwizzleCol;
	/// Array of tables for storing swizzling of pixels in the x dimension
	const GSPixelRowOffsetTable* const* m_pixelSwizzleRow;
	GSVector2i m_pageMask;  ///< Mask for getting the offset of a pixel that's within a page (may also be used as page dimensions - 1)
	GSVector2i m_blockMask; ///< Mask for getting the offset of a pixel that's within a block (may also be used as block dimensions - 1)
	int m_pixelRowMask;     ///< Mask for getting the offset in m_pixelSwizzleRow for a given y value
	u8 m_pageShiftX;  ///< Amount to rshift x value by to get page offset
	u8 m_pageShiftY;  ///< Amount to rshift y value by to get page offset
	u8 m_blockShiftX; ///< Amount to rshift x value by to get offset in block
	u8 m_blockShiftY; ///< Amount to rshift y value by to get offset in block
	u32 m_blockAddressXor; ///< Value to xor with the final block address (used for Z swizzles)
	u32 m_pixelAddressXor; ///< Value to xor with the final pixel address
	static constexpr u8 ilog2(u32 i) { return i < 2 ? 0 : 1 + ilog2(i >> 1); }

public:
	GSSwizzleInfo() = default;

	/// @param blockSize Size of block in pixels
	template <int PageWidth, int PageHeight, int BlocksWide, int BlocksHigh, int PixelRowMask>
	constexpr GSSwizzleInfo(GSSwizzleTableList<PageHeight, PageWidth, BlocksHigh, BlocksWide, PixelRowMask> list, u32 blockXor)
		: m_blockSwizzle(&list.block)
		, m_pixelSwizzleCol(list.col.value)
		, m_pixelSwizzleRow(list.row.rows)
		, m_pageMask{PageWidth - 1, PageHeight - 1}
		, m_blockMask{(PageWidth / BlocksWide) - 1, (PageHeight / BlocksHigh) - 1}
		, m_pixelRowMask(PixelRowMask)
		, m_pageShiftX(ilog2(PageWidth)), m_pageShiftY(ilog2(PageHeight))
		, m_blockShiftX(ilog2(PageWidth / BlocksWide)), m_blockShiftY(ilog2(PageHeight / BlocksHigh))
		, m_blockAddressXor(blockXor)
		, m_pixelAddressXor(blockXor << (m_blockShiftX + m_blockShiftY))
	{
		static_assert(1 << ilog2(PageWidth) == PageWidth, "PageWidth must be a power of 2");
		static_assert(1 << ilog2(PageHeight) == PageHeight, "PageHeight must be a power of 2");
	}

	/// Returns the amount to shift to convert a width to pages.
	u8 pageShiftX() const { return m_pageShiftX; }
	u8 pageShiftY() const { return m_pageShiftY; }

	/// Get the block number of the given pixel
	u32 bn(int x, int y, u32 bp, u32 bw) const;

	/// Get the address of the given pixel
	u32 pa(int x, int y, u32 bp, u32 bw) const;
};

class GSOffset : GSSwizzleInfo
{
	int m_bp;   ///< Offset's base pointer (same measurement as GS)
	int m_bwPg; ///< Offset's buffer width in pages (not equal to bw in GS for 8 and 4-bit textures)
	int m_psm;  ///< Offset's pixel storage mode (just for storage, not used by any of the GSOffset algorithms)
public:
	GSOffset() = default;
	constexpr GSOffset(const GSSwizzleInfo& swz, u32 bp, u32 bw, u32 psm)
		: GSSwizzleInfo(swz)
		, m_bp(bp)
		, m_bwPg(bw >> (m_pageShiftX - 6))
		, m_psm(psm)
	{
	}
	/// Help the optimizer by using this method instead of GSLocalMemory::GetOffset when the PSM is known
	constexpr static GSOffset fromKnownPSM(u32 bp, u32 bw, GS_PSM psm);

	u32 bp()  const { return m_bp; }
	u32 bw()  const { return m_bwPg << (m_pageShiftX - 6); }
	u32 psm() const { return m_psm; }
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
		u32 m_xor;       ///< XOR mask for final address
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
			m_xor = off.m_blockAddressXor;
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
		u32 valueNoWrap() const
		{
			return (m_bp + m_blockSwizzle->lookup(m_blkX, m_blkY)) ^ m_xor;
		}

		/// Get the current block number
		u32 value() const
		{
			return valueNoWrap() % MAX_BLOCKS;
		}
	};

	/// Get the block number of the given pixel
	u32 bn(int x, int y) const
	{
		return BNHelper(*this, x, y).value();
	}

	/// Get the block number of the given pixel, without wrapping to MAX_BLOCKS
	u32 bnNoWrap(int x, int y) const
	{
		return BNHelper(*this, x, y).valueNoWrap();
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

	/// Calculate the pixel address at the given y position with x of 0 minus the final xor
	int pixelAddressZeroXRaw(int y) const
	{
		int base = m_bp << (m_pageShiftX + m_pageShiftY - 5);   // Offset from base pointer
		base += ((y & ~m_pageMask.y) * m_bwPg) << m_pageShiftX; // Offset from pages in y direction
		// TODO: Old GSOffset masked here but is that useful?  Probably should mask at end or not at all...
		base &= (MAX_PAGES << (m_pageShiftX + m_pageShiftY)) - 1; // Mask
		base += m_pixelSwizzleCol[y & m_pageMask.y]; // Add offset from y within page
		return base;
	}

	/// Helper class for efficiently getting the addresses of multiple pixels in a line (along the x axis)
	class PAHelper
	{
		/// Pixel swizzle array
		const int* m_pixelSwizzleRow;
		int m_base;
		u32 m_xor;

	public:
		PAHelper() = default;
		PAHelper(const GSOffset& off, int x, int y)
		{
			m_pixelSwizzleRow = off.m_pixelSwizzleRow[y & off.m_pixelRowMask]->value + x;
			m_base = off.pixelAddressZeroXRaw(y);
			m_xor = off.m_pixelAddressXor;
		}

		/// Get pixel reference for the given x offset from the one used to create the PAHelper
		u32 value(int x) const
		{
			return (m_base + m_pixelSwizzleRow[x]) ^ m_xor;
		}
	};

	/// Get the address of the given pixel
	u32 pa(int x, int y) const
	{
		return PAHelper(*this, 0, y).value(x);
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

		for (int y = r.top; y < r.bottom; y++, px = reinterpret_cast<Src*>(reinterpret_cast<u8*>(px) + pitch))
		{
			PAHelper pa = paMulti(0, y);
			for (int x = r.left; x < r.right; x++)
			{
				fn(&vm[pa.value(x)], px + x);
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
		bool slowPath; ///< True if the texture is big enough to wrap around GS memory and overlap itself

		friend class GSOffset;

	public:
		/// Loop over pages, fn can return `false` to break the loop
		/// Fn: bool(*)(u32)
		template <typename Fn>
		void loopPagesWithBreak(Fn&& fn) const
		{
			int lineBP = bp;
			int startOff = firstRowPgXStart;
			int endOff   = firstRowPgXEnd;
			int yCnt = this->yCnt;

			if (slowPath) [[unlikely]]
			{
				u32 touched[MAX_PAGES / 32] = {};
				for (int y = 0; y < yCnt; y++)
				{
					u32 start = lineBP + startOff;
					u32 end   = lineBP + endOff;
					lineBP += yInc;
					for (u32 pos = start; pos < end; pos++)
					{
						u32 page = pos % MAX_PAGES;
						u32 idx = page / 32;
						u32 mask = 1 << (page % 32);
						if (touched[idx] & mask)
							continue;
						if (!fn(page))
							return;
						touched[idx] |= mask;
					}

					if (y < yCnt - 2)
					{
						// Next iteration is not last (y + 1 < yCnt - 1).
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
			else
			{
				u32 nextMin = 0;

				for (int y = 0; y < yCnt; y++)
				{
					u32 start = std::max<u32>(nextMin, lineBP + startOff);
					u32 end   = lineBP + endOff;
					nextMin = end;
					lineBP += yInc;
					for (u32 pos = start; pos < end; pos++)
						if (!fn(pos % MAX_PAGES))
							return;

					if (y < yCnt - 2)
					{
						// Next iteration is not last (y + 1 < yCnt - 1).
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
		}

		/// Loop over pages, calling `fn` on each one with no option to break
		/// Fn: void(*)(u32)
		template <typename Fn>
		void loopPages(Fn&& fn) const
		{
			loopPagesWithBreak([fn = std::forward<Fn>(fn)](u32 page) { fn(page); return true; });
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

	/// Use compile-time dimensions from `swz` as a performance optimization
	/// Also asserts if your assumption was wrong
	constexpr GSOffset assertSizesMatch(const GSSwizzleInfo& swz) const
	{
		GSOffset o = *this;
#define MATCH(x) pxAssert(o.x == swz.x); o.x = swz.x;
		MATCH(m_pageMask)
		MATCH(m_blockMask)
		MATCH(m_pixelRowMask)
		MATCH(m_pageShiftX)
		MATCH(m_pageShiftY)
		MATCH(m_blockShiftX)
		MATCH(m_blockShiftY)
#undef MATCH
		return o;
	}
};

inline u32 GSSwizzleInfo::bn(int x, int y, u32 bp, u32 bw) const
{
	return GSOffset(*this, bp, bw, 0).bn(x, y);
}

inline u32 GSSwizzleInfo::pa(int x, int y, u32 bp, u32 bw) const
{
	return GSOffset(*this, bp, bw, 0).pa(x, y);
}

class GSLocalMemory;
MULTI_ISA_DEF(class GSLocalMemoryFunctions;)
MULTI_ISA_DEF(void GSLocalMemoryPopulateFunctions(GSLocalMemory& mem);)

class GSLocalMemory final : public GSAlignedClass<32>
{
	MULTI_ISA_FRIEND(GSLocalMemoryFunctions)

public:
	typedef u32 (*pixelAddress)(int x, int y, u32 bp, u32 bw);
	typedef void (GSLocalMemory::*writePixel)(int x, int y, u32 c, u32 bp, u32 bw);
	typedef void (GSLocalMemory::*writeFrame)(int x, int y, u32 c, u32 bp, u32 bw);
	typedef u32 (GSLocalMemory::*readPixel)(int x, int y, u32 bp, u32 bw) const;
	typedef u32 (GSLocalMemory::*readTexel)(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const;
	typedef void (GSLocalMemory::*writePixelAddr)(u32 addr, u32 c);
	typedef void (GSLocalMemory::*writeFrameAddr)(u32 addr, u32 c);
	typedef u32(GSLocalMemory::*PixelAddr)(int x, int y, u32 bp, u32 bw) const;
	typedef u32 (GSLocalMemory::*readPixelAddr)(u32 addr) const;
	typedef u32 (GSLocalMemory::*readTexelAddr)(u32 addr, const GIFRegTEXA& TEXA) const;
	typedef void (*writeImage)(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	typedef void (*readImage)(const GSLocalMemory& mem, int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	typedef void (*readTexture)(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	typedef void (*readTextureBlock)(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	enum PSM_FMT
	{
		PSM_FMT_32,
		PSM_FMT_24,
		PSM_FMT_16
	};

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
		u16 bpp, trbpp, pal, fmt;
		GSVector2i cs, bs, pgs;
		u8 msk, depth;
		u32 fmsk;
	};

	static psm_t m_psm[64];
	static readImage m_readImageX;

	static constexpr int m_vmsize = 1024 * 1024 * 4;

	u8* m_vm8;

	GSClut m_clut;

public:
	static constexpr GSSwizzleInfo swizzle32   {swizzleTables32,  0x00};
	static constexpr GSSwizzleInfo swizzle32Z  {swizzleTables32,  0x18};
	static constexpr GSSwizzleInfo swizzle16   {swizzleTables16,  0x00};
	static constexpr GSSwizzleInfo swizzle16S  {swizzleTables16S, 0x00};
	static constexpr GSSwizzleInfo swizzle16Z  {swizzleTables16,  0x18};
	static constexpr GSSwizzleInfo swizzle16SZ {swizzleTables16S, 0x18};
	static constexpr GSSwizzleInfo swizzle8    {swizzleTables8,   0x00};
	static constexpr GSSwizzleInfo swizzle4    {swizzleTables4,   0x00};

protected:
	__forceinline static u32 Expand24To32(u32 c, const GIFRegTEXA& TEXA)
	{
		return (((!TEXA.AEM | (c & 0xffffff)) ? TEXA.TA0 : 0) << 24) | (c & 0xffffff);
	}

	__forceinline static u32 Expand16To32(u16 c, const GIFRegTEXA& TEXA)
	{
		return (((c & 0x8000) ? TEXA.TA1 : (!TEXA.AEM | c) ? TEXA.TA0 : 0) << 24)
			| ((c & 0x7c00) << 9)
			| ((c & 0x03e0) << 6)
			| ((c & 0x001f) << 3);
	}

	// TODO

	friend class GSClut;

	//

	std::unordered_map<u32, GSPixelOffset*> m_pomap;
	std::unordered_map<u32, GSPixelOffset4*> m_po4map;
	std::unordered_map<u64, std::vector<GSVector2i>*> m_p2tmap;

public:
	GSLocalMemory();
	~GSLocalMemory();

	__forceinline u8* vm8() const { return m_vm8; }
	__forceinline u16* vm16() const { return reinterpret_cast<u16*>(m_vm8); }
	__forceinline u32* vm32() const { return reinterpret_cast<u32*>(m_vm8); }

	GSOffset GetOffset(u32 bp, u32 bw, u32 psm) const
	{
		return GSOffset(m_psm[psm].info, bp, bw, psm);
	}
	GSPixelOffset* GetPixelOffset(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	GSPixelOffset4* GetPixelOffset4(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF);
	std::vector<GSVector2i>* GetPage2TileMap(const GIFRegTEX0& TEX0);
	static bool HasOverlap(u32 src_bp, u32 src_bw, u32 src_psm, GSVector4i src_rect, u32 dst_bp, u32 dst_bw, u32 dst_psm, GSVector4i dst_rect);
	static bool IsPageAligned(u32 psm, const GSVector4i& rc);
	static u32 GetStartBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect);
	static u32 GetEndBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect);
	static u32 GetUnwrappedEndBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect);
	static GSVector4i GetRectForPageOffset(u32 base_bp, u32 offset_bp, u32 bw, u32 psm);

	// address

	static u32 BlockNumber32(int x, int y, u32 bp, u32 bw)
	{
		return swizzle32.bn(x, y, bp, bw);
	}

	static u32 BlockNumber16(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16.bn(x, y, bp, bw);
	}

	static u32 BlockNumber16S(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16S.bn(x, y, bp, bw);
	}

	static u32 BlockNumber8(int x, int y, u32 bp, u32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle8.bn(x, y, bp, bw);
	}

	static u32 BlockNumber4(int x, int y, u32 bp, u32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle4.bn(x, y, bp, bw);
	}

	static u32 BlockNumber32Z(int x, int y, u32 bp, u32 bw)
	{
		return swizzle32Z.bn(x, y, bp, bw);
	}

	static u32 BlockNumber16Z(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16Z.bn(x, y, bp, bw);
	}

	static u32 BlockNumber16SZ(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16SZ.bn(x, y, bp, bw);
	}

	u8* BlockPtr(u32 bp) const
	{
		return &m_vm8[(bp % MAX_BLOCKS) << 8];
	}

	u8* BlockPtr32(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber32(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16S(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16S(x, y, bp, bw) << 8];
	}

	u8* BlockPtr8(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber8(x, y, bp, bw) << 8];
	}

	u8* BlockPtr4(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber4(x, y, bp, bw) << 8];
	}

	u8* BlockPtr32Z(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber32Z(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16Z(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16Z(x, y, bp, bw) << 8];
	}

	u8* BlockPtr16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return &m_vm8[BlockNumber16SZ(x, y, bp, bw) << 8];
	}

	static __forceinline u32 PixelAddress32(int x, int y, u32 bp, u32 bw)
	{
		return swizzle32.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress16(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress16S(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16S.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress8(int x, int y, u32 bp, u32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle8.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress4(int x, int y, u32 bp, u32 bw)
	{
		// ASSERT((bw & 1) == 0); // allowed for mipmap levels

		return swizzle4.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress32Z(int x, int y, u32 bp, u32 bw)
	{
		return swizzle32Z.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress16Z(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16Z.pa(x, y, bp, bw);
	}

	static __forceinline u32 PixelAddress16SZ(int x, int y, u32 bp, u32 bw)
	{
		return swizzle16SZ.pa(x, y, bp, bw);
	}

	// pixel R/W

	__forceinline u32 ReadPixel32(u32 addr) const
	{
		return vm32()[addr];
	}

	__forceinline u32 ReadPixel24(u32 addr) const
	{
		return vm32()[addr] & 0x00ffffff;
	}

	__forceinline u32 ReadPixel16(u32 addr) const
	{
		return (u32)vm16()[addr];
	}

	__forceinline u32 ReadPixel8(u32 addr) const
	{
		return (u32)m_vm8[addr];
	}

	__forceinline u32 ReadPixel4(u32 addr) const
	{
		return (m_vm8[addr >> 1] >> ((addr & 1) << 2)) & 0x0f;
	}

	__forceinline u32 ReadPixel8H(u32 addr) const
	{
		return vm32()[addr] >> 24;
	}

	__forceinline u32 ReadPixel4HL(u32 addr) const
	{
		return (vm32()[addr] >> 24) & 0x0f;
	}

	__forceinline u32 ReadPixel4HH(u32 addr) const
	{
		return (vm32()[addr] >> 28) & 0x0f;
	}

	__forceinline u32 ReadFrame24(u32 addr) const
	{
		return 0x80000000 | (vm32()[addr] & 0xffffff);
	}

	__forceinline u32 ReadFrame16(u32 addr) const
	{
		u32 c = (u32)vm16()[addr];

		return ((c & 0x8000) << 16) | ((c & 0x7c00) << 9) | ((c & 0x03e0) << 6) | ((c & 0x001f) << 3);
	}

	__forceinline u32 ReadPixel32(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel32(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel24(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel24(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel16(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel16S(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16S(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel8(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel8(PixelAddress8(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel4(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4(PixelAddress4(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel8H(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel8H(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel4HL(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4HL(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel4HH(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel4HH(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel32Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel32(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel24Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel24(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel16Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16Z(x, y, bp, bw));
	}

	__forceinline u32 ReadPixel16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return ReadPixel16(PixelAddress16SZ(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame24(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame24(PixelAddress32(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame16(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame16S(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16S(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame24Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame24(PixelAddress32Z(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame16Z(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16Z(x, y, bp, bw));
	}

	__forceinline u32 ReadFrame16SZ(int x, int y, u32 bp, u32 bw) const
	{
		return ReadFrame16(PixelAddress16SZ(x, y, bp, bw));
	}

	__forceinline void WritePixel32(u32 addr, u32 c)
	{
		vm32()[addr] = c;
	}

	__forceinline static void WritePixel24(u32* addr, u32 c)
	{
		*addr = (*addr & 0xff000000) | (c & 0x00ffffff);
	}

	__forceinline void WritePixel24(u32 addr, u32 c)
	{
		WritePixel24(vm32() + addr, c);
	}

	__forceinline void WritePixel16(u32 addr, u32 c)
	{
		vm16()[addr] = (u16)c;
	}

	__forceinline void WritePixel8(u32 addr, u32 c)
	{
		m_vm8[addr] = (u8)c;
	}

	__forceinline void WritePixel4(u32 addr, u32 c)
	{
		int shift = (addr & 1) << 2;
		addr >>= 1;

		m_vm8[addr] = (u8)((m_vm8[addr] & (0xf0 >> shift)) | ((c & 0x0f) << shift));
	}

	__forceinline static void WritePixel8H(u32* addr, u32 c)
	{
		*addr = (*addr & 0x00ffffff) | (c << 24);
	}

	__forceinline void WritePixel8H(u32 addr, u32 c)
	{
		WritePixel8H(vm32() + addr, c);
	}

	__forceinline static void WritePixel4HL(u32* addr, u32 c)
	{
		*addr = (*addr & 0xf0ffffff) | ((c & 0x0f) << 24);
	}

	__forceinline void WritePixel4HL(u32 addr, u32 c)
	{
		WritePixel4HL(vm32() + addr, c);
	}

	__forceinline static void WritePixel4HH(u32* addr, u32 c)
	{
		*addr = (*addr & 0x0fffffff) | ((c & 0x0f) << 28);
	}

	__forceinline void WritePixel4HH(u32 addr, u32 c)
	{
		WritePixel4HH(vm32() + addr, c);
	}

	__forceinline void WriteFrame16(u32 addr, u32 c)
	{
		u32 rb = c & 0x00f800f8;
		u32 ga = c & 0x8000f800;

		WritePixel16(addr, (ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
	}

	__forceinline void WritePixel32(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel32(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel24(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel24(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16S(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16S(x, y, bp, bw), c);
	}

	__forceinline void WritePixel8(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel8(PixelAddress8(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4(PixelAddress4(x, y, bp, bw), c);
	}

	__forceinline void WritePixel8H(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel8H(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4HL(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4HL(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel4HH(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel4HH(PixelAddress32(x, y, bp, bw), c);
	}

	__forceinline void WritePixel32Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel32(PixelAddress32Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel24Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel24(PixelAddress32Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16Z(x, y, bp, bw), c);
	}

	__forceinline void WritePixel16SZ(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WritePixel16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16S(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16S(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16Z(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16Z(x, y, bp, bw), c);
	}

	__forceinline void WriteFrame16SZ(int x, int y, u32 c, u32 bp, u32 bw)
	{
		WriteFrame16(PixelAddress16SZ(x, y, bp, bw), c);
	}

	void WritePixel32(u8* RESTRICT src, u32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, vm32(), (u32*)src, pitch, [&](u32* dst, u32* src) { *dst = *src; });
	}

	void WritePixel32(u8* RESTRICT src, u32 pitch, const GSOffset& off, const GSVector4i& r, u32 write_mask)
	{
		off.loopPixels(r, vm32(), (u32*)src, pitch, [&](u32* dst, u32* src) { *dst = (*dst & ~write_mask) | (*src & write_mask); });
	}

	void WritePixel24(u8* RESTRICT src, u32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, vm32(), (u32*)src, pitch,
			[&](u32* dst, u32* src)
		{
			*dst = (*dst & 0xff000000) | (*src & 0x00ffffff);
		});
	}

	void WritePixel16(u8* RESTRICT src, u32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, vm16(), (u16*)src, pitch, [&](u16* dst, u16* src) { *dst = *src; });
	}

	void WriteFrame16(u8* RESTRICT src, u32 pitch, const GSOffset& off, const GSVector4i& r)
	{
		off.loopPixels(r, vm16(), (u32*)src, pitch,
		[&](u16* dst, u32* src)
		{
			u32 rb = *src & 0x00f800f8;
			u32 ga = *src & 0x8000f800;

			*dst = (u16)((ga >> 16) | (rb >> 9) | (ga >> 6) | (rb >> 3));
		});
	}

	__forceinline u32 ReadTexel32(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return vm32()[addr];
	}

	__forceinline u32 ReadTexel24(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand24To32(vm32()[addr], TEXA);
	}

	__forceinline u32 ReadTexel16(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return Expand16To32(vm16()[addr], TEXA);
	}

	__forceinline u32 ReadTexel8(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8(addr)];
	}

	__forceinline u32 ReadTexel4(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4(addr)];
	}

	__forceinline u32 ReadTexel8H(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel8H(addr)];
	}

	__forceinline u32 ReadTexel4HL(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HL(addr)];
	}

	__forceinline u32 ReadTexel4HH(u32 addr, const GIFRegTEXA& TEXA) const
	{
		return m_clut[ReadPixel4HH(addr)];
	}

	__forceinline u32 ReadTexel32(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel24(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel16(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel16S(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16S(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel8(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8(PixelAddress8(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel4(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4(PixelAddress4(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel8H(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel8H(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel4HL(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HL(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel4HH(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel4HH(PixelAddress32(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel32Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel32(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel24Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel24(PixelAddress32Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel16Z(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16Z(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline u32 ReadTexel16SZ(int x, int y, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA) const
	{
		return ReadTexel16(PixelAddress16SZ(x, y, TEX0.TBP0, TEX0.TBW), TEXA);
	}

	__forceinline void ReadImageX(int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const
	{
		m_readImageX(*this, tx, ty, dst, len, BITBLTBUF, TRXPOS, TRXREG);
	}

	void ReadTexture(const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	//

	void SaveBMP(const std::string& fn, u32 bp, u32 bw, u32 psm, int w, int h);
};

constexpr inline GSOffset GSOffset::fromKnownPSM(u32 bp, u32 bw, GS_PSM psm)
{
	switch (psm)
	{
		case PSMCT32:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSMCT24:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSMCT16:  return GSOffset(GSLocalMemory::swizzle16,   bp, bw, psm);
		case PSMCT16S: return GSOffset(GSLocalMemory::swizzle16S,  bp, bw, psm);
		case PSGPU24:  return GSOffset(GSLocalMemory::swizzle16,   bp, bw, psm);
		case PSMT8:    return GSOffset(GSLocalMemory::swizzle8,    bp, bw, psm);
		case PSMT4:    return GSOffset(GSLocalMemory::swizzle4,    bp, bw, psm);
		case PSMT8H:   return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSMT4HL:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSMT4HH:  return GSOffset(GSLocalMemory::swizzle32,   bp, bw, psm);
		case PSMZ32:   return GSOffset(GSLocalMemory::swizzle32Z,  bp, bw, psm);
		case PSMZ24:   return GSOffset(GSLocalMemory::swizzle32Z,  bp, bw, psm);
		case PSMZ16:   return GSOffset(GSLocalMemory::swizzle16Z,  bp, bw, psm);
		case PSMZ16S:  return GSOffset(GSLocalMemory::swizzle16SZ, bp, bw, psm);
	}
	return GSOffset(GSLocalMemory::swizzle32, bp, bw, psm);
}
