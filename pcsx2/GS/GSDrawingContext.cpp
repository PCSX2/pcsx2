// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GSDrawingContext.h"
#include "GS/GSGL.h"
#include "GS/GS.h"
#include "GS/GSUtil.h"

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
	scissor.cull = rscissor.sll32<4>();

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

	// Warning: The indentation must be consistent with GSDrawingEnvironment::Dump().

	fprintf(fp,
		"XYOFFSET:\n"
		"    OFX: %.4f\n"
		"    OFY: %.4f\n\n",
		XYOFFSET.OFX / 16.0f, XYOFFSET.OFY / 16.0f);

	fprintf(fp,
		"MIPTBP1:\n"
		"    TBP1: 0x%x\n"
		"    TBW1: %u\n"
		"    TBP2: 0x%x\n"
		"    TBW2: %u\n"
		"    TBP3: 0x%x\n"
		"    TBW3: %u\n\n",
		static_cast<uint32_t>(MIPTBP1.TBP1), static_cast<uint32_t>(MIPTBP1.TBW1), static_cast<uint32_t>(MIPTBP1.TBP2),
		static_cast<uint32_t>(MIPTBP1.TBW2), static_cast<uint32_t>(MIPTBP1.TBP3), static_cast<uint32_t>(MIPTBP1.TBW3));

	fprintf(fp,
		"MIPTBP2:\n"
		"    TBP4: 0x%x\n"
		"    TBW4: %u\n"
		"    TBP5: 0x%x\n"
		"    TBW5: %u\n"
		"    TBP6: 0x%x\n"
		"    TBW6: %u\n\n",
		static_cast<uint32_t>(MIPTBP2.TBP4), static_cast<uint32_t>(MIPTBP2.TBW4), static_cast<uint32_t>(MIPTBP2.TBP5),
		static_cast<uint32_t>(MIPTBP2.TBW5), static_cast<uint32_t>(MIPTBP2.TBP6), static_cast<uint32_t>(MIPTBP2.TBW6));

	fprintf(fp,
		"TEX0:\n"
		"    TBP0: 0x%x\n"
		"    TBW: %u\n"
		"    PSM: 0x%x # %s\n"
		"    TW: %u\n"
		"    TH: %u\n"
		"    TCC: %u # %s\n"
		"    TFX: %u # %s\n"
		"    CBP: 0x%x\n"
		"    CPSM: 0x%x # %s\n"
		"    CSM: %u\n"
		"    CSA: %u\n"
		"    CLD: %u\n\n",
		TEX0.TBP0, TEX0.TBW, TEX0.PSM, GSUtil::GetPSMName(TEX0.PSM), TEX0.TW, static_cast<uint32_t>(TEX0.TH), TEX0.TCC, GSUtil::GetTCCName(TEX0.TCC), TEX0.TFX, GSUtil::GetTFXName(TEX0.TFX), TEX0.CBP, TEX0.CPSM, GSUtil::GetPSMName(TEX0.CPSM), TEX0.CSM, TEX0.CSA, TEX0.CLD);

	fprintf(fp,
		"TEX1:\n"
		"    LCM: %u # %s\n"
		"    MXL: %u\n"
		"    MMAG: %u # %s\n"
		"    MMIN: %u # %s\n"
		"    MTBA: %u\n"
		"    L: %u\n"
		"    K: %.4f\n\n",
		TEX1.LCM, GSUtil::GetLCMName(TEX1.LCM), TEX1.MXL, TEX1.MMAG, GSUtil::GetMMAGName(TEX1.MMAG), TEX1.MMIN, GSUtil::GetMMINName(TEX1.MMIN), TEX1.MTBA, TEX1.L, static_cast<float>((static_cast<int>(TEX1.K) ^ 0x800) - 0x800) / 16.0f);

	fprintf(fp,
		"CLAMP:\n"
		"    WMS: %u # %s\n"
		"    WMT: %u # %s\n"
		"    MINU: %u\n"
		"    MAXU: %u\n"
		"    MINV: %u\n"
		"    MAXV: %u\n\n",
		CLAMP.WMS, GSUtil::GetWMName(CLAMP.WMS), CLAMP.WMT,GSUtil::GetWMName(CLAMP.WMT), CLAMP.MINU, CLAMP.MAXU, static_cast<uint32_t>(CLAMP.MINV), CLAMP.MAXV);

	fprintf(fp,
		"SCISSOR:\n"
		"    SCAX0: %u\n"
		"    SCAX1: %u\n"
		"    SCAY0: %u\n"
		"    SCAY1: %u\n\n",
		SCISSOR.SCAX0, SCISSOR.SCAX1, SCISSOR.SCAY0, SCISSOR.SCAY1);

	fprintf(fp,
		"ALPHA:\n"
		"    A: %u\n"
		"    B: %u\n"
		"    C: %u\n"
		"    D: %u\n"
		"    FIX: %u\n",
		ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D, ALPHA.FIX);
	constexpr const char* col[3] = {"Cs", "Cd", "0"};
	constexpr const char* alpha[3] = {"As", "Ad", "Af"};
	fprintf(fp, "    # => (%s - %s) * %s + %s\n\n", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);

	fprintf(fp,
		"TEST:\n"
		"    ATE: %u\n"
		"    ATST: %u # %s\n"
		"    AREF: %u\n"
		"    AFAIL: %u # %s\n"
		"    DATE: %u\n"
		"    DATM: %u # %s\n"
		"    ZTE: %u\n"
		"    ZTST: %u # %s\n\n",
		TEST.ATE, TEST.ATST, GSUtil::GetATSTName(TEST.ATST), TEST.AREF, TEST.AFAIL, GSUtil::GetAFAILName(TEST.AFAIL), TEST.DATE, TEST.DATM, GSUtil::GetDATMName(TEST.DATM), TEST.ZTE, TEST.ZTST, GSUtil::GetZTSTName(TEST.ZTST));

	fprintf(fp,
		"FBA:\n"
		"    FBA: %u\n\n",
		FBA.FBA);

	fprintf(fp,
		"FRAME:\n"
		"    FBP: 0x%x # (*32)\n"
		"    FBW: %u\n"
		"    PSM: 0x%x # %s\n"
		"    FBMSK: 0x%x\n\n",
		FRAME.FBP * 32, FRAME.FBW, FRAME.PSM, GSUtil::GetPSMName(FRAME.PSM), FRAME.FBMSK);

	fprintf(fp,
		"ZBUF:\n"
		"    ZBP: 0x%x # (*32)\n"
		"    PSM: 0x%x # %s\n"
		"    ZMSK: %u\n\n",
		ZBUF.ZBP * 32, ZBUF.PSM, GSUtil::GetPSMName(ZBUF.PSM), ZBUF.ZMSK);

	fclose(fp);
}
