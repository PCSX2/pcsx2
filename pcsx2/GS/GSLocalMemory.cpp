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
#include "GSLocalMemory.h"
#include "GS.h"

template <typename Fn>
static void foreachBlock(const GSOffset& off, GSLocalMemory* mem, const GSVector4i& r, uint8* dst, int dstpitch, int bpp, Fn&& fn)
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
			const uint8* src = mem->BlockPtr(bn.value());
			uint8* read_dst = dst + x;
			fn(read_dst, src);
		}
	}
}

//

uint32 GSLocalMemory::pageOffset32[32][32][64];
uint32 GSLocalMemory::pageOffset32Z[32][32][64];
uint32 GSLocalMemory::pageOffset16[32][64][64];
uint32 GSLocalMemory::pageOffset16S[32][64][64];
uint32 GSLocalMemory::pageOffset16Z[32][64][64];
uint32 GSLocalMemory::pageOffset16SZ[32][64][64];
uint32 GSLocalMemory::pageOffset8[32][64][128];
uint32 GSLocalMemory::pageOffset4[32][128][128];

int GSLocalMemory::rowOffset32[4096];
int GSLocalMemory::rowOffset32Z[4096];
int GSLocalMemory::rowOffset16[4096];
int GSLocalMemory::rowOffset16S[4096];
int GSLocalMemory::rowOffset16Z[4096];
int GSLocalMemory::rowOffset16SZ[4096];
int GSLocalMemory::rowOffset8[2][4096];
int GSLocalMemory::rowOffset4[2][4096];

//

GSLocalMemory::psm_t GSLocalMemory::m_psm[64];

//

