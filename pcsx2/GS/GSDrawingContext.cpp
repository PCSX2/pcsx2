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
	// return max possible texcoord.
	int uv = br;

	// Confirmed on hardware if the size exceeds 1024, it basically gets masked so you end up with a 1x1 pixel (Except Region Clamp).
	if (limit > 1024)
		limit = 0;

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
		// REGION_REPEAT adhears to the original texture size, even if offset outside the texture (with MAXUV).
		minuv &= limit;
		if (tl < 0)
			uv = minuv | maxuv; // wrap around, just use (any & mask) | fix.
		else
			uv = std::min(uv, minuv) | maxuv; // (any & mask) cannot be larger than mask, select br if that is smaller (not br & mask because there might be a larger value between tl and br when &'ed with the mask).
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

GSDrawingContext::GSDrawingContext()
{
	std::memset(&offset, 0, sizeof(offset));

	Reset();
}

void GSDrawingContext::Reset()
{
	std::memset(&XYOFFSET, 0, sizeof(XYOFFSET));
	std::memset(&TEX0, 0, sizeof(TEX0));
	std::memset(&TEX1, 0, sizeof(TEX1));
	std::memset(&CLAMP, 0, sizeof(CLAMP));
	std::memset(&MIPTBP1, 0, sizeof(MIPTBP1));
	std::memset(&MIPTBP2, 0, sizeof(MIPTBP2));
	std::memset(&SCISSOR, 0, sizeof(SCISSOR));
	std::memset(&ALPHA, 0, sizeof(ALPHA));
	std::memset(&TEST, 0, sizeof(TEST));
	std::memset(&FBA, 0, sizeof(FBA));
	std::memset(&FRAME, 0, sizeof(FRAME));
	std::memset(&ZBUF, 0, sizeof(ZBUF));
}

void GSDrawingContext::UpdateScissor()
{
	// Scissor registers are inclusive of the upper bounds.
	const GSVector4i rscissor = GSVector4i(static_cast<int>(SCISSOR.SCAX0), static_cast<int>(SCISSOR.SCAY0),
		static_cast<int>(SCISSOR.SCAX1), static_cast<int>(SCISSOR.SCAY1));
	scissor.in = rscissor + GSVector4i::cxpr(0, 0, 1, 1);

	// Fixed-point scissor min/max, used for rejecting primitives which are entirely outside.
	scissor.cull = rscissor.sll32(4);

	// Offset applied to vertices for culling, zw is for native resolution culling
	// We want to round subpixels down, because at least one pixel gets filled per scanline.
	scissor.xyof = GSVector4i::loadl(&XYOFFSET.U64).xyxy().sub32(GSVector4i::cxpr(0, 0, 15, 15));
}

GIFRegTEX0 GSDrawingContext::GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap) const
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

	GIFRegTEX0 res = TEX0;

	res.TW = tw > 10 ? 0 : tw;
	res.TH = th > 10 ? 0 : th;

	if (TEX0.TW != res.TW || TEX0.TH != res.TH)
	{
		GL_DBG("FixedTEX0 %05x %d %d tw %d=>%d th %d=>%d st (%.0f,%.0f,%.0f,%.0f) uvmax %d,%d wm %d,%d (%d,%d,%d,%d)",
			(int)TEX0.TBP0, (int)TEX0.TBW, (int)TEX0.PSM,
			(int)TEX0.TW, tw, (int)TEX0.TH, th,
			uvf.x, uvf.y, uvf.z, uvf.w,
			uv.x, uv.y,
			wms, wmt, minu, maxu, minv, maxv);
	}

	return res;
}

