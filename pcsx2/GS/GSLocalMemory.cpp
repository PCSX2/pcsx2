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

#include "PrecompiledHeader.h"
#include "GS/GS.h"
#include "GS/GSLocalMemory.h"
#include "GS/GSExtra.h"
#include "GS/GSPng.h"
#include <unordered_set>

template <typename Fn>
static void foreachBlock(const GSOffset& off, GSLocalMemory* mem, const GSVector4i& r, u8* dst, int dstpitch, int bpp, Fn&& fn)
{
	ASSERT(off.isBlockAligned(r));
	GSOffset::BNHelper bn = off.bnMulti(r.left, r.top);
	int right = r.right >> off.blockShiftX();
	int bottom = r.bottom >> off.blockShiftY();

	int offset = dstpitch << off.blockShiftY();
	int xAdd = (1 << off.blockShiftX()) * (bpp / 8);

	for (; bn.blkY() < bottom; bn.nextBlockY(), dst += offset)
	{
		for (int x = 0; bn.blkX() < right; bn.nextBlockX(), x += xAdd)
		{
			const u8* src = mem->BlockPtr(bn.value());
			u8* read_dst = dst + x;
			fn(read_dst, src);
		}
	}
}

//

constexpr GSSwizzleInfo GSLocalMemory::swizzle32;
constexpr GSSwizzleInfo GSLocalMemory::swizzle32Z;
constexpr GSSwizzleInfo GSLocalMemory::swizzle16;
constexpr GSSwizzleInfo GSLocalMemory::swizzle16S;
constexpr GSSwizzleInfo GSLocalMemory::swizzle16Z;
constexpr GSSwizzleInfo GSLocalMemory::swizzle16SZ;
constexpr GSSwizzleInfo GSLocalMemory::swizzle8;
constexpr GSSwizzleInfo GSLocalMemory::swizzle4;

//

GSLocalMemory::psm_t GSLocalMemory::m_psm[64];
GSLocalMemory::readImage GSLocalMemory::m_readImageX;

//