GSLocalMemory::GSLocalMemory()
	: m_clut(this)
{
	m_use_fifo_alloc = theApp.GetConfigB("UserHacks") && theApp.GetConfigB("wrap_gs_mem");
	switch (theApp.GetCurrentRendererType())
	{
		case GSRendererType::OGL_SW:
			m_use_fifo_alloc = true;
			break;
		default:
			break;
	}

	if (m_use_fifo_alloc)
		m_vm8 = (uint8*)fifo_alloc(m_vmsize, 4);
	else
		m_vm8 = nullptr;

	// Either we don't use fifo alloc or we get an error.
	if (m_vm8 == nullptr)
	{
		m_vm8 = (uint8*)vmalloc(m_vmsize * 4, false);
		m_use_fifo_alloc = false;
	}

	m_vm16 = (uint16*)m_vm8;
	m_vm32 = (uint32*)m_vm8;

	memset(m_vm8, 0, m_vmsize);

	for (int bp = 0; bp < 32; bp++)
	{
		for (int y = 0; y < 32; y++) for (int x = 0; x < 64; x++)
		{
			pageOffset32[bp][y][x] = PixelAddressOrg32(x, y, bp, 0);
			pageOffset32Z[bp][y][x] = PixelAddressOrg32Z(x, y, bp, 0);
		}

		for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
		{
			pageOffset16[bp][y][x] = PixelAddressOrg16(x, y, bp, 0);
			pageOffset16S[bp][y][x] = PixelAddressOrg16S(x, y, bp, 0);
			pageOffset16Z[bp][y][x] = PixelAddressOrg16Z(x, y, bp, 0);
			pageOffset16SZ[bp][y][x] = PixelAddressOrg16SZ(x, y, bp, 0);
		}

		for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++)
		{
			pageOffset8[bp][y][x] = PixelAddressOrg8(x, y, bp, 0);
		}

		for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++)
		{
			pageOffset4[bp][y][x] = PixelAddressOrg4(x, y, bp, 0);
		}
	}

	for (size_t x = 0; x < countof(rowOffset32); x++)
	{
		rowOffset32[x] = (int)PixelAddress32(x & 0x7ff, 0, 0, 32) - (int)PixelAddress32(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset32Z); x++)
	{
		rowOffset32Z[x] = (int)PixelAddress32Z(x & 0x7ff, 0, 0, 32) - (int)PixelAddress32Z(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset16); x++)
	{
		rowOffset16[x] = (int)PixelAddress16(x & 0x7ff, 0, 0, 32) - (int)PixelAddress16(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset16S); x++)
	{
		rowOffset16S[x] = (int)PixelAddress16S(x & 0x7ff, 0, 0, 32) - (int)PixelAddress16S(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset16Z); x++)
	{
		rowOffset16Z[x] = (int)PixelAddress16Z(x & 0x7ff, 0, 0, 32) - (int)PixelAddress16Z(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset16SZ); x++)
	{
		rowOffset16SZ[x] = (int)PixelAddress16SZ(x & 0x7ff, 0, 0, 32) - (int)PixelAddress16SZ(0, 0, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset8[0]); x++)
	{
		rowOffset8[0][x] = (int)PixelAddress8(x & 0x7ff, 0, 0, 32) - (int)PixelAddress8(0, 0, 0, 32);
		rowOffset8[1][x] = (int)PixelAddress8(x & 0x7ff, 2, 0, 32) - (int)PixelAddress8(0, 2, 0, 32);
	}

	for (size_t x = 0; x < countof(rowOffset4[0]); x++)
	{
		rowOffset4[0][x] = (int)PixelAddress4(x & 0x7ff, 0, 0, 32) - (int)PixelAddress4(0, 0, 0, 32);
		rowOffset4[1][x] = (int)PixelAddress4(x & 0x7ff, 2, 0, 32) - (int)PixelAddress4(0, 2, 0, 32);
	}

	for (size_t i = 0; i < countof(m_psm); i++)
	{
		m_psm[i].info = GSLocalMemory::swizzle32;
		m_psm[i].rp = &GSLocalMemory::ReadPixel32;
		m_psm[i].rpa = &GSLocalMemory::ReadPixel32;
		m_psm[i].wp = &GSLocalMemory::WritePixel32;
		m_psm[i].wpa = &GSLocalMemory::WritePixel32;
		m_psm[i].rt = &GSLocalMemory::ReadTexel32;
		m_psm[i].rta = &GSLocalMemory::ReadTexel32;
		m_psm[i].wfa = &GSLocalMemory::WritePixel32;
		m_psm[i].wi = &GSLocalMemory::WriteImage<PSM_PSMCT32, 8, 8, 32>;
		m_psm[i].ri = &GSLocalMemory::ReadImageX; // TODO
		m_psm[i].rtx = &GSLocalMemory::ReadTexture32;
		m_psm[i].rtxP = &GSLocalMemory::ReadTexture32;
		m_psm[i].rtxb = &GSLocalMemory::ReadTextureBlock32;
		m_psm[i].rtxbP = &GSLocalMemory::ReadTextureBlock32;
		m_psm[i].bpp = m_psm[i].trbpp = 32;
		m_psm[i].pal = 0;
		m_psm[i].bs = GSVector2i(8, 8);
		m_psm[i].pgs = GSVector2i(64, 32);
		for (int j = 0; j < 8; j++)
			m_psm[i].rowOffset[j] = rowOffset32;
		m_psm[i].msk = 0xff;
		m_psm[i].depth = 0;
	}

	m_psm[PSM_PSGPU24].info = GSLocalMemory::swizzle16;
	m_psm[PSM_PSMCT16].info = GSLocalMemory::swizzle16;
	m_psm[PSM_PSMCT16S].info = GSLocalMemory::swizzle16S;
	m_psm[PSM_PSMT8].info = GSLocalMemory::swizzle8;
	m_psm[PSM_PSMT4].info = GSLocalMemory::swizzle4;
	m_psm[PSM_PSMZ32].info = GSLocalMemory::swizzle32Z;
	m_psm[PSM_PSMZ24].info = GSLocalMemory::swizzle32Z;
	m_psm[PSM_PSMZ16].info = GSLocalMemory::swizzle16Z;
	m_psm[PSM_PSMZ16S].info = GSLocalMemory::swizzle16SZ;

	m_psm[PSM_PSMCT24].rp = &GSLocalMemory::ReadPixel24;
	m_psm[PSM_PSMCT16].rp = &GSLocalMemory::ReadPixel16;
	m_psm[PSM_PSMCT16S].rp = &GSLocalMemory::ReadPixel16S;
	m_psm[PSM_PSMT8].rp = &GSLocalMemory::ReadPixel8;
	m_psm[PSM_PSMT4].rp = &GSLocalMemory::ReadPixel4;
	m_psm[PSM_PSMT8H].rp = &GSLocalMemory::ReadPixel8H;
	m_psm[PSM_PSMT4HL].rp = &GSLocalMemory::ReadPixel4HL;
	m_psm[PSM_PSMT4HH].rp = &GSLocalMemory::ReadPixel4HH;
	m_psm[PSM_PSMZ32].rp = &GSLocalMemory::ReadPixel32Z;
	m_psm[PSM_PSMZ24].rp = &GSLocalMemory::ReadPixel24Z;
	m_psm[PSM_PSMZ16].rp = &GSLocalMemory::ReadPixel16Z;
	m_psm[PSM_PSMZ16S].rp = &GSLocalMemory::ReadPixel16SZ;

	m_psm[PSM_PSMCT24].rpa = &GSLocalMemory::ReadPixel24;
	m_psm[PSM_PSMCT16].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSM_PSMCT16S].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSM_PSMT8].rpa = &GSLocalMemory::ReadPixel8;
	m_psm[PSM_PSMT4].rpa = &GSLocalMemory::ReadPixel4;
	m_psm[PSM_PSMT8H].rpa = &GSLocalMemory::ReadPixel8H;
	m_psm[PSM_PSMT4HL].rpa = &GSLocalMemory::ReadPixel4HL;
	m_psm[PSM_PSMT4HH].rpa = &GSLocalMemory::ReadPixel4HH;
	m_psm[PSM_PSMZ32].rpa = &GSLocalMemory::ReadPixel32;
	m_psm[PSM_PSMZ24].rpa = &GSLocalMemory::ReadPixel24;
	m_psm[PSM_PSMZ16].rpa = &GSLocalMemory::ReadPixel16;
	m_psm[PSM_PSMZ16S].rpa = &GSLocalMemory::ReadPixel16;

	m_psm[PSM_PSMCT32].wp = &GSLocalMemory::WritePixel32;
	m_psm[PSM_PSMCT24].wp = &GSLocalMemory::WritePixel24;
	m_psm[PSM_PSMCT16].wp = &GSLocalMemory::WritePixel16;
	m_psm[PSM_PSMCT16S].wp = &GSLocalMemory::WritePixel16S;
	m_psm[PSM_PSMT8].wp = &GSLocalMemory::WritePixel8;
	m_psm[PSM_PSMT4].wp = &GSLocalMemory::WritePixel4;
	m_psm[PSM_PSMT8H].wp = &GSLocalMemory::WritePixel8H;
	m_psm[PSM_PSMT4HL].wp = &GSLocalMemory::WritePixel4HL;
	m_psm[PSM_PSMT4HH].wp = &GSLocalMemory::WritePixel4HH;
	m_psm[PSM_PSMZ32].wp = &GSLocalMemory::WritePixel32Z;
	m_psm[PSM_PSMZ24].wp = &GSLocalMemory::WritePixel24Z;
	m_psm[PSM_PSMZ16].wp = &GSLocalMemory::WritePixel16Z;
	m_psm[PSM_PSMZ16S].wp = &GSLocalMemory::WritePixel16SZ;

	m_psm[PSM_PSMCT32].wpa = &GSLocalMemory::WritePixel32;
	m_psm[PSM_PSMCT24].wpa = &GSLocalMemory::WritePixel24;
	m_psm[PSM_PSMCT16].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSM_PSMCT16S].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSM_PSMT8].wpa = &GSLocalMemory::WritePixel8;
	m_psm[PSM_PSMT4].wpa = &GSLocalMemory::WritePixel4;
	m_psm[PSM_PSMT8H].wpa = &GSLocalMemory::WritePixel8H;
	m_psm[PSM_PSMT4HL].wpa = &GSLocalMemory::WritePixel4HL;
	m_psm[PSM_PSMT4HH].wpa = &GSLocalMemory::WritePixel4HH;
	m_psm[PSM_PSMZ32].wpa = &GSLocalMemory::WritePixel32;
	m_psm[PSM_PSMZ24].wpa = &GSLocalMemory::WritePixel24;
	m_psm[PSM_PSMZ16].wpa = &GSLocalMemory::WritePixel16;
	m_psm[PSM_PSMZ16S].wpa = &GSLocalMemory::WritePixel16;

	m_psm[PSM_PSMCT24].rt = &GSLocalMemory::ReadTexel24;
	m_psm[PSM_PSMCT16].rt = &GSLocalMemory::ReadTexel16;
	m_psm[PSM_PSMCT16S].rt = &GSLocalMemory::ReadTexel16S;
	m_psm[PSM_PSMT8].rt = &GSLocalMemory::ReadTexel8;
	m_psm[PSM_PSMT4].rt = &GSLocalMemory::ReadTexel4;
	m_psm[PSM_PSMT8H].rt = &GSLocalMemory::ReadTexel8H;
	m_psm[PSM_PSMT4HL].rt = &GSLocalMemory::ReadTexel4HL;
	m_psm[PSM_PSMT4HH].rt = &GSLocalMemory::ReadTexel4HH;
	m_psm[PSM_PSMZ32].rt = &GSLocalMemory::ReadTexel32Z;
	m_psm[PSM_PSMZ24].rt = &GSLocalMemory::ReadTexel24Z;
	m_psm[PSM_PSMZ16].rt = &GSLocalMemory::ReadTexel16Z;
	m_psm[PSM_PSMZ16S].rt = &GSLocalMemory::ReadTexel16SZ;

	m_psm[PSM_PSMCT24].rta = &GSLocalMemory::ReadTexel24;
	m_psm[PSM_PSMCT16].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSM_PSMCT16S].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSM_PSMT8].rta = &GSLocalMemory::ReadTexel8;
	m_psm[PSM_PSMT4].rta = &GSLocalMemory::ReadTexel4;
	m_psm[PSM_PSMT8H].rta = &GSLocalMemory::ReadTexel8H;
	m_psm[PSM_PSMT4HL].rta = &GSLocalMemory::ReadTexel4HL;
	m_psm[PSM_PSMT4HH].rta = &GSLocalMemory::ReadTexel4HH;
	m_psm[PSM_PSMZ24].rta = &GSLocalMemory::ReadTexel24;
	m_psm[PSM_PSMZ16].rta = &GSLocalMemory::ReadTexel16;
	m_psm[PSM_PSMZ16S].rta = &GSLocalMemory::ReadTexel16;

	m_psm[PSM_PSMCT24].wfa = &GSLocalMemory::WritePixel24;
	m_psm[PSM_PSMCT16].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSM_PSMCT16S].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSM_PSMZ24].wfa = &GSLocalMemory::WritePixel24;
	m_psm[PSM_PSMZ16].wfa = &GSLocalMemory::WriteFrame16;
	m_psm[PSM_PSMZ16S].wfa = &GSLocalMemory::WriteFrame16;

	m_psm[PSM_PSMCT24].wi = &GSLocalMemory::WriteImage24; // TODO
	m_psm[PSM_PSMCT16].wi = &GSLocalMemory::WriteImage<PSM_PSMCT16, 16, 8, 16>;
	m_psm[PSM_PSMCT16S].wi = &GSLocalMemory::WriteImage<PSM_PSMCT16S, 16, 8, 16>;
	m_psm[PSM_PSMT8].wi = &GSLocalMemory::WriteImage<PSM_PSMT8, 16, 16, 8>;
	m_psm[PSM_PSMT4].wi = &GSLocalMemory::WriteImage<PSM_PSMT4, 32, 16, 4>;
	m_psm[PSM_PSMT8H].wi = &GSLocalMemory::WriteImage8H; // TODO
	m_psm[PSM_PSMT4HL].wi = &GSLocalMemory::WriteImage4HL; // TODO
	m_psm[PSM_PSMT4HH].wi = &GSLocalMemory::WriteImage4HH; // TODO
	m_psm[PSM_PSMZ32].wi = &GSLocalMemory::WriteImage<PSM_PSMZ32, 8, 8, 32>;
	m_psm[PSM_PSMZ24].wi = &GSLocalMemory::WriteImage24Z; // TODO
	m_psm[PSM_PSMZ16].wi = &GSLocalMemory::WriteImage<PSM_PSMZ16, 16, 8, 16>;
	m_psm[PSM_PSMZ16S].wi = &GSLocalMemory::WriteImage<PSM_PSMZ16S, 16, 8, 16>;

	m_psm[PSM_PSMCT24].rtx = &GSLocalMemory::ReadTexture24;
	m_psm[PSM_PSGPU24].rtx = &GSLocalMemory::ReadTextureGPU24;
	m_psm[PSM_PSMCT16].rtx = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMCT16S].rtx = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMT8].rtx = &GSLocalMemory::ReadTexture8;
	m_psm[PSM_PSMT4].rtx = &GSLocalMemory::ReadTexture4;
	m_psm[PSM_PSMT8H].rtx = &GSLocalMemory::ReadTexture8H;
	m_psm[PSM_PSMT4HL].rtx = &GSLocalMemory::ReadTexture4HL;
	m_psm[PSM_PSMT4HH].rtx = &GSLocalMemory::ReadTexture4HH;
	m_psm[PSM_PSMZ32].rtx = &GSLocalMemory::ReadTexture32;
	m_psm[PSM_PSMZ24].rtx = &GSLocalMemory::ReadTexture24;
	m_psm[PSM_PSMZ16].rtx = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMZ16S].rtx = &GSLocalMemory::ReadTexture16;

	m_psm[PSM_PSMCT24].rtxP = &GSLocalMemory::ReadTexture24;
	m_psm[PSM_PSMCT16].rtxP = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMCT16S].rtxP = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMT8].rtxP = &GSLocalMemory::ReadTexture8P;
	m_psm[PSM_PSMT4].rtxP = &GSLocalMemory::ReadTexture4P;
	m_psm[PSM_PSMT8H].rtxP = &GSLocalMemory::ReadTexture8HP;
	m_psm[PSM_PSMT4HL].rtxP = &GSLocalMemory::ReadTexture4HLP;
	m_psm[PSM_PSMT4HH].rtxP = &GSLocalMemory::ReadTexture4HHP;
	m_psm[PSM_PSMZ32].rtxP = &GSLocalMemory::ReadTexture32;
	m_psm[PSM_PSMZ24].rtxP = &GSLocalMemory::ReadTexture24;
	m_psm[PSM_PSMZ16].rtxP = &GSLocalMemory::ReadTexture16;
	m_psm[PSM_PSMZ16S].rtxP = &GSLocalMemory::ReadTexture16;

	m_psm[PSM_PSMCT24].rtxb = &GSLocalMemory::ReadTextureBlock24;
	m_psm[PSM_PSMCT16].rtxb = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMCT16S].rtxb = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMT8].rtxb = &GSLocalMemory::ReadTextureBlock8;
	m_psm[PSM_PSMT4].rtxb = &GSLocalMemory::ReadTextureBlock4;
	m_psm[PSM_PSMT8H].rtxb = &GSLocalMemory::ReadTextureBlock8H;
	m_psm[PSM_PSMT4HL].rtxb = &GSLocalMemory::ReadTextureBlock4HL;
	m_psm[PSM_PSMT4HH].rtxb = &GSLocalMemory::ReadTextureBlock4HH;
	m_psm[PSM_PSMZ32].rtxb = &GSLocalMemory::ReadTextureBlock32;
	m_psm[PSM_PSMZ24].rtxb = &GSLocalMemory::ReadTextureBlock24;
	m_psm[PSM_PSMZ16].rtxb = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMZ16S].rtxb = &GSLocalMemory::ReadTextureBlock16;

	m_psm[PSM_PSMCT24].rtxbP = &GSLocalMemory::ReadTextureBlock24;
	m_psm[PSM_PSMCT16].rtxbP = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMCT16S].rtxbP = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMT8].rtxbP = &GSLocalMemory::ReadTextureBlock8P;
	m_psm[PSM_PSMT4].rtxbP = &GSLocalMemory::ReadTextureBlock4P;
	m_psm[PSM_PSMT8H].rtxbP = &GSLocalMemory::ReadTextureBlock8HP;
	m_psm[PSM_PSMT4HL].rtxbP = &GSLocalMemory::ReadTextureBlock4HLP;
	m_psm[PSM_PSMT4HH].rtxbP = &GSLocalMemory::ReadTextureBlock4HHP;
	m_psm[PSM_PSMZ32].rtxbP = &GSLocalMemory::ReadTextureBlock32;
	m_psm[PSM_PSMZ24].rtxbP = &GSLocalMemory::ReadTextureBlock24;
	m_psm[PSM_PSMZ16].rtxbP = &GSLocalMemory::ReadTextureBlock16;
	m_psm[PSM_PSMZ16S].rtxbP = &GSLocalMemory::ReadTextureBlock16;

	m_psm[PSM_PSGPU24].bpp = 16;
	m_psm[PSM_PSMCT16].bpp = m_psm[PSM_PSMCT16S].bpp = 16;
	m_psm[PSM_PSMT8].bpp = 8;
	m_psm[PSM_PSMT4].bpp = 4;
	m_psm[PSM_PSMZ16].bpp = m_psm[PSM_PSMZ16S].bpp = 16;

	m_psm[PSM_PSMCT24].trbpp = 24;
	m_psm[PSM_PSGPU24].trbpp = 16;
	m_psm[PSM_PSMCT16].trbpp = m_psm[PSM_PSMCT16S].trbpp = 16;
	m_psm[PSM_PSMT8].trbpp = m_psm[PSM_PSMT8H].trbpp = 8;
	m_psm[PSM_PSMT4].trbpp = m_psm[PSM_PSMT4HL].trbpp = m_psm[PSM_PSMT4HH].trbpp = 4;
	m_psm[PSM_PSMZ24].trbpp = 24;
	m_psm[PSM_PSMZ16].trbpp = m_psm[PSM_PSMZ16S].trbpp = 16;

	m_psm[PSM_PSMT8].pal = m_psm[PSM_PSMT8H].pal = 256;
	m_psm[PSM_PSMT4].pal = m_psm[PSM_PSMT4HL].pal = m_psm[PSM_PSMT4HH].pal = 16;

	for (size_t i = 0; i < countof(m_psm); i++)
		m_psm[i].fmt = 3;
	m_psm[PSM_PSMCT32].fmt = m_psm[PSM_PSMZ32].fmt = 0;
	m_psm[PSM_PSMCT24].fmt = m_psm[PSM_PSMZ24].fmt = 1;
	m_psm[PSM_PSMCT16].fmt = m_psm[PSM_PSMZ16].fmt = 2;
	m_psm[PSM_PSMCT16S].fmt = m_psm[PSM_PSMZ16S].fmt = 2;


	m_psm[PSM_PSGPU24].bs = GSVector2i(16, 8);
	m_psm[PSM_PSMCT16].bs = m_psm[PSM_PSMCT16S].bs = GSVector2i(16, 8);
	m_psm[PSM_PSMT8].bs = GSVector2i(16, 16);
	m_psm[PSM_PSMT4].bs = GSVector2i(32, 16);
	m_psm[PSM_PSMZ16].bs = m_psm[PSM_PSMZ16S].bs = GSVector2i(16, 8);

	m_psm[PSM_PSGPU24].pgs = GSVector2i(64, 64);
	m_psm[PSM_PSMCT16].pgs = m_psm[PSM_PSMCT16S].pgs = GSVector2i(64, 64);
	m_psm[PSM_PSMT8].pgs = GSVector2i(128, 64);
	m_psm[PSM_PSMT4].pgs = GSVector2i(128, 128);
	m_psm[PSM_PSMZ16].pgs = m_psm[PSM_PSMZ16S].pgs = GSVector2i(64, 64);

	for(int i = 0; i < 8; i++) m_psm[PSM_PSGPU24].rowOffset[i] = rowOffset16;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMCT16].rowOffset[i] = rowOffset16;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMCT16S].rowOffset[i] = rowOffset16S;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMT8].rowOffset[i] = rowOffset8[((i + 2) >> 2) & 1];
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMT4].rowOffset[i] = rowOffset4[((i + 2) >> 2) & 1];
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMZ32].rowOffset[i] = rowOffset32Z;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMZ24].rowOffset[i] = rowOffset32Z;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMZ16].rowOffset[i] = rowOffset16Z;
	for(int i = 0; i < 8; i++) m_psm[PSM_PSMZ16S].rowOffset[i] = rowOffset16SZ;

	m_psm[PSM_PSMCT24].msk = 0x3f;
	m_psm[PSM_PSMZ24].msk = 0x3f;
	m_psm[PSM_PSMT8H].msk = 0xc0;
	m_psm[PSM_PSMT4HL].msk = 0x40;
	m_psm[PSM_PSMT4HH].msk = 0x80;

	m_psm[PSM_PSMZ32].depth  = 1;
	m_psm[PSM_PSMZ24].depth  = 1;
	m_psm[PSM_PSMZ16].depth  = 1;
	m_psm[PSM_PSMZ16S].depth = 1;
}

