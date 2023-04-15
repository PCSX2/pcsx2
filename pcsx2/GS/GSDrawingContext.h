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

// Clang note: 64bit bitfields are cast to 32bit here, since in VS it uses the format specified (64bit == 64bit), but in clang it converts to uint32_t.
// Since we're only using 64bit for bitfield alignment mess, we can just cast it to 32bit for everything.

#include "GSLocalMemory.h"

class alignas(32) GSDrawingContext
{
public:
	GIFRegXYOFFSET XYOFFSET;
	GIFRegTEX0     TEX0;
	GIFRegTEX1     TEX1;
	GIFRegCLAMP    CLAMP;
	GIFRegMIPTBP1  MIPTBP1;
	GIFRegMIPTBP2  MIPTBP2;
	GIFRegSCISSOR  SCISSOR;
	GIFRegALPHA    ALPHA;
	GIFRegTEST     TEST;
	GIFRegFBA      FBA;
	GIFRegFRAME    FRAME;
	GIFRegZBUF     ZBUF;

	struct
	{
		GSVector4 in;
		GSVector4i ex;
		GSVector4i ofxy;
	} scissor;

	struct
	{
		GSOffset fb;
		GSOffset zb;
		GSPixelOffset4* fzb4;
	} offset;

	GSDrawingContext()
	{
		memset(&offset, 0, sizeof(offset));

		Reset();
	}

	void Reset()
	{
		memset(&XYOFFSET, 0, sizeof(XYOFFSET));
		memset(&TEX0, 0, sizeof(TEX0));
		memset(&TEX1, 0, sizeof(TEX1));
		memset(&CLAMP, 0, sizeof(CLAMP));
		memset(&MIPTBP1, 0, sizeof(MIPTBP1));
		memset(&MIPTBP2, 0, sizeof(MIPTBP2));
		memset(&SCISSOR, 0, sizeof(SCISSOR));
		memset(&ALPHA, 0, sizeof(ALPHA));
		memset(&TEST, 0, sizeof(TEST));
		memset(&FBA, 0, sizeof(FBA));
		memset(&FRAME, 0, sizeof(FRAME));
		memset(&ZBUF, 0, sizeof(ZBUF));
	}

	void UpdateScissor()
	{
		ASSERT(XYOFFSET.OFX <= 0xf800 && XYOFFSET.OFY <= 0xf800);

		scissor.ex.U16[0] = (u16)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[1] = (u16)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY - 0x8000);
		scissor.ex.U16[2] = (u16)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[3] = (u16)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY - 0x8000);

		scissor.in = GSVector4(
			(int)SCISSOR.SCAX0,
			(int)SCISSOR.SCAY0,
			(int)SCISSOR.SCAX1 + 1,
			(int)SCISSOR.SCAY1 + 1);

		scissor.ofxy = GSVector4i(
			0x8000,
			0x8000,
			(int)XYOFFSET.OFX - 15,
			(int)XYOFFSET.OFY - 15);
	}

	GIFRegTEX0 GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap = false) const;

	void Dump(const std::string& filename);
};
