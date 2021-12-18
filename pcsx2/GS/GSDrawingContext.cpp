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
#include "GSDrawingContext.h"
#include "GSGL.h"
#include "GS.h"

static int findmax(int tl, int br, int limit, int wm, int minuv, int maxuv)
{
	// return max possible texcoord

	int uv = br;

	if (wm == CLAMP_CLAMP)
	{
		if (uv > limit)
			uv = limit;
	}
	else if (wm == CLAMP_REPEAT)
	{
		if (tl < 0)
			uv = limit; // wrap around
		else if (uv > limit)
			uv = limit;
	}
	else if (wm == CLAMP_REGION_CLAMP)
	{
		if (uv < minuv)
			uv = minuv;
		if (uv > maxuv)
			uv = maxuv;
	}
	else if (wm == CLAMP_REGION_REPEAT)
	{
		if (tl < 0)
			uv = minuv | maxuv; // wrap around, just use (any & mask) | fix
		else
			uv = std::min(uv, minuv) | maxuv; // (any & mask) cannot be larger than mask, select br if that is smaller (not br & mask because there might be a larger value between tl and br when &'ed with the mask)
	}

	return uv;
}

static int reduce(int uv, int size)
{
	while (size > 3 && (1 << (size - 1)) >= uv)
	{
		size--;
	}

	return size;
}

static int extend(int uv, int size)
{
	while (size < 10 && (1 << size) < uv)
	{
		size++;
	}

	return size;
}

GIFRegTEX0 GSDrawingContext::GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap)
{
	if (mipmap)
		return TEX0; // no mipmaping allowed

	// find the optimal value for TW/TH by analyzing vertex trace and clamping values, extending only for region modes where uv may be outside

	int tw = TEX0.TW;
	int th = TEX0.TH;

	int wms = (int)CLAMP.WMS;
	int wmt = (int)CLAMP.WMT;

	int minu = (int)CLAMP.MINU;
	int minv = (int)CLAMP.MINV;
	int maxu = (int)CLAMP.MAXU;
	int maxv = (int)CLAMP.MAXV;

	GSVector4 uvf = st;

	if (linear)
	{
		uvf += GSVector4(-0.5f, 0.5f).xxyy();
	}

	GSVector4i uv = GSVector4i(uvf.floor().xyzw(uvf.ceil()));

	uv.x = findmax(uv.x, uv.z, (1 << tw) - 1, wms, minu, maxu);
	uv.y = findmax(uv.y, uv.w, (1 << th) - 1, wmt, minv, maxv);

	if (tw + th >= 19) // smaller sizes aren't worth, they just create multiple entries in the textue cache and the saved memory is less
	{
		tw = reduce(uv.x, tw);
		th = reduce(uv.y, th);
	}

	if (wms == CLAMP_REGION_CLAMP || wms == CLAMP_REGION_REPEAT)
	{
		tw = extend(uv.x, tw);
	}

	if (wmt == CLAMP_REGION_CLAMP || wmt == CLAMP_REGION_REPEAT)
	{
		th = extend(uv.y, th);
	}

	if (GSConfig.Renderer == GSRendererType::SW && ((int)TEX0.TW != tw || (int)TEX0.TH != th))
	{
		GL_DBG("FixedTEX0 %05x %d %d tw %d=>%d th %d=>%d st (%.0f,%.0f,%.0f,%.0f) uvmax %d,%d wm %d,%d (%d,%d,%d,%d)",
			(int)TEX0.TBP0, (int)TEX0.TBW, (int)TEX0.PSM,
			(int)TEX0.TW, tw, (int)TEX0.TH, th,
			uvf.x, uvf.y, uvf.z, uvf.w,
			uv.x, uv.y,
			wms, wmt, minu, maxu, minv, maxv);
	}

	GIFRegTEX0 res = TEX0;

	res.TW = tw;
	res.TH = th;

	return res;
}

void GSDrawingContext::ComputeFixedTEX0(const GSVector4& st)
{
	// It is quite complex to handle rescaling so this function is less stricter than GetSizeFixedTEX0,
	// therefore we remove the reduce optimization and we don't handle bilinear filtering which might create wrong interpolation at the border.

	int tw = TEX0.TW;
	int th = TEX0.TH;

	int wms = (int)CLAMP.WMS;
	int wmt = (int)CLAMP.WMT;

	int minu = (int)CLAMP.MINU;
	int minv = (int)CLAMP.MINV;
	int maxu = (int)CLAMP.MAXU;
	int maxv = (int)CLAMP.MAXV;

	GSVector4i uv = GSVector4i(st.floor().xyzw(st.ceil()));

	uv.x = findmax(uv.x, uv.z, (1 << TEX0.TW) - 1, wms, minu, maxu);
	uv.y = findmax(uv.y, uv.w, (1 << TEX0.TH) - 1, wmt, minv, maxv);

	if (wms == CLAMP_REGION_CLAMP || wms == CLAMP_REGION_REPEAT)
		tw = extend(uv.x, tw);

	if (wmt == CLAMP_REGION_CLAMP || wmt == CLAMP_REGION_REPEAT)
		th = extend(uv.y, th);

	if ((tw != (int)TEX0.TW) || (th != (int)TEX0.TH))
	{
		m_fixed_tex0 = true;
		TEX0.TW = tw;
		TEX0.TH = th;

		GL_DBG("FixedTEX0 TW %d=>%d, TH %d=>%d wm %d,%d",
			(int)stack.TEX0.TW, (int)TEX0.TW, (int)stack.TEX0.TH, (int)TEX0.TH,
			(int)CLAMP.WMS, (int)CLAMP.WMT);
	}
}