GSLocalMemory::GSLocalMemory()
	: m_clut(this)
{
	m_vm8 = (u8*)GSAllocateWrappedMemory(m_vmsize, 4);
	if (!m_vm8)
		pxFailRel("Failed to allocate GS memory storage.");

	memset(m_vm8, 0, m_vmsize);

	MULTI_ISA_SELECT(GSLocalMemoryPopulateFunctions)(*this);

	for (psm_t& psm : m_psm)
	{
		psm.info = GSLocalMemory::swizzle32;
		psm.rp = &GSLocalMemory::ReadPixel32;
		psm.rpa = &GSLocalMemory::ReadPixel32;
		psm.wp = &GSLocalMemory::WritePixel32;
		psm.wpa = &GSLocalMemory::WritePixel32;
		psm.rt = &GSLocalMemory::ReadTexel32;
		psm.rta = &GSLocalMemory::ReadTexel32;
		psm.wfa = &GSLocalMemory::WritePixel32;
		psm.bpp = psm.trbpp = 32;
		psm.pal = 0;
		psm.bs = GSVector2i(8, 8);
		psm.pgs = GSVector2i(64, 32);
		psm.msk = 0xff;
		psm.depth = 0;
		psm.fmsk = 0xffffffff;
	}

	m_psm[PSGPU24].info = GSLocalMemory::swizzle16;
	m_psm[PSMCT16].info = GSLocalMemory::swizzle16;
	m_psm[PSMCT16S].info = GSLocalMemory::swizzle16S;
	m_psm[PSMT8].info = GSLocalMemory::swizzle8;
	m_psm[PSMT4].info = GSLocalMemory::swizzle4;
	m_psm[PSMZ32].info = GSLocalMemory::swizzle32Z;
	m_psm[PSMZ24].info = GSLocalMemory::swizzle32Z;
	m_psm[PSMZ16].info = GSLocalMemory::swizzle16Z;
	m_psm[PSMZ16S].info = GSLocalMemory::swizzle16SZ;

	m_psm[PSMCT24].rp = &GSLocalMemory::ReadPixel24;
	m_psm[PSMCT16].rp = &GSLocalMemory::ReadPixel16;
	m_psm[PSMCT16S].rp = &GSLocalMemory::ReadPixel16S;
	m_psm[PSMT8].rp = &GSLocalMemory::ReadPixel8;
	m_psm[PSMT4].rp = &GSLocalMemory::ReadPixel4;
	m_psm[PSMT8H].rp = &GSLocalMemory::ReadPixel8H;
	m_psm[PSMT4HL].rp = &GSLocalMemory::ReadPixel4HL;
	m_psm[PSMT4HH].rp = &GSLocalMemory::ReadPixel4HH;
	m_psm[PSMZ32].rp = &GSLocalMemory::ReadPixel32Z;
	m_psm[PSMZ24].rp = &GSLocalMemory::ReadPixel24Z;
	m_psm[PSMZ16].rp = &GSLocalMemory::ReadPixel16Z;
	m_psm[PSMZ16S].rp = &GSLocalMemory::ReadPixel16SZ;

	m_psm[PSMCT24].rpa = &GSLocalMemory::ReadPixel24;
	m_psm[PSMCT16].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSMCT16S].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSMT8].rpa = &GSLocalMemory::ReadPixel8;
	m_psm[PSMT4].rpa = &GSLocalMemory::ReadPixel4;
	m_psm[PSMT8H].rpa = &GSLocalMemory::ReadPixel8H;
	m_psm[PSMT4HL].rpa = &GSLocalMemory::ReadPixel4HL;
	m_psm[PSMT4HH].rpa = &GSLocalMemory::ReadPixel4HH;
	m_psm[PSMZ32].rpa = &GSLocalMemory::ReadPixel32;
	m_psm[PSMZ24].rpa = &GSLocalMemory::ReadPixel24;
	m_psm[PSMZ16].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSMZ16S].rpa = &GSLocalMemory::ReadPixel16;

	m_psm[PSMCT32].wp = &GSLocalMemory::WritePixel32;
	m_psm[PSMCT24].wp = &GSLocalMemory::WritePixel24;
	m_psm[PSMCT16].wp = &GSLocalMemory::WritePixel16;
	m_psm[PSMCT16S].wp = &GSLocalMemory::WritePixel16S;
	m_psm[PSMT8].wp = &GSLocalMemory::WritePixel8;
	m_psm[PSMT4].wp = &GSLocalMemory::WritePixel4;
	m_psm[PSMT8H].wp = &GSLocalMemory::WritePixel8H;
	m_psm[PSMT4HL].wp = &GSLocalMemory::WritePixel4HL;
	m_psm[PSMT4HH].wp = &GSLocalMemory::WritePixel4HH;
	m_psm[PSMZ32].wp = &GSLocalMemory::WritePixel32Z;
	m_psm[PSMZ24].wp = &GSLocalMemory::WritePixel24Z;
	m_psm[PSMZ16].wp = &GSLocalMemory::WritePixel16Z;
	m_psm[PSMZ16S].wp = &GSLocalMemory::WritePixel16SZ;

	m_psm[PSMCT32].wpa = &GSLocalMemory::WritePixel32;
	m_psm[PSMCT24].wpa = &GSLocalMemory::WritePixel24;
	m_psm[PSMCT16].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSMCT16S].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSMT8].wpa = &GSLocalMemory::WritePixel8;
	m_psm[PSMT4].wpa = &GSLocalMemory::WritePixel4;
	m_psm[PSMT8H].wpa = &GSLocalMemory::WritePixel8H;
	m_psm[PSMT4HL].wpa = &GSLocalMemory::WritePixel4HL;
	m_psm[PSMT4HH].wpa = &GSLocalMemory::WritePixel4HH;
	m_psm[PSMZ32].wpa = &GSLocalMemory::WritePixel32;
	m_psm[PSMZ24].wpa = &GSLocalMemory::WritePixel24;
	m_psm[PSMZ16].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSMZ16S].wpa = &GSLocalMemory::WritePixel16;

	m_psm[PSMCT24].rt = &GSLocalMemory::ReadTexel24;
	m_psm[PSMCT16].rt = &GSLocalMemory::ReadTexel16;
	m_psm[PSMCT16S].rt = &GSLocalMemory::ReadTexel16S;
	m_psm[PSMT8].rt = &GSLocalMemory::ReadTexel8;
	m_psm[PSMT4].rt = &GSLocalMemory::ReadTexel4;
	m_psm[PSMT8H].rt = &GSLocalMemory::ReadTexel8H;
	m_psm[PSMT4HL].rt = &GSLocalMemory::ReadTexel4HL;
	m_psm[PSMT4HH].rt = &GSLocalMemory::ReadTexel4HH;
	m_psm[PSMZ32].rt = &GSLocalMemory::ReadTexel32Z;
	m_psm[PSMZ24].rt = &GSLocalMemory::ReadTexel24Z;
	m_psm[PSMZ16].rt = &GSLocalMemory::ReadTexel16Z;
	m_psm[PSMZ16S].rt = &GSLocalMemory::ReadTexel16SZ;

	m_psm[PSMCT24].rta = &GSLocalMemory::ReadTexel24;
	m_psm[PSMCT16].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSMCT16S].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSMT8].rta = &GSLocalMemory::ReadTexel8;
	m_psm[PSMT4].rta = &GSLocalMemory::ReadTexel4;
	m_psm[PSMT8H].rta = &GSLocalMemory::ReadTexel8H;
	m_psm[PSMT4HL].rta = &GSLocalMemory::ReadTexel4HL;
	m_psm[PSMT4HH].rta = &GSLocalMemory::ReadTexel4HH;
	m_psm[PSMZ24].rta = &GSLocalMemory::ReadTexel24;
	m_psm[PSMZ16].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSMZ16S].rta = &GSLocalMemory::ReadTexel16;

	m_psm[PSMCT24].wfa = &GSLocalMemory::WritePixel24;
	m_psm[PSMCT16].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSMCT16S].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSMZ24].wfa = &GSLocalMemory::WritePixel24;
	m_psm[PSMZ16].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSMZ16S].wfa = &GSLocalMemory::WriteFrame16;

	m_psm[PSGPU24].bpp = 16;
	m_psm[PSMCT16].bpp = m_psm[PSMCT16S].bpp = 16;
	m_psm[PSMT8].bpp = 8;
	m_psm[PSMT4].bpp = 4;
	m_psm[PSMZ16].bpp = m_psm[PSMZ16S].bpp = 16;

	m_psm[PSMCT24].trbpp = 24;
	m_psm[PSGPU24].trbpp = 16;
	m_psm[PSMCT16].trbpp = m_psm[PSMCT16S].trbpp = 16;
	m_psm[PSMT8].trbpp = m_psm[PSMT8H].trbpp = 8;
	m_psm[PSMT4].trbpp = m_psm[PSMT4HL].trbpp = m_psm[PSMT4HH].trbpp = 4;
	m_psm[PSMZ24].trbpp = 24;
	m_psm[PSMZ16].trbpp = m_psm[PSMZ16S].trbpp = 16;

	m_psm[PSMT8].pal = m_psm[PSMT8H].pal = 256;
	m_psm[PSMT4].pal = m_psm[PSMT4HL].pal = m_psm[PSMT4HH].pal = 16;

	for (psm_t& psm : m_psm)
		psm.fmt = 3;
	m_psm[PSMCT32].fmt = m_psm[PSMZ32].fmt = 0;
	m_psm[PSMCT24].fmt = m_psm[PSMZ24].fmt = 1;
	m_psm[PSMCT16].fmt = m_psm[PSMZ16].fmt = 2;
	m_psm[PSMCT16S].fmt = m_psm[PSMZ16S].fmt = 2;


	m_psm[PSGPU24].bs = GSVector2i(16, 8);
	m_psm[PSMCT16].bs = m_psm[PSMCT16S].bs = GSVector2i(16, 8);
	m_psm[PSMT8].bs = GSVector2i(16, 16);
	m_psm[PSMT4].bs = GSVector2i(32, 16);
	m_psm[PSMZ16].bs = m_psm[PSMZ16S].bs = GSVector2i(16, 8);

	m_psm[PSGPU24].pgs = GSVector2i(64, 64);
	m_psm[PSMCT16].pgs = m_psm[PSMCT16S].pgs = GSVector2i(64, 64);
	m_psm[PSMT8].pgs = GSVector2i(128, 64);
	m_psm[PSMT4].pgs = GSVector2i(128, 128);
	m_psm[PSMZ16].pgs = m_psm[PSMZ16S].pgs = GSVector2i(64, 64);

	m_psm[PSMCT24].msk = 0x3f;
	m_psm[PSMZ24].msk = 0x3f;
	m_psm[PSMT8H].msk = 0xc0;
	m_psm[PSMT4HL].msk = 0x40;
	m_psm[PSMT4HH].msk = 0x80;

	m_psm[PSMZ32].depth  = 1;
	m_psm[PSMZ24].depth  = 1;
	m_psm[PSMZ16].depth  = 1;
	m_psm[PSMZ16S].depth = 1;

	m_psm[PSMCT24].fmsk = 0x00FFFFFF;
	m_psm[PSGPU24].fmsk = 0x00FFFFFF;
	m_psm[PSMCT16].fmsk = 0x80F8F8F8;
	m_psm[PSMCT16S].fmsk = 0x80F8F8F8;
	m_psm[PSMT8H].fmsk = 0xFF000000;
	m_psm[PSMT4HL].fmsk = 0x0F000000;
	m_psm[PSMT4HH].fmsk = 0xF0000000;
	m_psm[PSMZ24].fmsk = 0x00FFFFFF;
	m_psm[PSMZ16].fmsk = 0x80F8F8F8;
	m_psm[PSMZ16S].fmsk = 0x80F8F8F8;
}

