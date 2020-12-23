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

GSDirtyRect::GSDirtyRect() :
	r(GSVector4i::zero()),
	psm(PSM_PSMCT32),
	bw(1)
{
}

GSDirtyRect::GSDirtyRect(const GSVector4i& r, const u32 psm, const u32 bw) :
	r(r),
	psm(psm),
	bw(bw)
{
}

const GSVector4i GSDirtyRect::GetDirtyRect(const GIFRegTEX0& TEX0) const
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

//

const GSVector4i GSDirtyRectList::GetDirtyRect(const GIFRegTEX0& TEX0, const GSVector2i& size) const
{
	if (!empty())
	{
		GSVector4i r(INT_MAX, INT_MAX, 0, 0);

		for (const auto& dirty_rect : *this)
		{
			r = r.runion(dirty_rect.GetDirtyRect(TEX0));
		}

		const GSVector2i bs = GSLocalMemory::m_psm[TEX0.PSM].bs;

		return r.ralign<Align_Outside>(bs).rintersect(GSVector4i(0, 0, size.x, size.y));
	}

	return GSVector4i::zero();
}

const GSVector4i GSDirtyRectList::GetDirtyRectAndClear(const GIFRegTEX0& TEX0, const GSVector2i& size)
{
	const GSVector4i r = GetDirtyRect(TEX0, size);
	clear();
	return r;
}