void GSDrawingContext::Dump(const std::string& filename)
{
	// Append on purpose so env + context are merged into a single file
	FILE* fp = fopen(filename.c_str(), "at");
	if (!fp)
		return;

	fprintf(fp,
		"XYOFFSET\n"
		"\tX:%u\n"
		"\tY:%u\n\n",
		XYOFFSET.OFX, XYOFFSET.OFY);

	fprintf(fp,
		"MIPTBP1\n"
		"\tBP1:0x%x\n"
		"\tBW1:%u\n"
		"\tBP2:0x%x\n"
		"\tBW2:%u\n"
		"\tBP3:0x%x\n"
		"\tBW3:%u\n\n",
		static_cast<uint32_t>(MIPTBP1.TBP1), static_cast<uint32_t>(MIPTBP1.TBW1), static_cast<uint32_t>(MIPTBP1.TBP2),
		static_cast<uint32_t>(MIPTBP1.TBW2), static_cast<uint32_t>(MIPTBP1.TBP3), static_cast<uint32_t>(MIPTBP1.TBW3));

	fprintf(fp,
		"MIPTBP2\n"
		"\tBP4:0x%x\n"
		"\tBW4:%u\n"
		"\tBP5:0x%x\n"
		"\tBW5:%u\n"
		"\tBP6:0x%x\n"
		"\tBW6:%u\n\n",
		static_cast<uint32_t>(MIPTBP2.TBP4), static_cast<uint32_t>(MIPTBP2.TBW4), static_cast<uint32_t>(MIPTBP2.TBP5),
		static_cast<uint32_t>(MIPTBP2.TBW5), static_cast<uint32_t>(MIPTBP2.TBP6), static_cast<uint32_t>(MIPTBP2.TBW6));

	fprintf(fp,
		"TEX0\n"
		"\tTBP0:0x%x\n"
		"\tTBW:%u\n"
		"\tPSM:0x%x\n"
		"\tTW:%u\n"
		"\tTCC:%u\n"
		"\tTFX:%u\n"
		"\tCBP:0x%x\n"
		"\tCPSM:0x%x\n"
		"\tCSM:%u\n"
		"\tCSA:%u\n"
		"\tCLD:%u\n"
		"\tTH:%u\n",
		TEX0.TBP0, TEX0.TBW, TEX0.PSM, TEX0.TW, TEX0.TCC, TEX0.TFX, TEX0.CBP, TEX0.CPSM, TEX0.CSM, TEX0.CSA, TEX0.CLD,
		static_cast<uint32_t>(TEX0.TH));

	fprintf(fp,
		"TEX1\n"
		"\tLCM:%u\n"
		"\tMXL:%u\n"
		"\tMMAG:%u\n"
		"\tMMIN:%u\n"
		"\tMTBA:%u\n"
		"\tL:%u\n"
		"\tK:%d\n\n",
		TEX1.LCM, TEX1.MXL, TEX1.MMAG, TEX1.MMIN, TEX1.MTBA, TEX1.L, TEX1.K);

	fprintf(fp,
		"CLAMP\n"
		"\tWMS:%u\n"
		"\tWMT:%u\n"
		"\tMINU:%u\n"
		"\tMAXU:%u\n"
		"\tMAXV:%u\n"
		"\tMINV:%u\n\n",
		CLAMP.WMS, CLAMP.WMT, CLAMP.MINU, CLAMP.MAXU, CLAMP.MAXV, static_cast<uint32_t>(CLAMP.MINV));

	// TODO mimmap? (yes I'm lazy)
	fprintf(fp,
		"SCISSOR\n"
		"\tX0:%u\n"
		"\tX1:%u\n"
		"\tY0:%u\n"
		"\tY1:%u\n\n",
		SCISSOR.SCAX0, SCISSOR.SCAX1, SCISSOR.SCAY0, SCISSOR.SCAY1);

	fprintf(fp,
		"ALPHA\n"
		"\tA:%u\n"
		"\tB:%u\n"
		"\tC:%u\n"
		"\tD:%u\n"
		"\tFIX:%u\n",
		ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D, ALPHA.FIX);
	const char* col[3] = {"Cs", "Cd", "0"};
	const char* alpha[3] = {"As", "Ad", "Af"};
	fprintf(fp, "\t=> (%s - %s) * %s + %s\n\n", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);

	fprintf(fp,
		"TEST\n"
		"\tATE:%u\n"
		"\tATST:%u\n"
		"\tAREF:%u\n"
		"\tAFAIL:%u\n"
		"\tDATE:%u\n"
		"\tDATM:%u\n"
		"\tZTE:%u\n"
		"\tZTST:%u\n\n",
		TEST.ATE, TEST.ATST, TEST.AREF, TEST.AFAIL, TEST.DATE, TEST.DATM, TEST.ZTE, TEST.ZTST);

	fprintf(fp,
		"FBA\n"
		"\tFBA:%u\n\n",
		FBA.FBA);

	fprintf(fp,
		"FRAME\n"
		"\tFBP (*32):0x%x\n"
		"\tFBW:%u\n"
		"\tPSM:0x%x\n"
		"\tFBMSK:0x%x\n\n",
		FRAME.FBP * 32, FRAME.FBW, FRAME.PSM, FRAME.FBMSK);

	fprintf(fp,
		"ZBUF\n"
		"\tZBP (*32):0x%x\n"
		"\tPSM:0x%x\n"
		"\tZMSK:%u\n\n",
		ZBUF.ZBP * 32, ZBUF.PSM, ZBUF.ZMSK);

	fclose(fp);
}
