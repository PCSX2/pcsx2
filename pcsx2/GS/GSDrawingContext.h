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

#pragma once
#ifdef __clang__
// Ignore format for this file, as it spams a lot of warnings about u64 and %llu.
#pragma clang diagnostic ignored "-Wformat"
#endif

#include "GSLocalMemory.h"

class alignas(32) GSDrawingContext
{
public:
	GIFRegXYOFFSET XYOFFSET;
	GIFRegTEX0     TEX0;
	GIFRegTEX1     TEX1;
	GIFRegCLAMP    CLAMP;
	GIFRegMIPTBP1  MIPTBP1;
	GIFRegMIPTBP2  MIPTBP2;
	GIFRegSCISSOR  SCISSOR;
	GIFRegALPHA    ALPHA;
	GIFRegTEST     TEST;
	GIFRegFBA      FBA;
	GIFRegFRAME    FRAME;
	GIFRegZBUF     ZBUF;

	struct
	{
		GSVector4 in;
		GSVector4i ex;
		GSVector4 ofex;
		GSVector4i ofxy;
	} scissor;

	struct
	{
		GSOffset fb;
		GSOffset zb;
		GSOffset tex;
		GSPixelOffset* fzb;
		GSPixelOffset4* fzb4;
	} offset;

	struct
	{
		GIFRegXYOFFSET XYOFFSET;
		GIFRegTEX0     TEX0;
		GIFRegTEX1     TEX1;
		GIFRegCLAMP    CLAMP;
		GIFRegMIPTBP1  MIPTBP1;
		GIFRegMIPTBP2  MIPTBP2;
		GIFRegSCISSOR  SCISSOR;
		GIFRegALPHA    ALPHA;
		GIFRegTEST     TEST;
		GIFRegFBA      FBA;
		GIFRegFRAME    FRAME;
		GIFRegZBUF     ZBUF;
	} stack;

	bool m_fixed_tex0;

	GSDrawingContext()
	{
		m_fixed_tex0 = false;

		memset(&offset, 0, sizeof(offset));

		Reset();
	}

	void Reset()
	{
		memset(&XYOFFSET, 0, sizeof(XYOFFSET));
		memset(&TEX0, 0, sizeof(TEX0));
		memset(&TEX1, 0, sizeof(TEX1));
		memset(&CLAMP, 0, sizeof(CLAMP));
		memset(&MIPTBP1, 0, sizeof(MIPTBP1));
		memset(&MIPTBP2, 0, sizeof(MIPTBP2));
		memset(&SCISSOR, 0, sizeof(SCISSOR));
		memset(&ALPHA, 0, sizeof(ALPHA));
		memset(&TEST, 0, sizeof(TEST));
		memset(&FBA, 0, sizeof(FBA));
		memset(&FRAME, 0, sizeof(FRAME));
		memset(&ZBUF, 0, sizeof(ZBUF));
	}

