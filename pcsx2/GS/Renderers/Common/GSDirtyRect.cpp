// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSDirtyRect.h"
#include <vector>

GSDirtyRect::GSDirtyRect() :
	r(GSVector4i::zero()),
	psm(PSMCT32),
	bw(1),
	rgba({}),
	req_linear(false)
{
}

GSDirtyRect::GSDirtyRect(GSVector4i& r, u32 psm, u32 bw, RGBAMask rgba, bool req_linear) :
	r(r),
	psm(psm),
	bw(bw),
	rgba(rgba),
	req_linear(req_linear)
{
}

GSVector4i GSDirtyRect::GetDirtyRect(GIFRegTEX0 TEX0, bool align) const
{
	GSVector4i _r;

	const GSVector2i& src = GSLocalMemory::m_psm[psm].bs;

	if (psm != TEX0.PSM)
	{
		const GSVector2i& dst = GSLocalMemory::m_psm[TEX0.PSM].bs;
		_r.left = (r.left * dst.x) / src.x;
		_r.top = (r.top * dst.y) / src.y;
		_r.right = (r.right * dst.x) / src.x;
		_r.bottom = (r.bottom * dst.y) / src.y;
	}
	else
	{
		_r = r;
	}

	return align ? _r.ralign<Align_Outside>(src) : _r;
}

GSVector4i GSDirtyRectList::GetTotalRect(GIFRegTEX0 TEX0, const GSVector2i& size) const
{
	if (!empty())
	{
		GSVector4i r = GSVector4i::cxpr(INT_MAX, INT_MAX, 0, 0);

		for (auto& dirty_rect : *this)
		{
			r = r.runion(dirty_rect.GetDirtyRect(TEX0, true));
		}

		const GSVector2i& bs = GSLocalMemory::m_psm[TEX0.PSM].bs;

		return r.ralign<Align_Outside>(bs).rintersect(GSVector4i::loadh(size));
	}

	return GSVector4i::zero();
}

u32 GSDirtyRectList::GetDirtyChannels()
{
	u32 channels = 0;

	if (!empty())
	{
		for (auto& dirty_rect : *this)
		{
			channels |= dirty_rect.rgba._u32;
		}
	}

	return channels;
}

GSVector4i GSDirtyRectList::GetDirtyRect(size_t index, GIFRegTEX0 TEX0, const GSVector4i& clamp, bool align) const
{
	GSVector4i r = (*this)[index].GetDirtyRect(TEX0, align);
	const GSVector2i& bs = GSLocalMemory::m_psm[TEX0.PSM].bs;
	if (align)
		r = r.ralign<Align_Outside>(bs);

	return r.rintersect(clamp);
}