GSLocalMemory::~GSLocalMemory()
{
	if (m_use_fifo_alloc)
		fifo_free(m_vm8, m_vmsize, 4);
	else
		vmfree(m_vm8, m_vmsize * 4);

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
	uint32 fbp = FRAME.Block();
	uint32 zbp = ZBUF.Block();
	uint32 fpsm = FRAME.PSM;
	uint32 zpsm = ZBUF.PSM;
	uint32 bw = FRAME.FBW;

	ASSERT(m_psm[fpsm].trbpp > 8 || m_psm[zpsm].trbpp > 8);

	// "(psm & 0x0f) ^ ((psm & 0xf0) >> 2)" creates 4 bit unique identifiers for render target formats (only)

	uint32 fpsm_hash = (fpsm & 0x0f) ^ ((fpsm & 0x30) >> 2);
	uint32 zpsm_hash = (zpsm & 0x0f) ^ ((zpsm & 0x30) >> 2);

	uint32 hash = (FRAME.FBP << 0) | (ZBUF.ZBP << 9) | (bw << 18) | (fpsm_hash << 24) | (zpsm_hash << 28);

	auto it = m_pomap.find(hash);

	if (it != m_pomap.end())
	{
		return it->second;
	}

	GSPixelOffset* off = (GSPixelOffset*)_aligned_malloc(sizeof(GSPixelOffset), 32);

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
		off->col[i].x = m_psm[fpsm].rowOffset[0][i] << fs;
		off->col[i].y = m_psm[zpsm].rowOffset[0][i] << zs;
	}

	m_pomap[hash] = off;

	return off;
}

