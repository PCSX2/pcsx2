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
#include "GSBlock.h"
#include "GSExtra.h"

class CURRENT_ISA::GSLocalMemoryFunctions
{
	template <int psm, int bsx, int bsy, int alignment>
	static void WriteImageColumn(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int alignment>
	static void WriteImageBlock(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy>
	static void WriteImageLeftRight(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int trbpp>
	static void WriteImageTopBottom(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF);

	template <int psm, int bsx, int bsy, int trbpp>
	static void WriteImage(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	static void WriteImage24(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	static void WriteImage8H(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	static void WriteImage4HL(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	static void WriteImage4HH(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	static void WriteImage24Z(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);
	static void WriteImageX(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	// TODO: ReadImage32/24/...

	static void ReadTexture32(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureGPU24(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture24(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture16(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture8(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture8H(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4HL(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4HH(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	static void ReadTextureBlock32(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock24(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock16(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock8(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock8H(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4HL(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4HH(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	static void ReadTexture8P(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4P(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture8HP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4HLP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture4HHP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

	static void ReadTextureBlock8P(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4P(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock8HP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4HLP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock4HHP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

#if _M_SSE == 0x501
	static void ReadTexture8HSW(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTexture8HHSW(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock8HSW(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
	static void ReadTextureBlock8HHSW(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);
#endif

	template <typename T>
	static void ReadTexture(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA);

public:
	static void ReadImageX(const GSLocalMemory& mem, int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG);

	static void PopulateFunctions(GSLocalMemory& mem);
};

MULTI_ISA_UNSHARED_IMPL;

void CURRENT_ISA::GSLocalMemoryPopulateFunctions(GSLocalMemory& mem)
{
	GSLocalMemoryFunctions::PopulateFunctions(mem);
}

void GSLocalMemoryFunctions::PopulateFunctions(GSLocalMemory& mem)
{
	mem.m_readImageX = ReadImageX;

	for (GSLocalMemory::psm_t& psm : mem.m_psm)
	{
		psm.wi = WriteImage<PSMCT32, 8, 8, 32>;
		psm.ri = ReadImageX; // TODO
		psm.rtx = ReadTexture32;
		psm.rtxP = ReadTexture32;
		psm.rtxb = ReadTextureBlock32;
		psm.rtxbP = ReadTextureBlock32;
	}

	mem.m_psm[PSMCT24].wi = WriteImage24; // TODO
	mem.m_psm[PSMCT16].wi = WriteImage<PSMCT16, 16, 8, 16>;
	mem.m_psm[PSMCT16S].wi = WriteImage<PSMCT16S, 16, 8, 16>;
	mem.m_psm[PSMT8].wi = WriteImage<PSMT8, 16, 16, 8>;
	mem.m_psm[PSMT4].wi = WriteImage<PSMT4, 32, 16, 4>;
	mem.m_psm[PSMT8H].wi = WriteImage8H; // TODO
	mem.m_psm[PSMT4HL].wi = WriteImage4HL; // TODO
	mem.m_psm[PSMT4HH].wi = WriteImage4HH; // TODO
	mem.m_psm[PSMZ32].wi = WriteImage<PSMZ32, 8, 8, 32>;
	mem.m_psm[PSMZ24].wi = WriteImage24Z; // TODO
	mem.m_psm[PSMZ16].wi = WriteImage<PSMZ16, 16, 8, 16>;
	mem.m_psm[PSMZ16S].wi = WriteImage<PSMZ16S, 16, 8, 16>;

	mem.m_psm[PSMCT24].rtx = ReadTexture24;
	mem.m_psm[PSGPU24].rtx = ReadTextureGPU24;
	mem.m_psm[PSMCT16].rtx = ReadTexture16;
	mem.m_psm[PSMCT16S].rtx = ReadTexture16;
	mem.m_psm[PSMT8].rtx = ReadTexture8;
	mem.m_psm[PSMT4].rtx = ReadTexture4;
	mem.m_psm[PSMT8H].rtx = ReadTexture8H;
	mem.m_psm[PSMT4HL].rtx = ReadTexture4HL;
	mem.m_psm[PSMT4HH].rtx = ReadTexture4HH;
	mem.m_psm[PSMZ32].rtx = ReadTexture32;
	mem.m_psm[PSMZ24].rtx = ReadTexture24;
	mem.m_psm[PSMZ16].rtx = ReadTexture16;
	mem.m_psm[PSMZ16S].rtx = ReadTexture16;

	mem.m_psm[PSMCT24].rtxP = ReadTexture24;
	mem.m_psm[PSMCT16].rtxP = ReadTexture16;
	mem.m_psm[PSMCT16S].rtxP = ReadTexture16;
	mem.m_psm[PSMT8].rtxP = ReadTexture8P;
	mem.m_psm[PSMT4].rtxP = ReadTexture4P;
	mem.m_psm[PSMT8H].rtxP = ReadTexture8HP;
	mem.m_psm[PSMT4HL].rtxP = ReadTexture4HLP;
	mem.m_psm[PSMT4HH].rtxP = ReadTexture4HHP;
	mem.m_psm[PSMZ32].rtxP = ReadTexture32;
	mem.m_psm[PSMZ24].rtxP = ReadTexture24;
	mem.m_psm[PSMZ16].rtxP = ReadTexture16;
	mem.m_psm[PSMZ16S].rtxP = ReadTexture16;

	mem.m_psm[PSMCT24].rtxb = ReadTextureBlock24;
	mem.m_psm[PSMCT16].rtxb = ReadTextureBlock16;
	mem.m_psm[PSMCT16S].rtxb = ReadTextureBlock16;
	mem.m_psm[PSMT8].rtxb = ReadTextureBlock8;
	mem.m_psm[PSMT4].rtxb = ReadTextureBlock4;
	mem.m_psm[PSMT8H].rtxb = ReadTextureBlock8H;
	mem.m_psm[PSMT4HL].rtxb = ReadTextureBlock4HL;
	mem.m_psm[PSMT4HH].rtxb = ReadTextureBlock4HH;
	mem.m_psm[PSMZ32].rtxb = ReadTextureBlock32;
	mem.m_psm[PSMZ24].rtxb = ReadTextureBlock24;
	mem.m_psm[PSMZ16].rtxb = ReadTextureBlock16;
	mem.m_psm[PSMZ16S].rtxb = ReadTextureBlock16;

	mem.m_psm[PSMCT24].rtxbP = ReadTextureBlock24;
	mem.m_psm[PSMCT16].rtxbP = ReadTextureBlock16;
	mem.m_psm[PSMCT16S].rtxbP = ReadTextureBlock16;
	mem.m_psm[PSMT8].rtxbP = ReadTextureBlock8P;
	mem.m_psm[PSMT4].rtxbP = ReadTextureBlock4P;
	mem.m_psm[PSMT8H].rtxbP = ReadTextureBlock8HP;
	mem.m_psm[PSMT4HL].rtxbP = ReadTextureBlock4HLP;
	mem.m_psm[PSMT4HH].rtxbP = ReadTextureBlock4HHP;
	mem.m_psm[PSMZ32].rtxbP = ReadTextureBlock32;
	mem.m_psm[PSMZ24].rtxbP = ReadTextureBlock24;
	mem.m_psm[PSMZ16].rtxbP = ReadTextureBlock16;
	mem.m_psm[PSMZ16S].rtxbP = ReadTextureBlock16;

#if _M_SSE == 0x501
	if (g_cpu.hasSlowGather)
	{
		mem.m_psm[PSMT8].rtx = ReadTexture8HSW;
		mem.m_psm[PSMT8H].rtx = ReadTexture8HHSW;
		mem.m_psm[PSMT8].rtxb = ReadTextureBlock8HSW;
		mem.m_psm[PSMT8H].rtxb = ReadTextureBlock8HHSW;
	}
#endif
}

template <typename Fn>
static void foreachBlock(const GSOffset& off, GSLocalMemory& mem, const GSVector4i& r, u8* dst, int dstpitch, int bpp, Fn&& fn)
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
			const u8* src = mem.BlockPtr(bn.value());
			u8* read_dst = dst + x;
			fn(read_dst, src);
		}
	}
}

template <int psm, int bsx, int bsy, int alignment>
void GSLocalMemoryFunctions::WriteImageColumn(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	const int csy = bsy / 4;

	for (int offset = srcpitch * csy; h >= csy; h -= csy, y += csy, src += offset)
	{
		for (int x = l; x < r; x += bsx)
		{
			switch (psm)
			{
				case PSMCT32: GSBlock::WriteColumn32<alignment, 0xffffffff>(y, mem.BlockPtr32(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSMCT16: GSBlock::WriteColumn16<alignment>(y, mem.BlockPtr16(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMCT16S: GSBlock::WriteColumn16<alignment>(y, mem.BlockPtr16S(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMT8: GSBlock::WriteColumn8<alignment>(y, mem.BlockPtr8(x, y, bp, bw), &src[x], srcpitch); break;
				case PSMT4: GSBlock::WriteColumn4<alignment>(y, mem.BlockPtr4(x, y, bp, bw), &src[x >> 1], srcpitch); break;
				case PSMZ32: GSBlock::WriteColumn32<alignment, 0xffffffff>(y, mem.BlockPtr32Z(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSMZ16: GSBlock::WriteColumn16<alignment>(y, mem.BlockPtr16Z(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMZ16S: GSBlock::WriteColumn16<alignment>(y, mem.BlockPtr16SZ(x, y, bp, bw), &src[x * 2], srcpitch); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy, int alignment>
void GSLocalMemoryFunctions::WriteImageBlock(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	for (int offset = srcpitch * bsy; h >= bsy; h -= bsy, y += bsy, src += offset)
	{
		for (int x = l; x < r; x += bsx)
		{
			switch (psm)
			{
				case PSMCT32: GSBlock::WriteBlock32<alignment, 0xffffffff>(mem.BlockPtr32(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSMCT16: GSBlock::WriteBlock16<alignment>(mem.BlockPtr16(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMCT16S: GSBlock::WriteBlock16<alignment>(mem.BlockPtr16S(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMT8: GSBlock::WriteBlock8<alignment>(mem.BlockPtr8(x, y, bp, bw), &src[x], srcpitch); break;
				case PSMT4: GSBlock::WriteBlock4<alignment>(mem.BlockPtr4(x, y, bp, bw), &src[x >> 1], srcpitch); break;
				case PSMZ32: GSBlock::WriteBlock32<alignment, 0xffffffff>(mem.BlockPtr32Z(x, y, bp, bw), &src[x * 4], srcpitch); break;
				case PSMZ16: GSBlock::WriteBlock16<alignment>(mem.BlockPtr16Z(x, y, bp, bw), &src[x * 2], srcpitch); break;
				case PSMZ16S: GSBlock::WriteBlock16<alignment>(mem.BlockPtr16SZ(x, y, bp, bw), &src[x * 2], srcpitch); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy>
void GSLocalMemoryFunctions::WriteImageLeftRight(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	for (; h > 0; y++, h--, src += srcpitch)
	{
		for (int x = l; x < r; x++)
		{
			switch (psm)
			{
				case PSMCT32: mem.WritePixel32(x, y, *(u32*)&src[x * 4], bp, bw); break;
				case PSMCT16: mem.WritePixel16(x, y, *(u16*)&src[x * 2], bp, bw); break;
				case PSMCT16S: mem.WritePixel16S(x, y, *(u16*)&src[x * 2], bp, bw); break;
				case PSMT8: mem.WritePixel8(x, y, src[x], bp, bw); break;
				case PSMT4: mem.WritePixel4(x, y, src[x >> 1] >> ((x & 1) << 2), bp, bw); break;
				case PSMZ32: mem.WritePixel32Z(x, y, *(u32*)&src[x * 4], bp, bw); break;
				case PSMZ16: mem.WritePixel16Z(x, y, *(u16*)&src[x * 2], bp, bw); break;
				case PSMZ16S: mem.WritePixel16SZ(x, y, *(u16*)&src[x * 2], bp, bw); break;
				// TODO
				default: __assume(0);
			}
		}
	}
}

template <int psm, int bsx, int bsy, int trbpp>
void GSLocalMemoryFunctions::WriteImageTopBottom(GSLocalMemory& mem, int l, int r, int y, int h, const u8* src, int srcpitch, const GIFRegBITBLTBUF& BITBLTBUF)
{
	alignas(32) u8 buff[64]; // merge buffer for one column

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	const int csy = bsy / 4;

	// merge incomplete column

	int y2 = y & (csy - 1);

	if (y2 > 0)
	{
		int h2 = std::min(h, csy - y2);

		for (int x = l; x < r; x += bsx)
		{
			u8* dst = NULL;

			switch (psm)
			{
				case PSMCT32: dst = mem.BlockPtr32(x, y, bp, bw); break;
				case PSMCT16: dst = mem.BlockPtr16(x, y, bp, bw); break;
				case PSMCT16S: dst = mem.BlockPtr16S(x, y, bp, bw); break;
				case PSMT8: dst = mem.BlockPtr8(x, y, bp, bw); break;
				case PSMT4: dst = mem.BlockPtr4(x, y, bp, bw); break;
				case PSMZ32: dst = mem.BlockPtr32Z(x, y, bp, bw); break;
				case PSMZ16: dst = mem.BlockPtr16Z(x, y, bp, bw); break;
				case PSMZ16S: dst = mem.BlockPtr16SZ(x, y, bp, bw); break;
				// TODO
				default: __assume(0);
			}

			switch (psm)
			{
				case PSMCT32:
				case PSMZ32:
					GSBlock::ReadColumn32(y, dst, buff, 32);
					memcpy(&buff[32], &src[x * 4], 32);
					GSBlock::WriteColumn32<32, 0xffffffff>(y, dst, buff, 32);
					break;
				case PSMCT16:
				case PSMCT16S:
				case PSMZ16:
				case PSMZ16S:
					GSBlock::ReadColumn16(y, dst, buff, 32);
					memcpy(&buff[32], &src[x * 2], 32);
					GSBlock::WriteColumn16<32>(y, dst, buff, 32);
					break;
				case PSMT8:
					GSBlock::ReadColumn8(y, dst, buff, 16);
					for (int i = 0, j = y2; i < h2; i++, j++)
						memcpy(&buff[j * 16], &src[i * srcpitch + x], 16);
					GSBlock::WriteColumn8<32>(y, dst, buff, 16);
					break;
				case PSMT4:
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
#if FAST_UNALIGNED
			WriteImageColumn<psm, bsx, bsy, 0>(mem, l, r, y, h2, src, srcpitch, BITBLTBUF);
#else
			size_t addr = (size_t)&src[l * trbpp >> 3];

			if ((addr & 31) == 0 && (srcpitch & 31) == 0)
			{
				WriteImageColumn<psm, bsx, bsy, 32>(mem, l, r, y, h2, src, srcpitch, BITBLTBUF);
			}
			else if ((addr & 15) == 0 && (srcpitch & 15) == 0)
			{
				WriteImageColumn<psm, bsx, bsy, 16>(mem, l, r, y, h2, src, srcpitch, BITBLTBUF);
			}
			else
			{
				WriteImageColumn<psm, bsx, bsy, 0>(mem, l, r, y, h2, src, srcpitch, BITBLTBUF);
			}
#endif

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
			u8* dst = NULL;

			switch (psm)
			{
			case PSMCT32: dst = mem.BlockPtr32(x, y, bp, bw); break;
			case PSMCT16: dst = mem.BlockPtr16(x, y, bp, bw); break;
			case PSMCT16S: dst = mem.BlockPtr16S(x, y, bp, bw); break;
			case PSMT8: dst = mem.BlockPtr8(x, y, bp, bw); break;
			case PSMT4: dst = mem.BlockPtr4(x, y, bp, bw); break;
			case PSMZ32: dst = mem.BlockPtr32Z(x, y, bp, bw); break;
			case PSMZ16: dst = mem.BlockPtr16Z(x, y, bp, bw); break;
			case PSMZ16S: dst = mem.BlockPtr16SZ(x, y, bp, bw); break;
			// TODO
			default: __assume(0);
			}

			switch (psm)
			{
				case PSMCT32:
				case PSMZ32:
					GSBlock::ReadColumn32(y, dst, buff, 32);
					memcpy(&buff[0], &src[x * 4], 32);
					GSBlock::WriteColumn32<32, 0xffffffff>(y, dst, buff, 32);
					break;
				case PSMCT16:
				case PSMCT16S:
				case PSMZ16:
				case PSMZ16S:
					GSBlock::ReadColumn16(y, dst, buff, 32);
					memcpy(&buff[0], &src[x * 2], 32);
					GSBlock::WriteColumn16<32>(y, dst, buff, 32);
					break;
				case PSMT8:
					GSBlock::ReadColumn8(y, dst, buff, 16);
					for (int i = 0; i < h; i++)
						memcpy(&buff[i * 16], &src[i * srcpitch + x], 16);
					GSBlock::WriteColumn8<32>(y, dst, buff, 16);
					break;
				case PSMT4:
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
void GSLocalMemoryFunctions::WriteImage(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	const int l = (int)TRXPOS.DSAX;
	const int r = l + (int)TRXREG.RRW;

	// finish the incomplete row first

	if (tx != l)
	{
		int n = std::min(len, (r - tx) * trbpp >> 3);
		WriteImageX(mem, tx, ty, src, n, BITBLTBUF, TRXPOS, TRXREG);
		src += n;
		len -= n;
	}

	const int la = (l + (bsx - 1)) & ~(bsx - 1);
	const int ra = r & ~(bsx - 1);
	// Round up to the nearest byte (NFL 2K5 does r = 1, l = 0 bpp =4, causing divide by zero)
	const int srcpitch = (((r - l) * trbpp) + 7) >> 3;
	int h = len / srcpitch;

	// Slow path for odd width 4bpp, the fast path expects everything to be perfectly aligned and great,
	// but things get hairy with 4bpp pixels and odd widths since the lowest size we can address is 8bits, it goes out of sync.
	// Although I call this a slow path, it's probably faster than modifying the data alignment every other line.
	// GT3 demo, Jak 2 Japanese subtitles, and the BG Dark Alliance minimap do this.
	if (trbpp == 4 && (TRXREG.RRW & 0x1))
	{
		int count = 0;
		const int t = TRXPOS.DSAY;
		const int b = t + (int)TRXREG.RRH;
		for (int y = t; y < b; y++)
		{
			for (int x = l; x < r; x++)
			{
				mem.WritePixel4(x, y, src[count >> 1] >> ((count & 1) << 2), BITBLTBUF.DBP, BITBLTBUF.DBW);
				count++;
			}
		}
		return;
	}

	if (ra - la >= bsx && h > 0) // "transfer width" >= "block width" && there is at least one full row
	{
		const u8* s = &src[-l * trbpp >> 3];

		src += srcpitch * h;
		len -= srcpitch * h;

		// left part

		if (l < la)
		{
			WriteImageLeftRight<psm, bsx, bsy>(mem, l, la, ty, h, s, srcpitch, BITBLTBUF);
		}

		// right part

		if (ra < r)
		{
			WriteImageLeftRight<psm, bsx, bsy>(mem, ra, r, ty, h, s, srcpitch, BITBLTBUF);
		}

		// horizontally aligned part

		if (la < ra)
		{
			// top part

			{
				int h2 = std::min(h, bsy - (ty & (bsy - 1)));

				if (h2 < bsy)
				{
					WriteImageTopBottom<psm, bsx, bsy, trbpp>(mem, la, ra, ty, h2, s, srcpitch, BITBLTBUF);

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
#if FAST_UNALIGNED
					WriteImageBlock<psm, bsx, bsy, 0>(mem, la, ra, ty, h2, s, srcpitch, BITBLTBUF);
#else
					size_t addr = (size_t)&s[la * trbpp >> 3];

					if ((addr & 31) == 0 && (srcpitch & 31) == 0)
					{
						WriteImageBlock<psm, bsx, bsy, 32>(mem, la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}
					else if ((addr & 15) == 0 && (srcpitch & 15) == 0)
					{
						WriteImageBlock<psm, bsx, bsy, 16>(mem, la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}
					else
					{
						WriteImageBlock<psm, bsx, bsy, 0>(mem, la, ra, ty, h2, s, srcpitch, BITBLTBUF);
					}
#endif

					s += srcpitch * h2;
					ty += h2;
					h -= h2;
				}
			}

			// bottom part

			if (h > 0)
			{
				WriteImageTopBottom<psm, bsx, bsy, trbpp>(mem, la, ra, ty, h, s, srcpitch, BITBLTBUF);

				// s += srcpitch * h;
				ty += h;
				// h -= h;
			}
		}
	}

	// the rest

	if (len > 0)
	{
		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
}


static bool IsTopLeftAligned(int dsax, int tx, int ty, int bw, int bh)
{
	return ((dsax & (bw - 1)) == 0 && (tx & (bw - 1)) == 0 && dsax == tx && (ty & (bh - 1)) == 0);
}

void GSLocalMemoryFunctions::WriteImage24(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW * 3;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock24(src + (x - tx) * 3, srcpitch, mem.BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemoryFunctions::WriteImage8H(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock8H(src + (x - tx), srcpitch, mem.BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemoryFunctions::WriteImage4HL(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW / 2;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock4HL(src + (x - tx) / 2, srcpitch, mem.BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemoryFunctions::WriteImage4HH(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW / 2;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock4HH(src + (x - tx) / 2, srcpitch, mem.BlockPtr32(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

void GSLocalMemoryFunctions::WriteImage24Z(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (TRXREG.RRW == 0)
		return;

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int tw = TRXPOS.DSAX + TRXREG.RRW, srcpitch = TRXREG.RRW * 3;
	int th = len / srcpitch;

	bool aligned = IsTopLeftAligned(TRXPOS.DSAX, tx, ty, 8, 8);

	if (!aligned || (tw & 7) || (th & 7) || (len % srcpitch))
	{
		// TODO

		WriteImageX(mem, tx, ty, src, len, BITBLTBUF, TRXPOS, TRXREG);
	}
	else
	{
		th += ty;

		for (int y = ty; y < th; y += 8, src += srcpitch * 8)
		{
			for (int x = tx; x < tw; x += 8)
			{
				GSBlock::UnpackAndWriteBlock24(src + (x - tx) * 3, srcpitch, mem.BlockPtr32Z(x, y, bp, bw));
			}
		}

		ty = th;
	}
}

/// Helper for WriteImageX and ReadImageX
/// `len` is in pixels, unlike WriteImageX/ReadImageX where it's bytes
/// `xinc` is the amount to increment `x` by per iteration
/// Creates a GSOffset::PAHelper on a starting (x, y) to get the base address for each line,
///  then `fn` on the helper and an x offset once for every `xinc` pixels along that line
template <typename Fn>
static void readWriteHelper(int& tx, int& ty, int len, int xinc, int sx, int w, const GSOffset& off, Fn&& fn)
{
	int y = ty;
	int ex = sx + w;
	int remX = ex - tx;

	ASSERT(remX >= 0);

	GSOffset::PAHelper pa = off.paMulti(tx, y);

	while (len > 0)
	{
		int stop = std::min(remX, len);
		len -= stop;
		remX -= stop;

		for (int x = 0; x < stop; x += xinc)
			fn(pa, x);

		if (remX == 0)
		{
			y++;
			remX = w;
			pa = off.paMulti(sx, y);
		}
	}

	tx = ex - remX;
	ty = y;
}

void GSLocalMemoryFunctions::WriteImageX(GSLocalMemory& mem, int& tx, int& ty, const u8* src, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (len <= 0)
		return;

	const u8* pb = (u8*)src;
	const u16* pw = (u16*)src;
	const u32* pd = (u32*)src;
	u8* vm8 = mem.vm8();
	u16* vm16 = mem.vm16();
	u32* vm32 = mem.vm32();

	u32 bp = BITBLTBUF.DBP;
	u32 bw = BITBLTBUF.DBW;

	int sx = TRXPOS.DSAX;
	int w = TRXREG.RRW;

	GSOffset off = mem.GetOffset(bp, bw, BITBLTBUF.DPSM);

	switch (BITBLTBUF.DPSM)
	{
		case PSMCT32:
		case PSMZ32:
			readWriteHelper(tx, ty, len / 4, 1, sx, w, off.assertSizesMatch(GSLocalMemory::swizzle32), [&](GSOffset::PAHelper& pa, int x)
			{
				vm32[pa.value(x)] = *pd;
				pd++;
			});
			break;

		case PSMCT24:
		case PSMZ24:
			readWriteHelper(tx, ty, len / 3, 1, sx, w, off.assertSizesMatch(GSLocalMemory::swizzle32), [&](GSOffset::PAHelper& pa, int x)
			{
				mem.WritePixel24(&vm32[pa.value(x)], *(u32*)pb);
				pb += 3;
			});
			break;

		case PSMCT16:
		case PSMCT16S:
		case PSMZ16:
		case PSMZ16S:
			readWriteHelper(tx, ty, len / 2, 1, sx, w, off.assertSizesMatch(GSLocalMemory::swizzle16), [&](GSOffset::PAHelper& pa, int x)
			{
				vm16[pa.value(x)] = *pw;
				pw++;
			});
			break;

		case PSMT8:
			readWriteHelper(tx, ty, len, 1, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT8), [&](GSOffset::PAHelper& pa, int x)
			{
				vm8[pa.value(x)] = *pb;
				pb++;
			});
			break;

		case PSMT4:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4), [&](GSOffset::PAHelper& pa, int x)
			{
				mem.WritePixel4(pa.value(x), *pb & 0xf);
				mem.WritePixel4(pa.value(x + 1), *pb >> 4);
				pb++;
			});
			break;

		case PSMT8H:
			readWriteHelper(tx, ty, len, 1, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT8H), [&](GSOffset::PAHelper& pa, int x)
			{
				mem.WritePixel8H(&vm32[pa.value(x)], *pb);
				pb++;
			});
			break;

		case PSMT4HL:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4HL), [&](GSOffset::PAHelper& pa, int x)
			{
				mem.WritePixel4HL(&vm32[pa.value(x)], *pb & 0xf);
				mem.WritePixel4HL(&vm32[pa.value(x + 1)], *pb >> 4);
				pb++;
			});
			break;

		case PSMT4HH:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4HH), [&](GSOffset::PAHelper& pa, int x)
			{
				mem.WritePixel4HH(&vm32[pa.value(x)], *pb & 0xf);
				mem.WritePixel4HH(&vm32[pa.value(x + 1)], *pb >> 4);
				pb++;
			});
			break;
	}
}

//

void GSLocalMemoryFunctions::ReadImageX(const GSLocalMemory& mem, int& tx, int& ty, u8* dst, int len, GIFRegBITBLTBUF& BITBLTBUF, GIFRegTRXPOS& TRXPOS, GIFRegTRXREG& TRXREG)
{
	if (len <= 0)
		return;

	u8* RESTRICT pb = (u8*)dst;
	u16* RESTRICT pw = (u16*)dst;
	u32* RESTRICT pd = (u32*)dst;
	const u32* vm32 = mem.vm32();
	const u16* vm16 = mem.vm16();
	const u8* vm8 = mem.vm8();

	u32 bp = BITBLTBUF.SBP;
	u32 bw = BITBLTBUF.SBW;

	int sx = TRXPOS.SSAX;
	int w = TRXREG.RRW;

	GSOffset off = mem.GetOffset(bp, bw, BITBLTBUF.SPSM);

	// printf("spsm=%d x=%d ex=%d y=%d len=%d\n", BITBLTBUF.SPSM, x, ex, y, len);

	switch (BITBLTBUF.SPSM)
	{
		case PSMCT32:
		case PSMZ32:
		{
			// MGS1 intro, fade effect between two scenes (airplane outside-inside transition)

			int x = tx;
			int y = ty;
			int ex = sx + w;

			len /= 4;

			u32* vm = mem.vm32();
			GSOffset::PAHelper pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(0, y);

			while (len > 0)
			{
				for (; len > 0 && x < ex && (x & 7); len--, x++, pd++)
				{
					*pd = vm[pa.value(x)];
				}

				// aligned to a column

				for (int ex8 = ex - 8; len >= 8 && x <= ex8; len -= 8, x += 8, pd += 8)
				{
					u32* ps = &vm[pa.value(x)];

					GSVector4i::store<false>(&pd[0], GSVector4i::load(ps + 0, ps + 4));
					GSVector4i::store<false>(&pd[4], GSVector4i::load(ps + 8, ps + 12));

					for (int i = 0; i < 8; i++)
						ASSERT(pd[i] == vm[pa.value(x + i)]);
				}

				for (; len > 0 && x < ex; len--, x++, pd++)
				{
					*pd = vm[pa.value(x)];
				}

				if (x == ex)
				{
					y++;
					x = sx;
					pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(0, y);
				}
			}

			tx = x;
			ty = y;
		}
		break;

		case PSMCT24:
		case PSMZ24:
			readWriteHelper(tx, ty, len / 3, 1, sx, w, off.assertSizesMatch(GSLocalMemory::swizzle32), [&](GSOffset::PAHelper& pa, int x)
			{
				u32 c = vm32[pa.value(x)];
				pb[0] = (u8)(c);
				pb[1] = (u8)(c >> 8);
				pb[2] = (u8)(c >> 16);
				pb += 3;
			});
			break;
		case PSMCT16:
		case PSMCT16S:
		case PSMZ16:
		case PSMZ16S:
			readWriteHelper(tx, ty, len / 2, 1, sx, w, off.assertSizesMatch(GSLocalMemory::swizzle16), [&](GSOffset::PAHelper& pa, int x)
			{
				*pw = vm16[pa.value(x)];
				pw++;
			});
			break;

		case PSMT8:
			readWriteHelper(tx, ty, len, 1, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT8), [&](GSOffset::PAHelper& pa, int x)
			{
				*pb = vm8[pa.value(x)];
				pb++;
			});
			break;

		case PSMT4:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4), [&](GSOffset::PAHelper& pa, int x)
			{
				u8 low = mem.ReadPixel4(pa.value(x));
				u8 high = mem.ReadPixel4(pa.value(x + 1));
				*pb = low | (high << 4);
			});
			break;

		case PSMT8H:
			readWriteHelper(tx, ty, len, 1, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT8H), [&](GSOffset::PAHelper& pa, int x)
			{
				*pb = (u8)(vm32[pa.value(x)] >> 24);
				pb++;
			});
			break;

		case PSMT4HL:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4HL), [&](GSOffset::PAHelper& pa, int x)
			{
				u32 c0 = vm32[pa.value(x)] >> 24 & 0x0f;
				u32 c1 = vm32[pa.value(x + 1)] >> 20 & 0xf0;
				*pb = (u8)(c0 | c1);
				pb++;
			});
			break;

		case PSMT4HH:
			readWriteHelper(tx, ty, len * 2, 2, sx, w, GSOffset::fromKnownPSM(bp, bw, PSMT4HH), [&](GSOffset::PAHelper& pa, int x)
			{
				u32 c0 = vm32[pa.value(x)] >> 28 & 0x0f;
				u32 c1 = vm32[pa.value(x + 1)] >> 24 & 0xf0;
				*pb = (u8)(c0 | c1);
				pb++;
			});
			break;
	}
}

///////////////////

void GSLocalMemoryFunctions::ReadTexture32(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock32(src, read_dst, dstpitch);
	});
}

void GSLocalMemoryFunctions::ReadTexture24(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	if (TEXA.AEM)
	{
		foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
		{
			GSBlock::ReadAndExpandBlock24<true>(src, read_dst, dstpitch, TEXA);
		});
	}
	else
	{
		foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
		{
			GSBlock::ReadAndExpandBlock24<false>(src, read_dst, dstpitch, TEXA);
		});
	}
}

void GSLocalMemoryFunctions::ReadTextureGPU24(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle16), mem, r, dst, dstpitch, 16, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock16(src, read_dst, dstpitch);
	});

	// Convert packed RGB scanline to 32 bits RGBA
	ASSERT(dstpitch >= r.width() * 4);
	for (int y = r.top; y < r.bottom; y++)
	{
		u8* line = dst + y * dstpitch;

		for (int x = r.right; x >= r.left; x--)
		{
			*(u32*)&line[x * 4] = *(u32*)&line[x * 3] & 0xFFFFFF;
		}
	}
}

void GSLocalMemoryFunctions::ReadTexture16(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	if (TEXA.AEM)
	{
		foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle16), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
		{
			GSBlock::ReadAndExpandBlock16<true>(src, read_dst, dstpitch, TEXA);
		});
	}
	else
	{
		foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle16), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
		{
			GSBlock::ReadAndExpandBlock16<false>(src, read_dst, dstpitch, TEXA);
		});
	}
}

void GSLocalMemoryFunctions::ReadTexture8(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle8), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock8_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemoryFunctions::ReadTexture4(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle4), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock4_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemoryFunctions::ReadTexture8H(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock8H_32(src, read_dst, dstpitch, pal);
	});
}

#if _M_SSE == 0x501
void GSLocalMemoryFunctions::ReadTexture8HSW(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle8), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock8_32HSW(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemoryFunctions::ReadTexture8HHSW(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock8H_32HSW(src, read_dst, dstpitch, pal);
	});
}
#endif

void GSLocalMemoryFunctions::ReadTexture4HL(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock4HL_32(src, read_dst, dstpitch, pal);
	});
}

void GSLocalMemoryFunctions::ReadTexture4HH(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	const u32* pal = mem.m_clut;

	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 32, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadAndExpandBlock4HH_32(src, read_dst, dstpitch, pal);
	});
}

///////////////////

void GSLocalMemoryFunctions::ReadTextureBlock32(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock32(mem.BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemoryFunctions::ReadTextureBlock24(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	if (TEXA.AEM)
	{
		GSBlock::ReadAndExpandBlock24<true>(mem.BlockPtr(bp), dst, dstpitch, TEXA);
	}
	else
	{
		GSBlock::ReadAndExpandBlock24<false>(mem.BlockPtr(bp), dst, dstpitch, TEXA);
	}
}

void GSLocalMemoryFunctions::ReadTextureBlock16(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	if (TEXA.AEM)
	{
		GSBlock::ReadAndExpandBlock16<true>(mem.BlockPtr(bp), dst, dstpitch, TEXA);
	}
	else
	{
		GSBlock::ReadAndExpandBlock16<false>(mem.BlockPtr(bp), dst, dstpitch, TEXA);
	}
}

void GSLocalMemoryFunctions::ReadTextureBlock8(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8_32(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

void GSLocalMemoryFunctions::ReadTextureBlock4(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4_32(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

void GSLocalMemoryFunctions::ReadTextureBlock8H(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8H_32(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

#if _M_SSE == 0x501
void GSLocalMemoryFunctions::ReadTextureBlock8HSW(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8_32HSW(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

void GSLocalMemoryFunctions::ReadTextureBlock8HHSW(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock8H_32HSW(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}
#endif

void GSLocalMemoryFunctions::ReadTextureBlock4HL(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4HL_32(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

void GSLocalMemoryFunctions::ReadTextureBlock4HH(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadAndExpandBlock4HH_32(mem.BlockPtr(bp), dst, dstpitch, mem.m_clut);
}

// 32/8

void GSLocalMemoryFunctions::ReadTexture8P(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle8), mem, r, dst, dstpitch, 8, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock8(src, read_dst, dstpitch);
	});
}

void GSLocalMemoryFunctions::ReadTexture4P(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle4), mem, r, dst, dstpitch, 8, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock4P(src, read_dst, dstpitch);
	});
}

void GSLocalMemoryFunctions::ReadTexture8HP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 8, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock8HP(src, read_dst, dstpitch);
	});
}

void GSLocalMemoryFunctions::ReadTexture4HLP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 8, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock4HLP(src, read_dst, dstpitch);
	});
}

void GSLocalMemoryFunctions::ReadTexture4HHP(GSLocalMemory& mem, const GSOffset& off, const GSVector4i& r, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	foreachBlock(off.assertSizesMatch(GSLocalMemory::swizzle32), mem, r, dst, dstpitch, 8, [&](u8* read_dst, const u8* src)
	{
		GSBlock::ReadBlock4HHP(src, read_dst, dstpitch);
	});
}

//

void GSLocalMemoryFunctions::ReadTextureBlock8P(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	GSBlock::ReadBlock8(mem.BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemoryFunctions::ReadTextureBlock4P(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4P(mem.BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemoryFunctions::ReadTextureBlock8HP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock8HP(mem.BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemoryFunctions::ReadTextureBlock4HLP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4HLP(mem.BlockPtr(bp), dst, dstpitch);
}

void GSLocalMemoryFunctions::ReadTextureBlock4HHP(const GSLocalMemory& mem, u32 bp, u8* dst, int dstpitch, const GIFRegTEXA& TEXA)
{
	ALIGN_STACK(32);

	GSBlock::ReadBlock4HHP(mem.BlockPtr(bp), dst, dstpitch);
}
