// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Clang note: 64bit bitfields are cast to 32bit here, since in VS it uses the format specified (64bit == 64bit), but in clang it converts to uint32_t.
// Since we're only using 64bit for bitfield alignment mess, we can just cast it to 32bit for everything.

#include "GSLocalMemory.h"

#include <string>

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
