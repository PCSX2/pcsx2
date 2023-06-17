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
		GSVector4i in;
		GSVector4i cull;
		GSVector4i xyof;
	} scissor;

	struct
	{
		GSOffset fb;
		GSOffset zb;
		GSPixelOffset4* fzb4;
	} offset;

	GSDrawingContext();

	void Reset();

	void UpdateScissor();

	GIFRegTEX0 GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap = false) const;

	void Dump(const std::string& filename);
};