GSLocalMemory::~GSLocalMemory()
{
	if (m_vm8)
		GSFreeWrappedMemory(m_vm8, m_vmsize, 4);

	for (auto& i : m_pomap)
		_aligned_free(i.second);
	for (auto& i : m_po4map)
		_aligned_free(i.second);

	for (auto& i : m_p2tmap)
	{
		delete[] i.second;
	}
}

GSPixelOffset* GSLocalMemory::GetPixelOffset(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF)
{
	u32 fbp = FRAME.Block();
	u32 zbp = ZBUF.Block();
	u32 fpsm = FRAME.PSM;
	u32 zpsm = ZBUF.PSM;
	u32 bw = FRAME.FBW;

	ASSERT(m_psm[fpsm].trbpp > 8 || m_psm[zpsm].trbpp > 8);

	// "(psm & 0x0f) ^ ((psm & 0xf0) >> 2)" creates 4 bit unique identifiers for render target formats (only)

	u32 fpsm_hash = (fpsm & 0x0f) ^ ((fpsm & 0x30) >> 2);
	u32 zpsm_hash = (zpsm & 0x0f) ^ ((zpsm & 0x30) >> 2);

	u32 hash = (FRAME.FBP << 0) | (ZBUF.ZBP << 9) | (bw << 18) | (fpsm_hash << 24) | (zpsm_hash << 28);

	auto it = m_pomap.find(hash);

	if (it != m_pomap.end())
	{
		return it->second;
	}

	GSPixelOffset* off = (GSPixelOffset*)_aligned_malloc(sizeof(GSPixelOffset), VECTOR_ALIGNMENT);

	off->hash = hash;
	off->fbp = fbp;
	off->zbp = zbp;
	off->fpsm = fpsm;
	off->zpsm = zpsm;
	off->bw = bw;

	int fs = m_psm[fpsm].bpp >> 5;
	int zs = m_psm[zpsm].bpp >> 5;

	for (int i = 0; i < 2048; i++)
	{
		off->row[i].x = (int)m_psm[fpsm].info.pa(0, i, fbp, bw) << fs;
		off->row[i].y = (int)m_psm[zpsm].info.pa(0, i, zbp, bw) << zs;
	}

	for (int i = 0; i < 2048; i++)
	{
		off->col[i].x = (m_psm[fpsm].info.pa(i, 0, 0, 32) - m_psm[fpsm].info.pa(0, 0, 0, 32)) << fs;
		off->col[i].y = (m_psm[zpsm].info.pa(i, 0, 0, 32) - m_psm[zpsm].info.pa(0, 0, 0, 32)) << zs;
	}

	m_pomap[hash] = off;

	return off;
}