GSPixelOffset4* GSLocalMemory::GetPixelOffset4(const GIFRegFRAME& FRAME, const GIFRegZBUF& ZBUF)
{
	uint32 fbp = FRAME.Block();
	uint32 zbp = ZBUF.Block();
	uint32 fpsm = FRAME.PSM;
	uint32 zpsm = ZBUF.PSM;
	uint32 bw = FRAME.FBW;

	ASSERT(m_psm[fpsm].trbpp > 8 || m_psm[zpsm].trbpp > 8);

	// "(psm & 0x0f) ^ ((psm & 0xf0) >> 2)" creates 4 bit unique identifiers for render target formats (only)

	uint32 fpsm_hash = (fpsm & 0x0f) ^ ((fpsm & 0x30) >> 2);
	uint32 zpsm_hash = (zpsm & 0x0f) ^ ((zpsm & 0x30) >> 2);

	uint32 hash = (FRAME.FBP << 0) | (ZBUF.ZBP << 9) | (bw << 18) | (fpsm_hash << 24) | (zpsm_hash << 28);

	auto it = m_po4map.find(hash);

	if (it != m_po4map.end())
	{
		return it->second;
	}

	GSPixelOffset4* off = (GSPixelOffset4*)_aligned_malloc(sizeof(GSPixelOffset4), 32);

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
		off->col[i].x = m_psm[fpsm].rowOffset[0][i * 4] << fs;
		off->col[i].y = m_psm[zpsm].rowOffset[0][i * 4] << zs;
	}

	m_po4map[hash] = off;

	return off;
}

static bool cmp_vec2x(const GSVector2i& a, const GSVector2i& b) { return a.x < b.x; }

