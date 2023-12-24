// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GSLocalMemory.h"

union RGBAMask
{
	u32 _u32;
	struct
	{
		u32 r : 1;
		u32 g : 1;
		u32 b : 1;
		u32 a : 1;
	} c;
};

class GSDirtyRect
{
public:
	GSVector4i r;
	u32 psm;
	u32 bw;
	RGBAMask rgba;
	bool req_linear;

	GSDirtyRect();
	GSDirtyRect(GSVector4i& r, u32 psm, u32 bw, RGBAMask rgba, bool req_linear);
	GSVector4i GetDirtyRect(GIFRegTEX0 TEX0, bool align) const;
};

class GSDirtyRectList : public std::vector<GSDirtyRect>
{
public:
	GSDirtyRectList() {}
	GSVector4i GetTotalRect(GIFRegTEX0 TEX0, const GSVector2i& size) const;
	u32 GetDirtyChannels();
	GSVector4i GetDirtyRect(size_t index, GIFRegTEX0 TEX0, const GSVector4i& clamp, bool align) const;
};