GSPixelOffset4* GSLocalMemory::GetPixelOffset4(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF)
{
	u32 fbp = FRAME.Block();
	u32 zbp = ZBUF.Block();
	u32 fpsm = FRAME.PSM;
	u32 zpsm = ZBUF.PSM;
	u32 bw = FRAME.FBW;

	ASSERT(m_psm[fpsm].trbpp > 8 || m_psm[zpsm].trbpp > 8);

	// "(psm & 0x0f) ^ ((psm & 0xf0) >> 2)" creates 4 bit unique identifiers for render target formats (only)

	u32 fpsm_hash = (fpsm & 0x0f) ^ ((fpsm & 0x30) >> 2);
	u32 zpsm_hash = (zpsm & 0x0f) ^ ((zpsm & 0x30) >> 2);

	u32 hash = (FRAME.FBP << 0) | (ZBUF.ZBP << 9) | (bw << 18) | (fpsm_hash << 24) | (zpsm_hash << 28);

	auto it = m_po4map.find(hash);

	if (it != m_po4map.end())
	{
		return it->second;
	}

	GSPixelOffset4* off = (GSPixelOffset4*)_aligned_malloc(sizeof(GSPixelOffset4), VECTOR_ALIGNMENT);

	off->hash = hash;
	off->fbp = fbp;
	off->zbp = zbp;
	off->fpsm = fpsm;
	off->zpsm = zpsm;
	off->bw = bw;

	int fs = m_psm[fpsm].bpp >> 5;
	int zs = m_psm[zpsm].bpp >> 5;

	for (int i = 0; i < 2048; i++)
	{
		off->row[i].x = (int)m_psm[fpsm].info.pa(0, i, fbp, bw) << fs;
		off->row[i].y = (int)m_psm[zpsm].info.pa(0, i, zbp, bw) << zs;
	}

	for (int i = 0; i < 512; i++)
	{
		off->col[i].x = (m_psm[fpsm].info.pa(i * 4, 0, 0, 32) - m_psm[fpsm].info.pa(0, 0, 0, 32)) << fs;
		off->col[i].y = (m_psm[zpsm].info.pa(i * 4, 0, 0, 32) - m_psm[zpsm].info.pa(0, 0, 0, 32)) << zs;
	}

	m_po4map[hash] = off;

	return off;
}

