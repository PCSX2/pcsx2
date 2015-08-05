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
#include "GSDrawingContext.h"
#include "GSdx.h"

static int clampuv(int uv, int wm, int minuv, int maxuv)
{
	if(wm == CLAMP_REGION_CLAMP)
	{
		if(uv < minuv) uv = minuv;
		if(uv > maxuv + 1) uv = maxuv + 1;
	}
	else if(wm == CLAMP_REGION_REPEAT)
	{
		uv = maxuv + (minuv + 1); // cannot determine safely, just use offset + mask + 1
	}

	return uv;
}

static int reduce(int uv, int size)
{
	while(size > 3 && (1 << (size - 1)) >= uv)
	{
		size--;
	}

	return size;
}

static int extend(int uv, int size)
{
	while(size < 10 && (1 << size) < uv)
	{
		size++;
	}

	return size;
}

GIFRegTEX0 GSDrawingContext::GetSizeFixedTEX0(const GSVector4i& uvmax, bool mipmap)
{
	if(mipmap) return TEX0; // no mipmaping allowed

	// find the optimal value for TW/TH by analyzing vertex trace and clamping values, extending only for region modes where uv may be outside

	int tw = TEX0.TW;
	int th = TEX0.TH;

	int wms = (int)CLAMP.WMS;
	int wmt = (int)CLAMP.WMT;

	int minu = (int)CLAMP.MINU;
	int minv = (int)CLAMP.MINV;
	int maxu = (int)CLAMP.MAXU;
	int maxv = (int)CLAMP.MAXV;

	GSVector4i uv = uvmax;

	uv.x = clampuv(uv.x, wms, minu, maxu);
	uv.y = clampuv(uv.y, wmt, minv, maxv);

	if(tw + th >= 19) // smaller sizes aren't worth, they just create multiple entries in the textue cache and the saved memory is less
	{
		tw = reduce(uv.x, tw);
		th = reduce(uv.y, th);
	}

	if(wms == CLAMP_REGION_CLAMP || wms == CLAMP_REGION_REPEAT)
	{
		tw = extend(uv.x, tw);
	}

	if(wmt == CLAMP_REGION_CLAMP || wmt == CLAMP_REGION_REPEAT)
	{
		th = extend(uv.y, th);
	}

#ifdef _DEBUG
	if(TEX0.TW != tw || TEX0.TH != th)
	{
		printf("FixedTEX0 %05x %d %d tw %d=>%d th %d=>%d uvmax %d,%d wm %d,%d (%d,%d,%d,%d)\n",
			(int)TEX0.TBP0, (int)TEX0.TBW, (int)TEX0.PSM,
			(int)TEX0.TW, tw, (int)TEX0.TH, th,
			uv.x, uv.y,
			wms, wmt, minu, maxu, minv, maxv);
	}
#endif

	GIFRegTEX0 res = TEX0;

	res.TW = tw;
	res.TH = th;

	return res;
}
