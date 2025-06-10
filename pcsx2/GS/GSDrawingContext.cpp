// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GSDrawingContext.h"
#include "GS/GSGL.h"
#include "GS/GS.h"
#include "GS/GSUtil.h"
#include "GS/GSState.h"

// SIZE: TW or TW
// WM, MIN, MAX : Correspondng field of TEX0
// min, max: Range that U or V coordintes take on.
static int GetMaxUV(int SIZE, int WM, int MIN, int MAX, int min, int max)
{
	// Confirmed on hardware if SIZE > 10 (or pixel size > 1024),
	// it basically gets masked so you end up with a 1x1 pixel (Except Region Clamp).
	if (SIZE > 10 && (WM != CLAMP_REGION_CLAMP))
		return 0;

	int min_out, max_out; // ignore min_out
	bool min_boundary, max_boundary; // ignore both
	
	GSState::GetClampWrapMinMaxUV(SIZE, WM, MIN, MAX, min, max, &min_out, &max_out, &min_boundary, &max_boundary);

	return max_out;
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

// Find the optimal value for TW/TH by analyzing vertex trace and clamping values,
// extending only for region modes where uv may be outside.
// uv_rect has rectangle bounding effecive UV coordinate (u0, v0, u1, v1) (u1 v1 endpoints exclusive)
GIFRegTEX0 GSDrawingContext::GetSizeFixedTEX0(GSVector4i uv_rect, bool linear, bool mipmap) const
{
	if (mipmap)
		return TEX0; // no mipmaping allowed

	const int WMS = (int)CLAMP.WMS;
	const int WMT = (int)CLAMP.WMT;

	const int MINU = (int)CLAMP.MINU;
	const int MINV = (int)CLAMP.MINV;
	const int MAXU = (int)CLAMP.MAXU;
	const int MAXV = (int)CLAMP.MAXV;

	int TW = TEX0.TW;
	int TH = TEX0.TH;

	const int min_width = uv_rect.right;
	const int min_height = uv_rect.bottom;

	auto ExtendLog2Size = [](int min_size, int log2_size) {
		while (log2_size < 10 && (1 << log2_size) < min_size)
			log2_size++;
		return log2_size;
	};

	auto ReduceLog2Size = [](int min_size, int log2_size) {
		while (log2_size > 3 && (1 << (log2_size - 1)) >= min_size)
			log2_size--;
		return log2_size;
	};

	if (TW + TH >= 19) // smaller sizes aren't worth, they just create multiple entries in the textue cache and the saved memory is less
	{
		TW = ReduceLog2Size(min_width, TW);
		TH = ReduceLog2Size(min_height, TH);
	}

	if (WMS == CLAMP_REGION_CLAMP || WMS == CLAMP_REGION_REPEAT)
	{
		TW = ExtendLog2Size(min_width, TW);
	}

	if (WMT == CLAMP_REGION_CLAMP || WMT == CLAMP_REGION_REPEAT)
	{
		TH = ExtendLog2Size(min_height, TH);
	}

	GIFRegTEX0 res = TEX0;

	res.TW = TW > 10 ? 0 : TW;
	res.TH = TH > 10 ? 0 : TH;

	if (TEX0.TW != res.TW || TEX0.TH != res.TH)
	{
		GL_DBG("FixedTEX0 %05x %d %d tw %d=>%d th %d=>%d uv (%d,%d,%d,%d) uvmax %d,%d wm %d,%d (%d,%d,%d,%d)",
			(int)TEX0.TBP0, (int)TEX0.TBW, (int)TEX0.PSM,
			(int)TEX0.TW, TW, (int)TEX0.TH, TH,
			uv_rect.left, uv_rect.top, uv_rect.right, uv_rect.bottom,
			min_width - 1, min_height - 1,
			WMS, WMT, MINU, MAXU, MINV, MAXV);
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
		"\tTH:%u\n"
		"\tTCC:%u\n"
		"\tTFX:%u\n"
		"\tCBP:0x%x\n"
		"\tCPSM:0x%x\n"
		"\tCSM:%u\n"
		"\tCSA:%u\n"
		"\tCLD:%u\n\n",
		TEX0.TBP0, TEX0.TBW, TEX0.PSM, TEX0.TW, static_cast<uint32_t>(TEX0.TH), TEX0.TCC, TEX0.TFX, TEX0.CBP, TEX0.CPSM, TEX0.CSM, TEX0.CSA, TEX0.CLD);

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
		"\tATST:%s\n"
		"\tAREF:%u\n"
		"\tAFAIL:%s\n"
		"\tDATE:%u\n"
		"\tDATM:%u\n"
		"\tZTE:%u\n"
		"\tZTST:%u\n\n",
		TEST.ATE, GSUtil::GetATSTName(TEST.ATST), TEST.AREF, GSUtil::GetAFAILName(TEST.AFAIL), TEST.DATE, TEST.DATM, TEST.ZTE, TEST.ZTST);

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