std::vector<GSVector2i>* GSLocalMemory::GetPage2TileMap(const GIFRegTEX0& TEX0)
{
	u64 hash = TEX0.U64 & 0x3ffffffffull; // TBP0 TBW PSM TW TH

	auto it = m_p2tmap.find(hash);

	if (it != m_p2tmap.end())
	{
		return it->second;
	}

	GSVector2i bs = m_psm[TEX0.PSM].bs;

	int tw = std::max<int>(1 << TEX0.TW, bs.x);
	int th = std::max<int>(1 << TEX0.TH, bs.y);

	GSOffset off = GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);
	GSOffset::BNHelper bn = off.bnMulti(0, 0);

	std::unordered_map<u32, std::unordered_set<u32>> tmp; // key = page, value = y:x, 7 bits each, max 128x128 tiles for the worst case (1024x1024 32bpp 8x8 blocks)

	for (; bn.blkY() < (th >> off.blockShiftY()); bn.nextBlockY())
	{
		for (; bn.blkX() < (tw >> off.blockShiftX()); bn.nextBlockX())
		{
			u32 page = (bn.value() >> 5) % MAX_PAGES;

			tmp[page].insert((bn.blkY() << 7) + bn.blkX());
		}
	}

	// combine the lower 5 bits of the address into a 9:5 pointer:mask form, so the "valid bits" can be tested against an u32 array

	auto p2t = new std::vector<GSVector2i>[MAX_PAGES];

	for (const auto& i : tmp)
	{
		u32 page = i.first;

		auto& tiles = i.second;

		std::unordered_map<u32, u32> m;

		for (const auto addr : tiles)
		{
			u32 row = addr >> 5;
			u32 col = 1 << (addr & 31);

			auto k = m.find(row);

			if (k != m.end())
			{
				k->second |= col;
			}
			else
			{
				m[row] = col;
			}
		}

		// Allocate vector with initial size
		p2t[page].reserve(m.size());

		// sort by x and flip the mask (it will be used to erase a lot of bits in a loop, [x] &= ~y)

		for (const auto& j : m)
		{
			p2t[page].push_back(GSVector2i(j.first, ~j.second));
		}

		std::sort(p2t[page].begin(), p2t[page].end(), [](const GSVector2i& a, const GSVector2i& b) { return a.x < b.x; });
	}

	m_p2tmap[hash] = p2t;

	return p2t;
}