	void UpdateScissor()
	{
		ASSERT(XYOFFSET.OFX <= 0xf800 && XYOFFSET.OFY <= 0xf800);

		scissor.ex.U16[0] = (u16)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[1] = (u16)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY - 0x8000);
		scissor.ex.U16[2] = (u16)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.U16[3] = (u16)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY - 0x8000);

		scissor.ofex = GSVector4(
			(int)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX),
			(int)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY),
			(int)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX),
			(int)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY));

		scissor.in = GSVector4(
			(int)SCISSOR.SCAX0,
			(int)SCISSOR.SCAY0,
			(int)SCISSOR.SCAX1 + 1,
			(int)SCISSOR.SCAY1 + 1);

		scissor.ofxy = GSVector4i(
			0x8000,
			0x8000,
			(int)XYOFFSET.OFX - 15,
			(int)XYOFFSET.OFY - 15);
	}

	bool DepthRead() const
	{
		return TEST.ZTE && TEST.ZTST >= 2;
	}

	bool DepthWrite() const
	{
		if (TEST.ATE && TEST.ATST == ATST_NEVER && TEST.AFAIL != AFAIL_ZB_ONLY) // alpha test, all pixels fail, z buffer is not updated
		{
			return false;
		}

		return ZBUF.ZMSK == 0 && TEST.ZTE != 0; // ZTE == 0 is bug on the real hardware, write is blocked then
	}

	GIFRegTEX0 GetSizeFixedTEX0(const GSVector4& st, bool linear, bool mipmap = false);
	void ComputeFixedTEX0(const GSVector4& st);
	bool HasFixedTEX0() const { return m_fixed_tex0; }

	// Save & Restore before/after draw allow to correct/optimize current register for current draw
	// Note: we could avoid the restore part if all renderer code is updated to use a local copy instead
	void SaveReg()
	{
		stack.XYOFFSET = XYOFFSET;
		stack.TEX0 = TEX0;
		stack.TEX1 = TEX1;
		stack.CLAMP = CLAMP;
		stack.MIPTBP1 = MIPTBP1;
		stack.MIPTBP2 = MIPTBP2;
		stack.SCISSOR = SCISSOR;
		stack.ALPHA = ALPHA;
		stack.TEST = TEST;
		stack.FBA = FBA;
		stack.FRAME = FRAME;
		stack.ZBUF = ZBUF;

		// This function is called before the draw so take opportunity to reset m_fixed_tex0
		m_fixed_tex0 = false;
	}

	void RestoreReg()
	{
		XYOFFSET = stack.XYOFFSET;
		TEX0 = stack.TEX0;
		TEX1 = stack.TEX1;
		CLAMP = stack.CLAMP;
		MIPTBP1 = stack.MIPTBP1;
		MIPTBP2 = stack.MIPTBP2;
		SCISSOR = stack.SCISSOR;
		ALPHA = stack.ALPHA;
		TEST = stack.TEST;
		FBA = stack.FBA;
		FRAME = stack.FRAME;
		ZBUF = stack.ZBUF;
	}

	void Dump(const std::string& filename)
	{
		// Append on purpose so env + context are merged into a single file
		FILE* fp = fopen(filename.c_str(), "at");
		if (!fp)
			return;

		fprintf(fp, "XYOFFSET\n"
		            "\tX:%u\n"
		            "\tY:%u\n\n"
		        , XYOFFSET.OFX, XYOFFSET.OFY);

		fprintf(fp, "MIPTBP1\n"
		            "\tBP1:0x%llx\n"
		            "\tBW1:%llu\n"
		            "\tBP2:0x%llx\n"
		            "\tBW2:%llu\n"
		            "\tBP3:0x%llx\n"
		            "\tBW3:%llu\n\n"
		        , MIPTBP1.TBP1, MIPTBP1.TBW1, MIPTBP1.TBP2, MIPTBP1.TBW2, MIPTBP1.TBP3, MIPTBP1.TBW3);

		fprintf(fp, "MIPTBP2\n"
		            "\tBP4:0x%llx\n"
		            "\tBW4:%llu\n"
		            "\tBP5:0x%llx\n"
		            "\tBW5:%llu\n"
		            "\tBP6:0x%llx\n"
		            "\tBW6:%llu\n\n"
		        , MIPTBP2.TBP4, MIPTBP2.TBW4, MIPTBP2.TBP5, MIPTBP2.TBW5, MIPTBP2.TBP6, MIPTBP2.TBW6);

		fprintf(fp, "TEX0\n"
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
		            "\tTH:%llu\n\n"
		        , TEX0.TBP0, TEX0.TBW, TEX0.PSM, TEX0.TW, TEX0.TCC, TEX0.TFX, TEX0.CBP, TEX0.CPSM, TEX0.CSM, TEX0.CSA, TEX0.CLD, TEX0.TH);

		fprintf(fp, "TEX1\n"
		            "\tLCM:%u\n"
		            "\tMXL:%u\n"
		            "\tMMAG:%u\n"
		            "\tMMIN:%u\n"
		            "\tMTBA:%u\n"
		            "\tL:%u\n"
		            "\tK:%d\n\n"
		        , TEX1.LCM, TEX1.MXL, TEX1.MMAG, TEX1.MMIN, TEX1.MTBA, TEX1.L, TEX1.K);

		fprintf(fp, "CLAMP\n"
		            "\tWMS:%u\n"
		            "\tWMT:%u\n"
		            "\tMINU:%u\n"
		            "\tMAXU:%u\n"
		            "\tMAXV:%u\n"
		            "\tMINV:%llu\n\n"
		        , CLAMP.WMS, CLAMP.WMT, CLAMP.MINU, CLAMP.MAXU, CLAMP.MAXV, CLAMP.MINV);

		// TODO mimmap? (yes I'm lazy)
		fprintf(fp, "SCISSOR\n"
		            "\tX0:%u\n"
		            "\tX1:%u\n"
		            "\tY0:%u\n"
		            "\tY1:%u\n\n"
		        , SCISSOR.SCAX0, SCISSOR.SCAX1, SCISSOR.SCAY0, SCISSOR.SCAY1);

		fprintf(fp, "ALPHA\n"
		            "\tA:%u\n"
		            "\tB:%u\n"
		            "\tC:%u\n"
		            "\tD:%u\n"
		            "\tFIX:%u\n"
		        , ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D, ALPHA.FIX);
		const char* col[3] = {"Cs", "Cd", "0"};
		const char* alpha[3] = {"As", "Ad", "Af"};
		fprintf(fp, "\t=> (%s - %s) * %s + %s\n\n", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);

		fprintf(fp, "TEST\n"
		            "\tATE:%u\n"
		            "\tATST:%u\n"
		            "\tAREF:%u\n"
		            "\tAFAIL:%u\n"
		            "\tDATE:%u\n"
		            "\tDATM:%u\n"
		            "\tZTE:%u\n"
		            "\tZTST:%u\n\n"
		        , TEST.ATE, TEST.ATST, TEST.AREF, TEST.AFAIL, TEST.DATE, TEST.DATM, TEST.ZTE, TEST.ZTST);

		fprintf(fp, "FBA\n"
		            "\tFBA:%u\n\n"
		        , FBA.FBA);

		fprintf(fp, "FRAME\n"
		            "\tFBP (*32):0x%x\n"
		            "\tFBW:%u\n"
		            "\tPSM:0x%x\n"
		            "\tFBMSK:0x%x\n\n"
		        , FRAME.FBP * 32, FRAME.FBW, FRAME.PSM, FRAME.FBMSK);

		fprintf(fp, "ZBUF\n"
		            "\tZBP (*32):0x%x\n"
		            "\tPSM:0x%x\n"
		            "\tZMSK:%u\n\n"
		        , ZBUF.ZBP * 32, ZBUF.PSM, ZBUF.ZMSK);

		fclose(fp);
	}
};
