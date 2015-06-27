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

#pragma once

#include "GS.h"
#include "GSLocalMemory.h"

__aligned(class, 32) GSDrawingContext
{
public:
	GIFRegXYOFFSET	XYOFFSET;
	GIFRegTEX0		TEX0;
	GIFRegTEX1		TEX1;
	GIFRegTEX2		TEX2;
	GIFRegCLAMP		CLAMP;
	GIFRegMIPTBP1	MIPTBP1;
	GIFRegMIPTBP2	MIPTBP2;
	GIFRegSCISSOR	SCISSOR;
	GIFRegALPHA		ALPHA;
	GIFRegTEST		TEST;
	GIFRegFBA		FBA;
	GIFRegFRAME		FRAME;
	GIFRegZBUF		ZBUF;

	struct
	{
		GSVector4 in;
		GSVector4i ex;
		GSVector4 ofex;
		GSVector4i ofxy;
	} scissor;

	struct
	{
		GSOffset* fb;
		GSOffset* zb;
		GSOffset* tex;
		GSPixelOffset* fzb;
		GSPixelOffset4* fzb4;
	} offset;

	GSDrawingContext()
	{
		memset(&offset, 0, sizeof(offset));

		Reset();
	}

	void Reset()
	{
		memset(&XYOFFSET, 0, sizeof(XYOFFSET));
		memset(&TEX0, 0, sizeof(TEX0));
		memset(&TEX1, 0, sizeof(TEX1));
		memset(&TEX2, 0, sizeof(TEX2));
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

		scissor.ex.u16[0] = (uint16)((SCISSOR.SCAX0 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.u16[1] = (uint16)((SCISSOR.SCAY0 << 4) + XYOFFSET.OFY - 0x8000);
		scissor.ex.u16[2] = (uint16)((SCISSOR.SCAX1 << 4) + XYOFFSET.OFX - 0x8000);
		scissor.ex.u16[3] = (uint16)((SCISSOR.SCAY1 << 4) + XYOFFSET.OFY - 0x8000);

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
		if(TEST.ATE && TEST.ATST == ATST_NEVER && TEST.AFAIL != AFAIL_ZB_ONLY) // alpha test, all pixels fail, z buffer is not updated
		{
			return false;
		}

		return ZBUF.ZMSK == 0 && TEST.ZTE != 0; // ZTE == 0 is bug on the real hardware, write is blocked then
	}

	void Dump(const std::string& filename)
	{
		// Append on purpose so env + context are merged into a single file
		FILE* fp = fopen(filename.c_str(), "at");
		if (!fp) return;

		fprintf(fp, "XYOFFSET\n"
				"\tX:%d\n"
				"\tY:%d\n\n"
				, XYOFFSET.OFX, XYOFFSET.OFY);

		fprintf(fp, "TEX0\n"
				"\tTBP0:0x%x\n"
				"\tTBW:%d\n"
				"\tPSM:0x%x\n"
				"\tTW:%d\n"
				"\tTCC:%d\n"
				"\tTFX:%d\n"
				"\tCBP:0x%x\n"
				"\tCPSM:0x%x\n"
				"\tCSM:%d\n"
				"\tCSA:%d\n"
				"\tCLD:%d\n"
				"\tTH:%lld\n\n"
				, TEX0.TBP0, TEX0.TBW, TEX0.PSM, TEX0.TW, TEX0.TCC, TEX0.TFX, TEX0.CBP, TEX0.CPSM, TEX0.CSM, TEX0.CSA, TEX0.CLD, TEX0.TH);

		fprintf(fp, "TEX1\n"
				"\tLCM:%d\n"
				"\tMXL:%d\n"
				"\tMMAG:%d\n"
				"\tMMIN:%d\n"
				"\tMTBA:%d\n"
				"\tL:%d\n"
				"\tK:%d\n\n"
				, TEX1.LCM, TEX1.MXL, TEX1.MMAG, TEX1.MMIN, TEX1.MTBA, TEX1.L, TEX1.K);

		fprintf(fp, "TEX2\n"
				"\tPSM:0x%x\n"
				"\tCBP:0x%x\n"
				"\tCPSM:0x%x\n"
				"\tCSM:%d\n"
				"\tCSA:%d\n"
				"\tCLD:%d\n\n"
				, TEX2.PSM, TEX2.CBP, TEX2.CPSM, TEX2.CSM, TEX2.CSA, TEX2.CLD);

		fprintf(fp, "CLAMP\n"
				"\tWMS:%d\n"
				"\tWMT:%d\n"
				"\tMINU:%d\n"
				"\tMAXU:%d\n"
				"\tMAXV:%d\n"
				"\tMINV:%lld\n\n"
				, CLAMP.WMS, CLAMP.WMT, CLAMP.MINU, CLAMP.MAXU, CLAMP.MAXV, CLAMP.MINV);

		// TODO mimmap? (yes I'm lazy)
		fprintf(fp, "SCISSOR\n"
				"\tX0:%d\n"
				"\tX1:%d\n"
				"\tY0:%d\n"
				"\tY1:%d\n\n"
				, SCISSOR.SCAX0, SCISSOR.SCAX1, SCISSOR.SCAY0, SCISSOR.SCAY1);

		fprintf(fp, "ALPHA\n"
				"\tA:%d\n"
				"\tB:%d\n"
				"\tC:%d\n"
				"\tD:%d\n"
				"\tFIX:%d\n"
				, ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D, ALPHA.FIX);
		const char *col[3] = {"Cs", "Cd", "0"};
		const char *alpha[3] = {"As", "Ad", "Af"};
		fprintf(fp, "\t=> (%s - %s) * %s + %s\n\n", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);

		fprintf(fp, "TEST\n"
				"\tATE:%d\n"
				"\tATST:%d\n"
				"\tAREF:%d\n"
				"\tAFAIL:%d\n"
				"\tDATE:%d\n"
				"\tDATM:%d\n"
				"\tZTE:%d\n"
				"\tZTST:%d\n\n"
				, TEST.ATE, TEST.ATST, TEST.AREF, TEST.AFAIL, TEST.DATE, TEST.DATM, TEST.ZTE, TEST.ZTST);

		fprintf(fp, "FBA\n"
				"\tFBA:%d\n\n"
				, FBA.FBA);

		fprintf(fp, "FRAME\n"
				"\tFBP (*32):0x%x\n"
				"\tFBW:%d\n"
				"\tPSM:0x%x\n"
				"\tFBMSK:0x%x\n\n"
				, FRAME.FBP*32, FRAME.FBW, FRAME.PSM, FRAME.FBMSK);

		fprintf(fp, "ZBUF\n"
				"\tZBP (*32):0x%x\n"
				"\tPSM:0x%x\n"
				"\tZMSK:%d\n\n"
				, ZBUF.ZBP*32, ZBUF.PSM, ZBUF.ZMSK);

		fclose(fp);
	}
};