bool GSLocalMemory::IsPageAligned(u32 psm, const GSVector4i& rc)
{
	const psm_t& psm_s = m_psm[psm];
	const GSVector4i pgmsk = GSVector4i(psm_s.pgs).xyxy() - GSVector4i(1);
	return (rc & pgmsk).eq(GSVector4i::zero());
}

u32 GSLocalMemory::GetStartBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect)
{
	u32 result = m_psm[psm].info.bn(rect.x, rect.y, bp, bw); // Valid only for color formats

	// If rect is page aligned, we can assume it's the start of the page. Z formats don't place block 0
	// in the top-left, so we have to round them down.
	const GSVector2i page_size = GSLocalMemory::m_psm[psm].pgs;
	if ((rect.x & (page_size.x - 1)) == 0 && (rect.y & (page_size.y - 1)) == 0)
	{
		constexpr u32 page_mask = (1 << 5) - 1;
		result &= ~page_mask;
	}

	return result;
}

u32 GSLocalMemory::GetEndBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect)
{
	u32 result = m_psm[psm].info.bn(rect.z - 1, rect.w - 1, bp, bw); // Valid only for color formats

	// If rect is page aligned, we can assume it's the start of the next block-1 as the max block position.
	// Using real end point for Z formats causes problems because it's a lower value.
	const GSVector2i page_size = GSLocalMemory::m_psm[psm].pgs;
	if ((rect.z & (page_size.x - 1)) == 0 && (rect.w & (page_size.y - 1)) == 0)
	{
		constexpr u32 page_mask = (1 << 5) - 1;
		result = (((result + page_mask) & ~page_mask)) - 1;
	}

	return result;
}

u32 GSLocalMemory::GetUnwrappedEndBlockAddress(u32 bp, u32 bw, u32 psm, GSVector4i rect)
{
	const u32 result = GetEndBlockAddress(bp, bw, psm, rect);
	return (result < bp) ? (result + MAX_BLOCKS) : result;
}

GSVector4i GSLocalMemory::GetRectForPageOffset(u32 base_bp, u32 offset_bp, u32 bw, u32 psm)
{
	pxAssert((base_bp % BLOCKS_PER_PAGE) == 0 && (offset_bp % BLOCKS_PER_PAGE) == 0);

	const u32 page_offset = (offset_bp - base_bp) >> 5;
	const GSVector2i& pgs = m_psm[psm].pgs;
	const u32 valid_bw = std::max(1U, bw);
	const GSVector2i page_offset_xy = GSVector2i(page_offset % valid_bw, page_offset / std::max(1U, valid_bw));
	return GSVector4i(pgs * page_offset_xy).xyxy() + GSVector4i::loadh(pgs);
}

bool GSLocalMemory::HasOverlap(const u32 src_bp, const u32 src_bw, const u32 src_psm, const GSVector4i src_rect
							, const u32 dst_bp, const u32 dst_bw, const u32 dst_psm, const GSVector4i dst_rect)
{
	const u32 src_start_bp = GSLocalMemory::GetStartBlockAddress(src_bp, src_bw, src_psm, src_rect) & ~(BLOCKS_PER_PAGE - 1);
	const u32 dst_start_bp = GSLocalMemory::GetStartBlockAddress(dst_bp, dst_bw, dst_psm, dst_rect) & ~(BLOCKS_PER_PAGE - 1);

	u32 src_end_bp = ((GSLocalMemory::GetEndBlockAddress(src_bp, src_bw, src_psm, src_rect) + 1) + (BLOCKS_PER_PAGE - 1)) & ~(BLOCKS_PER_PAGE - 1);
	u32 dst_end_bp = ((GSLocalMemory::GetEndBlockAddress(dst_bp, dst_bw, dst_psm, dst_rect) + 1) + (BLOCKS_PER_PAGE - 1)) & ~(BLOCKS_PER_PAGE - 1);
	
	if (src_start_bp == src_end_bp)
	{
		src_end_bp = (src_end_bp + BLOCKS_PER_PAGE) & ~(MAX_BLOCKS - 1);
	}

	if (dst_start_bp == dst_end_bp)
	{
		dst_end_bp = (dst_end_bp + BLOCKS_PER_PAGE) & ~(MAX_BLOCKS - 1);
	}

	// Source has wrapped, 2 separate checks.
	if (src_end_bp <= src_start_bp)
	{
		// Destination has also wrapped, so they *have* to overlap.
		if (dst_end_bp <= dst_start_bp)
		{
			return true;
		}
		else
		{
			if (dst_end_bp > src_start_bp)
				return true;

			if (dst_start_bp < src_end_bp)
				return true;
		}
	}
	else // No wrapping on source.
	{
		// Destination wraps.
		if (dst_end_bp <= dst_start_bp)
		{
			if (src_end_bp > dst_start_bp)
				return true;

			if (src_start_bp < dst_end_bp)
				return true;
		}
		else
		{
			if (dst_start_bp < src_end_bp && dst_end_bp > src_start_bp)
				return true;
		}
	}

	return false;
}

