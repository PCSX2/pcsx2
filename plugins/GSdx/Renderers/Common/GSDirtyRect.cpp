/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSDirtyRect.h"

GSDirtyRect::GSDirtyRect() :
	psm(PSM_PSMCT32),
	bw(1)
{
}

GSDirtyRect::GSDirtyRect(const GSVector4i& r, const uint32 psm, const uint32 bw) :
	r(r),
	psm(psm),
	bw(bw)
{
}

const GSVector4i GSDirtyRect::GetDirtyRect(const GIFRegTEX0& TEX0) const
{
	GSVector4i _r;

	const GSVector2i src = GSLocalMemory::m_psm[psm].bs;

	if(psm != TEX0.PSM)
	{
		const GSVector2i dst = GSLocalMemory::m_psm[TEX0.PSM].bs;
		const float ratio_x = static_cast<float>(dst.x) / src.x;
		const float ratio_y = static_cast<float>(dst.y) / src.y;
		_r.left = static_cast<int>(r.left * ratio_x);
		_r.top = static_cast<int>(r.top * ratio_y);
		_r.right = static_cast<int>(r.right * ratio_x);
		_r.bottom = static_cast<int>(r.bottom * ratio_y);
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
