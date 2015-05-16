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

__aligned(class, 32) GSDrawingEnvironment
{
public:
	GIFRegPRIM			PRIM;
	GIFRegPRMODE		PRMODE;
	GIFRegPRMODECONT	PRMODECONT;
	GIFRegTEXCLUT		TEXCLUT;
	GIFRegSCANMSK		SCANMSK;
	GIFRegTEXA			TEXA;
	GIFRegFOGCOL		FOGCOL;
	GIFRegDIMX			DIMX;
	GIFRegDTHE			DTHE;
	GIFRegCOLCLAMP		COLCLAMP;
	GIFRegPABE			PABE;
	GIFRegBITBLTBUF		BITBLTBUF;
	GIFRegTRXDIR		TRXDIR;
	GIFRegTRXPOS		TRXPOS;
	GIFRegTRXREG		TRXREG;
	GSDrawingContext	CTXT[2];

	GSDrawingEnvironment()
	{
	}

	void Reset()
	{
		memset(&PRIM, 0, sizeof(PRIM));
		memset(&PRMODE, 0, sizeof(PRMODE));
		memset(&PRMODECONT, 0, sizeof(PRMODECONT));
		memset(&TEXCLUT, 0, sizeof(TEXCLUT));
		memset(&SCANMSK, 0, sizeof(SCANMSK));
		memset(&TEXA, 0, sizeof(TEXA));
		memset(&FOGCOL, 0, sizeof(FOGCOL));
		memset(&DIMX, 0, sizeof(DIMX));
		memset(&DTHE, 0, sizeof(DTHE));
		memset(&COLCLAMP, 0, sizeof(COLCLAMP));
		memset(&PABE, 0, sizeof(PABE));
		memset(&BITBLTBUF, 0, sizeof(BITBLTBUF));
		memset(&TRXDIR, 0, sizeof(TRXDIR));
		memset(&TRXPOS, 0, sizeof(TRXPOS));
		memset(&TRXREG, 0, sizeof(TRXREG));

		CTXT[0].Reset();
		CTXT[1].Reset();

		memset(dimx, 0, sizeof(dimx));
	}

	GSVector4i dimx[8];

	void UpdateDIMX()
	{
		dimx[1] = GSVector4i(DIMX.DM00, 0, DIMX.DM01, 0, DIMX.DM02, 0, DIMX.DM03, 0);
		dimx[0] = dimx[1].xxzzlh();
		dimx[3] = GSVector4i(DIMX.DM10, 0, DIMX.DM11, 0, DIMX.DM12, 0, DIMX.DM13, 0),
		dimx[2] = dimx[3].xxzzlh();
		dimx[5] = GSVector4i(DIMX.DM20, 0, DIMX.DM21, 0, DIMX.DM22, 0, DIMX.DM23, 0),
		dimx[4] = dimx[5].xxzzlh();
		dimx[7] = GSVector4i(DIMX.DM30, 0, DIMX.DM31, 0, DIMX.DM32, 0, DIMX.DM33, 0),
		dimx[6] = dimx[7].xxzzlh();
	}

	void Dump(const std::string& filename)
	{
		FILE* fp = fopen(filename.c_str(), "wt");
		if (!fp) return;

		fprintf(fp, "PRIM\n"
				"\tPRIM:%d\n"
				"\tIIP:%d\n"
				"\tTME:%d\n"
				"\tFGE:%d\n"
				"\tABE:%d\n"
				"\tAA1:%d\n"
				"\tFST:%d\n"
				"\tCTXT:%d\n"
				"\tFIX:%d\n\n"
				, PRIM.PRIM, PRIM.IIP, PRIM.TME, PRIM.FGE, PRIM.ABE, PRIM.AA1, PRIM.FST, PRIM.CTXT, PRIM.FIX);

		fprintf(fp, "PRMODE (when AC=0)\n"
				"\t_PRIM:%d\n"
				"\tIIP:%d\n"
				"\tTME:%d\n"
				"\tFGE:%d\n"
				"\tABE:%d\n"
				"\tAA1:%d\n"
				"\tFST:%d\n"
				"\tCTXT:%d\n"
				"\tFIX:%d\n\n"
				, PRMODE._PRIM, PRMODE.IIP, PRMODE.TME, PRMODE.FGE, PRMODE.ABE, PRMODE.AA1, PRMODE.FST, PRMODE.CTXT, PRMODE.FIX);

		fprintf(fp, "PRMODECONT\n"
				"\tAC:%d\n\n"
				, PRMODECONT.AC);

		fprintf(fp, "TEXCLUT\n"
				"\tCOU:%d\n"
				"\tCBW:%d\n"
				"\tCOV:%d\n\n"
				, TEXCLUT.COU, TEXCLUT.CBW, TEXCLUT.COV);

		fprintf(fp, "SCANMSK\n"
				"\tMSK:%d\n\n"
				"\n"
				, SCANMSK.MSK);

		fprintf(fp, "TEXA\n"
				"\tAEM:%d\n"
				"\tTA0:%d\n"
				"\tTA1:%d\n\n"
				, TEXA.AEM, TEXA.TA0, TEXA.TA1);

		fprintf(fp, "FOGCOL\n"
				"\tFCG:%d\n"
				"\tFCB:%d\n"
				"\tFCR:%d\n\n"
				, FOGCOL.FCG, FOGCOL.FCB, FOGCOL.FCR);

		fprintf(fp, "DIMX\n"
				"\tDM22:%d\n"
				"\tDM23:%d\n"
				"\tDM31:%d\n"
				"\tDM02:%d\n"
				"\tDM21:%d\n"
				"\tDM12:%d\n"
				"\tDM03:%d\n"
				"\tDM01:%d\n"
				"\tDM33:%d\n"
				"\tDM30:%d\n"
				"\tDM11:%d\n"
				"\tDM10:%d\n"
				"\tDM20:%d\n"
				"\tDM32:%d\n"
				"\tDM00:%d\n"
				"\tDM13:%d\n\n"
				, DIMX.DM22, DIMX.DM23, DIMX.DM31, DIMX.DM02, DIMX.DM21, DIMX.DM12, DIMX.DM03, DIMX.DM01, DIMX.DM33, DIMX.DM30, DIMX.DM11, DIMX.DM10, DIMX.DM20, DIMX.DM32, DIMX.DM00, DIMX.DM13);

		fprintf(fp, "DTHE\n"
				"\tDTHE:%d\n\n"
				, DTHE.DTHE);

		fprintf(fp, "COLCLAMP\n"
				"\tCLAMP:%d\n\n"
				, COLCLAMP.CLAMP);

		fprintf(fp, "PABE\n"
				"\tPABE:%d\n\n"
				, PABE.PABE);

		fprintf(fp, "BITBLTBUF\n"
				"\tSBW:%d\n"
				"\tSBP:%d\n"
				"\tSPSM:%d\n"
				"\tDBW:%d\n"
				"\tDPSM:%d\n"
				"\tDBP:%d\n\n"
				, BITBLTBUF.SBW, BITBLTBUF.SBP, BITBLTBUF.SPSM, BITBLTBUF.DBW, BITBLTBUF.DPSM, BITBLTBUF.DBP);

		fprintf(fp, "TRXDIR\n"
				"\tXDIR:%d\n\n"
				, TRXDIR.XDIR);

		fprintf(fp, "TRXPOS\n"
				"\tDIRY:%d\n"
				"\tSSAY:%d\n"
				"\tSSAX:%d\n"
				"\tDIRX:%d\n"
				"\tDSAX:%d\n"
				"\tDSAY:%d\n\n"
				, TRXPOS.DIRY, TRXPOS.SSAY, TRXPOS.SSAX, TRXPOS.DIRX, TRXPOS.DSAX, TRXPOS.DSAY);

		fprintf(fp, "TRXREG\n"
				"\tRRH:%d\n"
				"\tRRW:%d\n\n"
				, TRXREG.RRH, TRXREG.RRW);

		fclose(fp);
	}

};