///////////////////

void GSLocalMemory::ReadTexture(const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const psm_t& psm = m_psm[off.psm()];

	readTexel rt = psm.rt;
	readTexture rtx = psm.rtx;

	if (r.width() < psm.bs.x || r.height() < psm.bs.y || (r.left & (psm.bs.x - 1)) || (r.top & (psm.bs.y - 1)) || (r.right & (psm.bs.x - 1)) || (r.bottom & (psm.bs.y - 1)))
	{
		GIFRegTEX0 TEX0;

		TEX0.TBP0 = off.bp();
		TEX0.TBW = off.bw();
		TEX0.PSM = off.psm();

		GSVector4i cr = r.ralign<Align_Inside>(psm.bs);

		bool aligned = ((size_t)(dst + (cr.left - r.left) * sizeof(u32)) & 0xf) == 0;

		if (cr.rempty() || !aligned)
		{
			// TODO: expand r to block size, read into temp buffer

			if (!aligned)
				printf("unaligned memory pointer passed to ReadTexture\n");

			for (int y = r.top; y < r.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((u32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}
		}
		else
		{
			u8* crdst = dst;

			for (int y = r.top; y < cr.top; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((u32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			for (int y = cr.top; y < cr.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < cr.left; x++, i++)
				{
					((u32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}

				for (int x = cr.right, i = x - r.left; x < r.right; x++, i++)
				{
					((u32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			for (int y = cr.bottom; y < r.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((u32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			if (!cr.rempty())
			{
				crdst += dstpitch * (cr.top - r.top);
				crdst += sizeof(u32) * (cr.left - r.left);
				rtx(*this, off, cr, crdst, dstpitch, TEXA);
			}
		}
	}
	else
	{
		rtx(*this, off, r, dst, dstpitch, TEXA);
	}
}

//

#include "Renderers/SW/GSTextureSW.h"

void GSLocalMemory::SaveBMP(const std::string& fn, u32 bp, u32 bw, u32 psm, int w, int h)
{
	int pitch = w * 4;
	int size = pitch * h;
	void* bits = _aligned_malloc(size, VECTOR_ALIGNMENT);

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = bp;
	TEX0.TBW = bw;
	TEX0.PSM = psm;

	readPixel rp = m_psm[psm].rp;

	u8* p = (u8*)bits;

	for (int j = 0; j < h; j++, p += pitch)
	{
		for (int i = 0; i < w; i++)
		{
			((u32*)p)[i] = (this->*rp)(i, j, TEX0.TBP0, TEX0.TBW);
		}
	}

#ifdef PCSX2_DEVBUILD
	GSPng::Save(GSPng::RGB_A_PNG, fn, static_cast<u8*>(bits), w, h, pitch, GSConfig.PNGCompressionLevel, false);
#else
	GSPng::Save(GSPng::RGB_PNG, fn, static_cast<u8*>(bits), w, h, pitch, GSConfig.PNGCompressionLevel, false);
#endif

	_aligned_free(bits);
}

// GSOffset

namespace
{
	/// Helper for GSOffset::pageLooperForRect
	struct alignas(16) TextureAligned
	{
		int ox1, oy1, ox2, oy2; ///< Block-aligned outer rect (smallest rectangle containing the original that is block-aligned)
		int ix1, iy1, ix2, iy2; ///< Page-aligned inner rect (largest rectangle inside original that is page-aligned)
	};

	/// Helper for GSOffset::pageLooperForRect
	TextureAligned align(const GSVector4i& rect, const GSVector2i& blockMask, const GSVector2i& pageMask, int blockShiftX, int blockShiftY)
	{
		GSVector4i outer = rect.ralign_presub<Align_Outside>(blockMask);
		GSVector4i inner = outer.ralign_presub<Align_Inside>(pageMask);
#if _M_SSE >= 0x501
		GSVector4i shift = GSVector4i(blockShiftX, blockShiftY).xyxy();
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
		GSVector4i outerX = outer.sra32(blockShiftX);
		GSVector4i outerY = outer.sra32(blockShiftY);
		GSVector4i innerX = inner.sra32(blockShiftX);
		GSVector4i innerY = inner.sra32(blockShiftY);
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

} // namespace

GSOffset::PageLooper GSOffset::pageLooperForRect(const GSVector4i& rect) const
{
	// Plan:
	// - Split texture into tiles on page lines
	// - When bp is not page-aligned, each page-sized tile of texture may touch the page on either side of it
	//   e.g. if bp is 1 on PSMCT32, the top left tile uses page 1 if the rect covers the bottom right block, and uses page 0 if the rect covers any block other than the bottom right
	// - Center tiles (ones that aren't first or last) cover all blocks that the first and last do in a row
	//   Therefore, if the first tile in a row touches the higher of its two pages, subsequent non-last tiles will at least touch the higher of their pages as well (and same for center to last, etc)
	// - Based on the above, we calculate the range of pages a row could touch with full coverage, then add one to the start if the first tile doesn't touch its lower page, and subtract one from the end if the last tile doesn't touch its upper page
	// - This is done separately for the first and last rows in the y axis, as they may not have the same coverage as a row in the middle

	PageLooper out;
	const int topPg = rect.top >> m_pageShiftY;
	const int botPg = (rect.bottom + m_pageMask.y) >> m_pageShiftY;
	const int blockOff = m_bp & 0x1f;
	const int invBlockOff = 32 - blockOff;
	const bool aligned = blockOff == 0;

	out.bp = (m_bp >> 5) + topPg * m_bwPg;
	out.yInc = m_bwPg;
	out.yCnt = botPg - topPg;
	out.firstRowPgXStart = out.midRowPgXStart = out.lastRowPgXStart = rect.left >> m_pageShiftX;
	out.firstRowPgXEnd = out.midRowPgXEnd = out.lastRowPgXEnd = ((rect.right + m_pageMask.x) >> m_pageShiftX) + !aligned;
	out.slowPath = static_cast<u32>(out.yCnt * out.yInc + out.midRowPgXEnd - out.midRowPgXStart) > MAX_PAGES;

	// Page-aligned bp is easy, all tiles touch their lower page but not the upper
	if (aligned)
		return out;

	TextureAligned a = align(rect, m_blockMask, m_pageMask, m_blockShiftX, m_blockShiftY);

	const int shiftX = m_pageShiftX - m_blockShiftX;
	const int shiftY = m_pageShiftY - m_blockShiftY;
	const int blkW = 1 << shiftX;
	const int blkH = 1 << shiftY;

	/// Does the given rect in the texture touch the given page?
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

	// If y stays within one page, loop functions will only look at the `first` fields
	if (staysWithinOnePage(a.oy1, a.oy2, a.iy1, a.iy2))
	{
		adjustStartEnd(out.firstRowPgXStart, out.firstRowPgXEnd, a.oy1, a.oy2);
		return out;
	}

	// Mid rows (if any) will always have full range of y
	adjustStartEnd(out.midRowPgXStart, out.midRowPgXEnd, 0, blkH);
	// For first and last rows, either copy mid if they are full height or separately calculate them with their smaller ranges
	if (a.oy1 != a.iy1)
	{
		adjustStartEnd(out.firstRowPgXStart, out.firstRowPgXEnd, a.oy1, a.iy1);
	}
	else
	{
		out.firstRowPgXStart = out.midRowPgXStart;
		out.firstRowPgXEnd   = out.midRowPgXEnd;
	}
	if (a.oy2 != a.iy2)
	{
		adjustStartEnd(out.lastRowPgXStart, out.lastRowPgXEnd, a.iy2, a.oy2);
	}
	else
	{
		out.lastRowPgXStart = out.midRowPgXStart;
		out.lastRowPgXEnd   = out.midRowPgXEnd;
	}
	return out;
}