std::vector<GSVector2i>* GSLocalMemory::GetPage2TileMap(const GIFRegTEX0& TEX0)
{
	uint64 hash = TEX0.u64 & 0x3ffffffffull; // TBP0 TBW PSM TW TH

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

	std::unordered_map<uint32, std::unordered_set<uint32>> tmp; // key = page, value = y:x, 7 bits each, max 128x128 tiles for the worst case (1024x1024 32bpp 8x8 blocks)

	for (; bn.blkY() < (th >> off.blockShiftY()); bn.nextBlockY())
	{
		for (; bn.blkX() < (tw >> off.blockShiftX()); bn.nextBlockX())
		{
			uint32 page = (bn.value() >> 5) % MAX_PAGES;

			tmp[page].insert((bn.blkY() << 7) + bn.blkX());
		}
	}

	// combine the lower 5 bits of the address into a 9:5 pointer:mask form, so the "valid bits" can be tested against an uint32 array

	auto p2t = new std::vector<GSVector2i>[MAX_PAGES];

	for (const auto& i : tmp)
	{
		uint32 page = i.first;

		auto& tiles = i.second;

		std::unordered_map<uint32, uint32> m;

		for (const auto addr : tiles)
		{
			uint32 row = addr >> 5;
			uint32 col = 1 << (addr & 31);

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

		std::sort(p2t[page].begin(), p2t[page].end(), cmp_vec2x);
	}

	m_p2tmap[hash] = p2t;

	return p2t;
}

////////////////////

template <int psm, int bsx, int bsy, int alignment>
void GSLocalMemory::WriteImageColumn(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	const int csy = bsy / 4;

	for (int offset = srcpitch * csy; h >= csy; h -= csy, y += csy, src += offset)
	{
		for (int x = l; x < r; x += bsx)
		{
			switch (psm)
			{
				case PSM_PSMCT32: GSBlock::WriteColumn32<alignment, 0xffffffff>(y, BlockPtr32(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSM_PSMCT16: GSBlock::WriteColumn16<alignment>(y, BlockPtr16(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMCT16S: GSBlock::WriteColumn16<alignment>(y, BlockPtr16S(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMT8: GSBlock::WriteColumn8<alignment>(y, BlockPtr8(x, y, bp, bw), &src[x], srcpitch); break;
				case PSM_PSMT4: GSBlock::WriteColumn4<alignment>(y, BlockPtr4(x, y, bp, bw), &src[x >> 1], srcpitch); break;
				case PSM_PSMZ32: GSBlock::WriteColumn32<alignment, 0xffffffff>(y, BlockPtr32Z(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSM_PSMZ16: GSBlock::WriteColumn16<alignment>(y, BlockPtr16Z(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMZ16S: GSBlock::WriteColumn16<alignment>(y, BlockPtr16SZ(x, y, bp, bw), &src[x * 2], srcpitch); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy, int alignment>
void GSLocalMemory::WriteImageBlock(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	for (int offset = srcpitch * bsy; h >= bsy; h -= bsy, y += bsy, src += offset)
	{
		for (int x = l; x < r; x += bsx)
		{
			switch (psm)
			{
				case PSM_PSMCT32: GSBlock::WriteBlock32<alignment, 0xffffffff>(BlockPtr32(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSM_PSMCT16: GSBlock::WriteBlock16<alignment>(BlockPtr16(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMCT16S: GSBlock::WriteBlock16<alignment>(BlockPtr16S(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMT8: GSBlock::WriteBlock8<alignment>(BlockPtr8(x, y, bp, bw), &src[x], srcpitch); break;
				case PSM_PSMT4: GSBlock::WriteBlock4<alignment>(BlockPtr4(x, y, bp, bw), &src[x >> 1], srcpitch); break;
				case PSM_PSMZ32: GSBlock::WriteBlock32<alignment, 0xffffffff>(BlockPtr32Z(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSM_PSMZ16: GSBlock::WriteBlock16<alignment>(BlockPtr16Z(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSM_PSMZ16S: GSBlock::WriteBlock16<alignment>(BlockPtr16SZ(x, y, bp, bw), &src[x * 2], srcpitch); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy>
void GSLocalMemory::WriteImageLeftRight(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	for (; h > 0; y++, h--, src += srcpitch)
	{
		for (int x = l; x < r; x++)
		{
			switch (psm)
			{
				case PSM_PSMCT32: WritePixel32(x, y, *(uint32*)&src[x * 4], bp, bw); break;
				case PSM_PSMCT16: WritePixel16(x, y, *(uint16*)&src[x * 2], bp, bw); break;
				case PSM_PSMCT16S: WritePixel16S(x, y, *(uint16*)&src[x * 2], bp, bw); break;
				case PSM_PSMT8: WritePixel8(x, y, src[x], bp, bw); break;
				case PSM_PSMT4: WritePixel4(x, y, src[x >> 1] >> ((x & 1) << 2), bp, bw); break;
				case PSM_PSMZ32: WritePixel32Z(x, y, *(uint32*)&src[x * 4], bp, bw); break;
				case PSM_PSMZ16: WritePixel16Z(x, y, *(uint16*)&src[x * 2], bp, bw); break;
				case PSM_PSMZ16S: WritePixel16SZ(x, y, *(uint16*)&src[x * 2], bp, bw); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy, int trbpp>
void GSLocalMemory::WriteImageTopBottom(int l, int r, int y, int h, const uint8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	alignas(32) uint8 buff[64]; // merge buffer for one column

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	const int csy = bsy / 4;

	// merge incomplete column

	int y2 = y & (csy - 1);

	if (y2 > 0)
	{
		int h2 = std::min(h, csy - y2);

		for (int x = l; x < r; x += bsx)
		{
			uint8* dst = NULL;

			switch (psm)
			{
				case PSM_PSMCT32: dst = BlockPtr32(x, y, bp, bw); break;
				case PSM_PSMCT16: dst = BlockPtr16(x, y, bp, bw); break;
				case PSM_PSMCT16S: dst = BlockPtr16S(x, y, bp, bw); break;
				case PSM_PSMT8: dst = BlockPtr8(x, y, bp, bw); break;
				case PSM_PSMT4: dst = BlockPtr4(x, y, bp, bw); break;
				case PSM_PSMZ32: dst = BlockPtr32Z(x, y, bp, bw); break;
				case PSM_PSMZ16: dst = BlockPtr16Z(x, y, bp, bw); break;
				case PSM_PSMZ16S: dst = BlockPtr16SZ(x, y, bp, bw); break;
				// TODO
				default: __assume(0);
			}

			switch (psm)
			{
				case PSM_PSMCT32:
				case PSM_PSMZ32:
					GSBlock::ReadColumn32(y, dst, buff, 32);
					memcpy(&buff[32], &src[x * 4], 32);
					GSBlock::WriteColumn32<32, 0xffffffff>(y, dst, buff, 32);
					break;
				case PSM_PSMCT16:
				case PSM_PSMCT16S:
				case PSM_PSMZ16:
				case PSM_PSMZ16S:
					GSBlock::ReadColumn16(y, dst, buff, 32);
					memcpy(&buff[32], &src[x * 2], 32);
					GSBlock::WriteColumn16<32>(y, dst, buff, 32);
					break;
				case PSM_PSMT8:
					GSBlock::ReadColumn8(y, dst, buff, 16);
					for (int i = 0, j = y2; i < h2; i++, j++)
						memcpy(&buff[j * 16], &src[i * srcpitch + x], 16);
					GSBlock::WriteColumn8<32>(y, dst, buff, 16);
					break;
				case PSM_PSMT4:
					GSBlock::ReadColumn4(y, dst, buff, 16);
					for (int i = 0, j = y2; i < h2; i++, j++)
						memcpy(&buff[j * 16], &src[i * srcpitch + (x >> 1)], 16);
					GSBlock::WriteColumn4<32>(y, dst, buff, 16);
					break;
				// TODO
				default:
					__assume(0);
			}
		}

		src += srcpitch * h2;
		y += h2;
		h -= h2;
	}

	// write whole columns

	{
		int h2 = h & ~(csy - 1);

		if (h2 > 0)
		{
			size_t addr = (size_t)&src[l * trbpp >> 3];

			if ((addr & 31) == 0 && (srcpitch & 31) == 0)
			{
				WriteImageColumn<psm, bsx, bsy, 32>(l, r, y, h2, src, srcpitch, BITBLTBUF);
			}
			else if ((addr & 15) == 0 && (srcpitch & 15) == 0)
			{
				WriteImageColumn<psm, bsx, bsy, 16>(l, r, y, h2, src, srcpitch, BITBLTBUF);
			}
			else
			{
				WriteImageColumn<psm, bsx, bsy, 0>(l, r, y, h2, src, srcpitch, BITBLTBUF);
			}

			src += srcpitch * h2;
			y += h2;
			h -= h2;
		}
	}

	// merge incomplete column

	if (h >= 1)
	{
		for (int x = l; x < r; x += bsx)
		{
			uint8* dst = NULL;

			switch (psm)
			{
			case PSM_PSMCT32: dst = BlockPtr32(x, y, bp, bw); break;
			case PSM_PSMCT16: dst = BlockPtr16(x, y, bp, bw); break;
			case PSM_PSMCT16S: dst = BlockPtr16S(x, y, bp, bw); break;
			case PSM_PSMT8: dst = BlockPtr8(x, y, bp, bw); break;
			case PSM_PSMT4: dst = BlockPtr4(x, y, bp, bw); break;
			case PSM_PSMZ32: dst = BlockPtr32Z(x, y, bp, bw); break;
			case PSM_PSMZ16: dst = BlockPtr16Z(x, y, bp, bw); break;
			case PSM_PSMZ16S: dst = BlockPtr16SZ(x, y, bp, bw); break;
			// TODO
			default: __assume(0);
			}

			switch (psm)
			{
				case PSM_PSMCT32:
				case PSM_PSMZ32:
					GSBlock::ReadColumn32(y, dst, buff, 32);
					memcpy(&buff[0], &src[x * 4], 32);
					GSBlock::WriteColumn32<32, 0xffffffff>(y, dst, buff, 32);
					break;
				case PSM_PSMCT16:
				case PSM_PSMCT16S:
				case PSM_PSMZ16:
				case PSM_PSMZ16S:
					GSBlock::ReadColumn16(y, dst, buff, 32);
					memcpy(&buff[0], &src[x * 2], 32);
					GSBlock::WriteColumn16<32>(y, dst, buff, 32);
					break;
				case PSM_PSMT8:
					GSBlock::ReadColumn8(y, dst, buff, 16);
					for (int i = 0; i < h; i++)
						memcpy(&buff[i * 16], &src[i * srcpitch + x], 16);
					GSBlock::WriteColumn8<32>(y, dst, buff, 16);
					break;
				case PSM_PSMT4:
					GSBlock::ReadColumn4(y, dst, buff, 16);
					for (int i = 0; i < h; i++)
						memcpy(&buff[i * 16], &src[i * srcpitch + (x >> 1)], 16);
					GSBlock::WriteColumn4<32>(y, dst, buff, 16);
					break;
				// TODO
				default:
					__assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy, int trbpp>
void GSLocalMemory::WriteImage(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	int l = (int)TRXPOS.DSAX;
	int r = l + (int)TRXREG.RRW;

	// finish the incomplete row first

	if (tx != l)
	{
		int n = std::min(len, (r - tx) * trbpp >> 3);
		WriteImageX(tx, ty, src, n, BITBLTBUF, TRXPOS, TRXREG);
		src += n;
		len -= n;
	}

	int la = (l + (bsx - 1)) & ~(bsx - 1);
	int ra = r & ~(bsx - 1);
	int srcpitch = (r - l) * trbpp >> 3;
	int h = len / srcpitch;

	if (ra - la >= bsx && h > 0) // "transfer width" >= "block width" && there is at least one full row
	{
		const uint8* s = &src[-l * trbpp >> 3];

		src += srcpitch * h;
		len -= srcpitch * h;

		// left part

		if (l < la)
		{
			WriteImageLeftRight<psm, bsx, bsy>(l, la, ty, h, s, srcpitch, BITBLTBUF);
		}

		// right part

		if (ra < r)
		{
			WriteImageLeftRight<psm, bsx, bsy>(ra, r, ty, h, s, srcpitch, BITBLTBUF);
		}

		// horizontally aligned part

		if (la < ra)
		{
			// top part

			{
				int h2 = std::min(h, bsy - (ty & (bsy - 1)));

				if (h2 < bsy)
				{
					WriteImageTopBottom<psm, bsx, bsy, trbpp>(la, ra, ty, h2, s, srcpitch, BITBLTBUF);

					s += srcpitch * h2;
					ty += h2;
					h -= h2;
				}
			}

			// horizontally and vertically aligned part

			{
				int h2 = h & ~(bsy - 1);

				if (h2 > 0)
				{
					size_t addr = (size_t)&s[la * trbpp >> 3];

					if ((addr & 31) == 0 && (srcpitch & 31) == 0)
					{
						WriteImageBlock<psm, bsx, bsy, 32>(la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}
					else if ((addr & 15) == 0 && (srcpitch & 15) == 0)
					{
						WriteImageBlock<psm, bsx, bsy, 16>(la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}
					else
					{
						WriteImageBlock<psm, bsx, bsy, 0>(la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}

					s += srcpitch * h2;
					ty += h2;
					h -= h2;
				}
			}

			// bottom part

			if (h > 0)
			{
				WriteImageTopBottom<psm, bsx, bsy, trbpp>(la, ra, ty, h, s, srcpitch, BITBLTBUF);

				// s += srcpitch * h;
				ty += h;
				// h -= h;
			}
		}
	}

	// the rest

	if (len > 0)
	{
		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
}


static bool IsTopLeftAligned(int dsax, int tx, int ty, int bw, int bh)
{
	return ((dsax & (bw - 1)) == 0 && (tx & (bw - 1)) == 0 && dsax == tx && (ty & (bh - 1)) == 0);
}

void GSLocalMemory::WriteImage24(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW * 3;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock24(src + (x - tx) * 3, srcpitch, BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemory::WriteImage8H(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock8H(src + (x - tx), srcpitch, BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemory::WriteImage4HL(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW / 2;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock4HL(src + (x - tx) / 2, srcpitch, BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemory::WriteImage4HH(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW / 2;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock4HH(src + (x - tx) / 2, srcpitch, BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemory::WriteImage24Z(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW * 3;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock24(src + (x - tx) * 3, srcpitch, BlockPtr32Z(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemory::WriteImageX(int& tx, int& ty, const uint8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (len <= 0)
		return;

	const uint8* pb = (uint8*)src;
	const uint16* pw = (uint16*)src;
	const uint32* pd = (uint32*)src;

	uint32 bp = BITBLTBUF.DBP;
	uint32 bw = BITBLTBUF.DBW;
	psm_t* psm = &m_psm[BITBLTBUF.DPSM];

	int x = tx;
	int y = ty;
	int sx = (int)TRXPOS.DSAX;
	int ex = sx + (int)TRXREG.RRW;

	GSOffset off = GetOffset(bp, bw, BITBLTBUF.DPSM);

	switch (BITBLTBUF.DPSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMZ32:

			len /= 4;

			while (len > 0)
			{
				uint32 addr = off.assertSizesMatch(swizzle32).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x++, pd++)
				{
					WritePixel32(addr + offset[x], *pd);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMCT24:
		case PSM_PSMZ24:

			len /= 3;

			while (len > 0)
			{
				uint32 addr = off.assertSizesMatch(swizzle32).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x++, pb += 3)
				{
					WritePixel24(addr + offset[x], *(uint32*)pb);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMCT16:
		case PSM_PSMCT16S:
		case PSM_PSMZ16:
		case PSM_PSMZ16S:

			len /= 2;

			while (len > 0)
			{
				uint32 addr = off.assertSizesMatch(swizzle16).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x++, pw++)
				{
					WritePixel16(addr + offset[x], *pw);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT8:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT8).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x++, pb++)
				{
					WritePixel8(addr + offset[x], *pb);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					WritePixel4(addr + offset[x + 0], *pb & 0xf);
					WritePixel4(addr + offset[x + 1], *pb >> 4);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT8H:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT8H).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x++, pb++)
				{
					WritePixel8H(addr + offset[x], *pb);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4HL:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4HL).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					WritePixel4HL(addr + offset[x + 0], *pb & 0xf);
					WritePixel4HL(addr + offset[x + 1], *pb >> 4);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4HH:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4HH).pa(0, y);
				int* offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					WritePixel4HH(addr + offset[x + 0], *pb & 0xf);
					WritePixel4HH(addr + offset[x + 1], *pb >> 4);
				}

				if (x >= ex)
				{
					x = sx;
					y++;
				}
			}

			break;
	}

	tx = x;
	ty = y;
}

//

void GSLocalMemory::ReadImageX(int& tx, int& ty, uint8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG) const
{
	if (len <= 0)
		return;

	uint8* RESTRICT pb = (uint8*)dst;
	uint16* RESTRICT pw = (uint16*)dst;
	uint32* RESTRICT pd = (uint32*)dst;

	uint32 bp = BITBLTBUF.SBP;
	uint32 bw = BITBLTBUF.SBW;
	psm_t* RESTRICT psm = &m_psm[BITBLTBUF.SPSM];

	int x = tx;
	int y = ty;
	int sx = (int)TRXPOS.SSAX;
	int ex = sx + (int)TRXREG.RRW;

	GSOffset off = GetOffset(bp, bw, BITBLTBUF.SPSM);

	// printf("spsm=%d x=%d ex=%d y=%d len=%d\n", BITBLTBUF.SPSM, x, ex, y, len);

	switch (BITBLTBUF.SPSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMZ32:

			// MGS1 intro, fade effect between two scenes (airplane outside-inside transition)

			len /= 4;

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint32* RESTRICT ps = &m_vm32[off.assertSizesMatch(swizzle32).pa(0, y)];

				for (; len > 0 && x < ex && (x & 7); len--, x++, pd++)
				{
					*pd = ps[offset[x]];
				}

				// aligned to a column

				for (int ex8 = ex - 8; len >= 8 && x <= ex8; len -= 8, x += 8, pd += 8)
				{
					int off = offset[x];

					GSVector4i::store<false>(&pd[0], GSVector4i::load(&ps[off + 0], &ps[off + 4]));
					GSVector4i::store<false>(&pd[4], GSVector4i::load(&ps[off + 8], &ps[off + 12]));

					for (int i = 0; i < 8; i++)
						ASSERT(pd[i] == ps[offset[x + i]]);
				}

				for (; len > 0 && x < ex; len--, x++, pd++)
				{
					*pd = ps[offset[x]];
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMCT24:
		case PSM_PSMZ24:

			len /= 3;

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint32* RESTRICT ps = &m_vm32[off.assertSizesMatch(swizzle32).pa(0, y)];

				for (; len > 0 && x < ex; len--, x++, pb += 3)
				{
					uint32 c = ps[offset[x]];

					pb[0] = (uint8)(c);
					pb[1] = (uint8)(c >> 8);
					pb[2] = (uint8)(c >> 16);
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMCT16:
		case PSM_PSMCT16S:
		case PSM_PSMZ16:
		case PSM_PSMZ16S:

			len /= 2;

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint16* RESTRICT ps = &m_vm16[off.assertSizesMatch(swizzle16).pa(0, y)];

				for (int ex4 = ex - 4; len >= 4 && x <= ex4; len -= 4, x += 4, pw += 4)
				{
					pw[0] = ps[offset[x + 0]];
					pw[1] = ps[offset[x + 1]];
					pw[2] = ps[offset[x + 2]];
					pw[3] = ps[offset[x + 3]];
				}

				for (; len > 0 && x < ex; len--, x++, pw++)
				{
					*pw = ps[offset[x]];
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT8:

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint8* RESTRICT ps = &m_vm8[GSOffset::fromKnownPSM(bp, bw, PSM_PSMT8).pa(0, y)];

				for (int ex4 = ex - 4; len >= 4 && x <= ex4; len -= 4, x += 4, pb += 4)
				{
					pb[0] = ps[offset[x + 0]];
					pb[1] = ps[offset[x + 1]];
					pb[2] = ps[offset[x + 2]];
					pb[3] = ps[offset[x + 3]];
				}

				for (; len > 0 && x < ex; len--, x++, pb++)
				{
					*pb = ps[offset[x]];
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4:

			while (len > 0)
			{
				uint32 addr = GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4).pa(0, y);
				int* RESTRICT offset = psm->rowOffset[y & 7];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					*pb = (uint8)(ReadPixel4(addr + offset[x + 0]) | (ReadPixel4(addr + offset[x + 1]) << 4));
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT8H:

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint32* RESTRICT ps = &m_vm32[GSOffset::fromKnownPSM(bp, bw, PSM_PSMT8H).pa(0, y)];

				for (int ex4 = ex - 4; len >= 4 && x <= ex4; len -= 4, x += 4, pb += 4)
				{
					pb[0] = (uint8)(ps[offset[x + 0]] >> 24);
					pb[1] = (uint8)(ps[offset[x + 1]] >> 24);
					pb[2] = (uint8)(ps[offset[x + 2]] >> 24);
					pb[3] = (uint8)(ps[offset[x + 3]] >> 24);
				}

				for (; len > 0 && x < ex; len--, x++, pb++)
				{
					*pb = (uint8)(ps[offset[x]] >> 24);
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4HL:

			while (len > 0)
			{
				int* offset = psm->rowOffset[y & 7];
				uint32* RESTRICT ps = &m_vm32[GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4HL).pa(0, y)];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					uint32 c0 = (ps[offset[x + 0]] >> 24) & 0x0f;
					uint32 c1 = (ps[offset[x + 1]] >> 20) & 0xf0;

					*pb = (uint8)(c0 | c1);
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;

		case PSM_PSMT4HH:

			while (len > 0)
			{
				int* RESTRICT offset = psm->rowOffset[y & 7];
				uint32* RESTRICT ps = &m_vm32[GSOffset::fromKnownPSM(bp, bw, PSM_PSMT4HH).pa(0, y)];

				for (; len > 0 && x < ex; len--, x += 2, pb++)
				{
					uint32 c0 = (ps[offset[x + 0]] >> 28) & 0x0f;
					uint32 c1 = (ps[offset[x + 1]] >> 24) & 0xf0;

					*pb = (uint8)(c0 | c1);
				}

				if (x == ex)
				{
					x = sx;
					y++;
				}
			}

			break;
	}

	tx = x;
	ty = y;
}

///////////////////

void GSLocalMemory::ReadTexture32(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock32(src, read_dst, dstpitch);
	});
}

void GSLocalMemory::ReadTexture24(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	if (TEXA.AEM)
	{
		foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
		{
			GSBlock::ReadAndExpandBlock24<true>(src, read_dst, dstpitch, TEXA);
		});
	}
	else
	{
		foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
		{
			GSBlock::ReadAndExpandBlock24<false>(src, read_dst, dstpitch, TEXA);
		});
	}
}

void GSLocalMemory::ReadTextureGPU24(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle16), this, r, dst, dstpitch, 16, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock16(src, read_dst, dstpitch);
	});

	// Convert packed RGB scanline to 32 bits RGBA
	ASSERT(dstpitch >= r.width() * 4);
	for (int y = r.top; y < r.bottom; y++)
	{
		uint8* line = dst + y * dstpitch;

		for (int x = r.right; x >= r.left; x--)
		{
			*(uint32*)&line[x * 4] = *(uint32*)&line[x * 3] & 0xFFFFFF;
		}
	}
}

void GSLocalMemory::ReadTexture16(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	if (TEXA.AEM)
	{
		foreachBlock(off.assertSizesMatch(swizzle16), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
		{
			GSBlock::ReadAndExpandBlock16<true>(src, read_dst, dstpitch, TEXA);
		});
	}
	else
	{
		foreachBlock(off.assertSizesMatch(swizzle16), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
		{
			GSBlock::ReadAndExpandBlock16<false>(src, read_dst, dstpitch, TEXA);
		});
	}
}

void GSLocalMemory::ReadTexture8(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const uint32* pal = m_clut;

	foreachBlock(off.assertSizesMatch(swizzle8), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadAndExpandBlock8_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemory::ReadTexture4(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const uint64* pal = m_clut;

	foreachBlock(off.assertSizesMatch(swizzle4), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadAndExpandBlock4_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemory::ReadTexture8H(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const uint32* pal = m_clut;

	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadAndExpandBlock8H_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemory::ReadTexture4HL(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const uint32* pal = m_clut;

	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadAndExpandBlock4HL_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemory::ReadTexture4HH(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const uint32* pal = m_clut;

	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 32, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadAndExpandBlock4HH_32(src, read_dst, dstpitch, pal);
	});
}

///////////////////

void GSLocalMemory::ReadTextureBlock32(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock32(BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemory::ReadTextureBlock24(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	if (TEXA.AEM)
	{
		GSBlock::ReadAndExpandBlock24<true>(BlockPtr(bp), dst, dstpitch, TEXA);
	}
	else
	{
		GSBlock::ReadAndExpandBlock24<false>(BlockPtr(bp), dst, dstpitch, TEXA);
	}
}

void GSLocalMemory::ReadTextureBlock16(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	if (TEXA.AEM)
	{
		GSBlock::ReadAndExpandBlock16<true>(BlockPtr(bp), dst, dstpitch, TEXA);
	}
	else
	{
		GSBlock::ReadAndExpandBlock16<false>(BlockPtr(bp), dst, dstpitch, TEXA);
	}
}

void GSLocalMemory::ReadTextureBlock8(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8_32(BlockPtr(bp), dst, dstpitch, m_clut);
}

void GSLocalMemory::ReadTextureBlock4(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4_32(BlockPtr(bp), dst, dstpitch, m_clut);
}

void GSLocalMemory::ReadTextureBlock8H(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8H_32(BlockPtr(bp), dst, dstpitch, m_clut);
}

void GSLocalMemory::ReadTextureBlock4HL(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4HL_32(BlockPtr(bp), dst, dstpitch, m_clut);
}

void GSLocalMemory::ReadTextureBlock4HH(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4HH_32(BlockPtr(bp), dst, dstpitch, m_clut);
}

///////////////////

void GSLocalMemory::ReadTexture(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
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

		bool aligned = ((size_t)(dst + (cr.left - r.left) * sizeof(uint32)) & 0xf) == 0;

		if (cr.rempty() || !aligned)
		{
			// TODO: expand r to block size, read into temp buffer

			if (!aligned)
				printf("unaligned memory pointer passed to ReadTexture\n");

			for (int y = r.top; y < r.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((uint32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}
		}
		else
		{
			for (int y = r.top; y < cr.top; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((uint32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			for (int y = cr.bottom; y < r.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < r.right; x++, i++)
				{
					((uint32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			for (int y = cr.top; y < cr.bottom; y++, dst += dstpitch)
			{
				for (int x = r.left, i = 0; x < cr.left; x++, i++)
				{
					((uint32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}

				for (int x = cr.right, i = x - r.left; x < r.right; x++, i++)
				{
					((uint32*)dst)[i] = (this->*rt)(x, y, TEX0, TEXA);
				}
			}

			if (!cr.rempty())
			{
				(this->*rtx)(off, cr, dst + (cr.left - r.left) * sizeof(uint32), dstpitch, TEXA);
			}
		}
	}
	else
	{
		(this->*rtx)(off, r, dst, dstpitch, TEXA);
	}
}

// 32/8

void GSLocalMemory::ReadTexture8P(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle8), this, r, dst, dstpitch, 8, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock8(src, read_dst, dstpitch);
	});
}

void GSLocalMemory::ReadTexture4P(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle4), this, r, dst, dstpitch, 8, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock4P(src, read_dst, dstpitch);
	});
}

void GSLocalMemory::ReadTexture8HP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 8, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock8HP(src, read_dst, dstpitch);
	});
}

void GSLocalMemory::ReadTexture4HLP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 8, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock4HLP(src, read_dst, dstpitch);
	});
}

void GSLocalMemory::ReadTexture4HHP(const GSOffset& off, const GSVector4i& r, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(swizzle32), this, r, dst, dstpitch, 8, [&](uint8* read_dst, const uint8* src)
	{
		GSBlock::ReadBlock4HHP(src, read_dst, dstpitch);
	});
}

//

void GSLocalMemory::ReadTextureBlock8P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	GSBlock::ReadBlock8(BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemory::ReadTextureBlock4P(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4P(BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemory::ReadTextureBlock8HP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock8HP(BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemory::ReadTextureBlock4HLP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4HLP(BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemory::ReadTextureBlock4HHP(uint32 bp, uint8* dst, int dstpitch, const GIFRegTEXA& TEXA) const
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4HHP(BlockPtr(bp), dst, dstpitch);
}

//

#include "Renderers/SW/GSTextureSW.h"

void GSLocalMemory::SaveBMP(const std::string& fn, uint32 bp, uint32 bw, uint32 psm, int w, int h)
{
	int pitch = w * 4;
	int size = pitch * h;
	void* bits = _aligned_malloc(size, 32);

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = bp;
	TEX0.TBW = bw;
	TEX0.PSM = psm;

	readPixel rp = m_psm[psm].rp;

	uint8* p = (uint8*)bits;

	for (int j = 0; j < h; j++, p += pitch)
	{
		for (int i = 0; i < w; i++)
		{
			((uint32*)p)[i] = (this->*rp)(i, j, TEX0.TBP0, TEX0.TBW);
		}
	}

	GSTextureSW t(GSTexture::Offscreen, w, h);

	if (t.Update(GSVector4i(0, 0, w, h), bits, pitch))
	{
		t.Save(fn);
	}

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
	//   Therefore, with the exception of row covering two pages in a z swizzle (which could touch e.g. pages 1 and 3 but not 2), all rows touch contiguous pages
	//   For now, we won't deal with that case as it's rare (only possible with a thin, unaligned rect on an unaligned bp on a z swizzle), and the worst issue looping too many pages could cause is unneccessary cache invalidation
	//   If code is added later to deal with the case, you'll need to change loopPagesWithBreak's block deduplication code, as it currently works by forcing the page number to increase monotonically which could cause blocks to be missed if e.g. the first row touches 1 and 3, and the second row touches 2 and 4
	// - Based on the above assumption, we calculate the range of pages a row could touch with full coverage, then add one to the start if the first tile doesn't touch its lower page, and subtract one from the end if the last tile doesn't touch its upper page
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
