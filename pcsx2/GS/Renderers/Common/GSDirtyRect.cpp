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

GSVector4i GSDirtyRect::GetDirtyRect(GIFRegTEX0 TEX0) const
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
		_r = _r.ralign<Align_Outside>(src);
	}
	else
	{
		_r = r.ralign<Align_Outside>(src);
	}

	return _r;
}

GSVector4i GSDirtyRectList::GetTotalRect(GIFRegTEX0 TEX0, const GSVector2i& size) const
{
	if (!empty())
	{
		GSVector4i r = GSVector4i::cxpr(INT_MAX, INT_MAX, 0, 0);

		for (auto& dirty_rect : *this)
		{
			r = r.runion(dirty_rect.GetDirtyRect(TEX0));
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

GSVector4i GSDirtyRectList::GetDirtyRect(size_t index, GIFRegTEX0 TEX0, const GSVector4i& clamp) const
{
	const GSVector4i r = (*this)[index].GetDirtyRect(TEX0);

	GSVector2i bs = GSLocalMemory::m_psm[TEX0.PSM].bs;

	return r.ralign<Align_Outside>(bs).rintersect(clamp);
}

