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

class alignas(32) GSDrawingEnvironment
{
public:
	GIFRegPRIM       PRIM;
	GIFRegPRMODE     PRMODE;
	GIFRegPRMODECONT PRMODECONT;
	GIFRegTEXCLUT    TEXCLUT;
	GIFRegSCANMSK    SCANMSK;
	GIFRegTEXA       TEXA;
	GIFRegFOGCOL     FOGCOL;
	GIFRegDIMX       DIMX;
	GIFRegDTHE       DTHE;
	GIFRegCOLCLAMP   COLCLAMP;
	GIFRegPABE       PABE;
	GIFRegBITBLTBUF  BITBLTBUF;
	GIFRegTRXDIR     TRXDIR;
	GIFRegTRXPOS     TRXPOS;
	GIFRegTRXREG     TRXREG;
	GSDrawingContext CTXT[2];

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
		if (!fp)
			return;

		fprintf(fp, "PRIM\n"
		            "\tPRIM:%u\n"
		            "\tIIP:%u\n"
		            "\tTME:%u\n"
		            "\tFGE:%u\n"
		            "\tABE:%u\n"
		            "\tAA1:%u\n"
		            "\tFST:%u\n"
		            "\tCTXT:%u\n"
		            "\tFIX:%u\n\n"
		        , PRIM.PRIM, PRIM.IIP, PRIM.TME, PRIM.FGE, PRIM.ABE, PRIM.AA1, PRIM.FST, PRIM.CTXT, PRIM.FIX);

		fprintf(fp, "PRMODE (when AC=0)\n"
		            "\t_PRIM:%u\n"
		            "\tIIP:%u\n"
		            "\tTME:%u\n"
		            "\tFGE:%u\n"
		            "\tABE:%u\n"
		            "\tAA1:%u\n"
		            "\tFST:%u\n"
		            "\tCTXT:%u\n"
		            "\tFIX:%u\n\n"
		        , PRMODE._PRIM, PRMODE.IIP, PRMODE.TME, PRMODE.FGE, PRMODE.ABE, PRMODE.AA1, PRMODE.FST, PRMODE.CTXT, PRMODE.FIX);

		fprintf(fp, "PRMODECONT\n"
		            "\tAC:%u\n\n"
		        , PRMODECONT.AC);

		fprintf(fp, "TEXCLUT\n"
		            "\tCOU:%u\n"
		            "\tCBW:%u\n"
		            "\tCOV:%u\n\n"
		        , TEXCLUT.COU, TEXCLUT.CBW, TEXCLUT.COV);

		fprintf(fp, "SCANMSK\n"
		            "\tMSK:%u\n\n"
		            "\n"
		        , SCANMSK.MSK);

		fprintf(fp, "TEXA\n"
		            "\tAEM:%u\n"
		            "\tTA0:%u\n"
		            "\tTA1:%u\n\n"
		        , TEXA.AEM, TEXA.TA0, TEXA.TA1);

		fprintf(fp, "FOGCOL\n"
		            "\tFCG:%u\n"
		            "\tFCB:%u\n"
		            "\tFCR:%u\n\n"
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
		            "\tDTHE:%u\n\n"
		        , DTHE.DTHE);

		fprintf(fp, "COLCLAMP\n"
		            "\tCLAMP:%u\n\n"
		        , COLCLAMP.CLAMP);

		fprintf(fp, "PABE\n"
		            "\tPABE:%u\n\n"
		        , PABE.PABE);

		fprintf(fp, "BITBLTBUF\n"
		            "\tSBW:%u\n"
		            "\tSBP:0x%x\n"
		            "\tSPSM:%u\n"
		            "\tDBW:%u\n"
		            "\tDPSM:%u\n"
		            "\tDBP:0x%x\n\n"
		        , BITBLTBUF.SBW, BITBLTBUF.SBP, BITBLTBUF.SPSM, BITBLTBUF.DBW, BITBLTBUF.DPSM, BITBLTBUF.DBP);

		fprintf(fp, "TRXDIR\n"
		            "\tXDIR:%u\n\n",
		        TRXDIR.XDIR);

		fprintf(fp, "TRXPOS\n"
		            "\tDIRY:%u\n"
		            "\tSSAY:%u\n"
		            "\tSSAX:%u\n"
		            "\tDIRX:%u\n"
		            "\tDSAX:%u\n"
		            "\tDSAY:%u\n\n"
		        , TRXPOS.DIRY, TRXPOS.SSAY, TRXPOS.SSAX, TRXPOS.DIRX, TRXPOS.DSAX, TRXPOS.DSAY);

		fprintf(fp, "TRXREG\n"
		            "\tRRH:%u\n"
		            "\tRRW:%u\n\n"
		        , TRXREG.RRH, TRXREG.RRW);

		fclose(fp);
	}
};
