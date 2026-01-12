// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GSGL.h"
#include "GS/GS.h"
#include "GS/GSUtil.h"
#include "GS/GSDrawingContext.h"
#include "GS/GSDrawingEnvironment.h"

void GSDrawingEnvironment::Reset()
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
}

void GSDrawingEnvironment::Dump(const std::string& filename) const
{
	FILE* fp = fopen(filename.c_str(), "wt");
	if (!fp)
		return;

	// Warning: The indentation must be consistent with GSDrawingContext::Dump().

	fprintf(fp, "PRIM:\n"
	            "    PRIM: %u # %s\n"
	            "    IIP: %u\n"
	            "    TME: %u\n"
	            "    FGE: %u\n"
	            "    ABE: %u\n"
	            "    AA1: %u\n"
	            "    FST: %u\n"
	            "    CTXT: %u\n"
	            "    FIX: %u\n\n",
		PRIM.PRIM, GSUtil::GetPrimName(PRIM.PRIM), PRIM.IIP, PRIM.TME, PRIM.FGE, PRIM.ABE, PRIM.AA1, PRIM.FST, PRIM.CTXT, PRIM.FIX);

	fprintf(fp, "PRMODECONT:\n"
	            "    AC: %u # %s\n\n",
		PRMODECONT.AC, GSUtil::GetACName(PRMODECONT.AC));

	fprintf(fp, "TEXCLUT:\n"
	            "    CBW: %u\n"
	            "    COU: %u\n"
	            "    COV: %u\n\n",
		TEXCLUT.CBW, TEXCLUT.COU, TEXCLUT.COV);

	fprintf(fp, "SCANMSK:\n"
	            "    MSK: %u # %s\n\n",
		SCANMSK.MSK, GSUtil::GetSCANMSKName(SCANMSK.MSK));

	fprintf(fp, "TEXA:\n"
	            "    AEM: %u\n"
	            "    TA0: %u\n"
	            "    TA1: %u\n\n",
		TEXA.AEM, TEXA.TA0, TEXA.TA1);

	fprintf(fp, "FOGCOL:\n"
	            "    FCG: %u\n"
	            "    FCB: %u\n"
	            "    FCR: %u\n\n",
		FOGCOL.FCG, FOGCOL.FCB, FOGCOL.FCR);

	fprintf(fp, "DIMX:\n"
	            "    DM22: %d\n"
	            "    DM23: %d\n"
	            "    DM31: %d\n"
	            "    DM02: %d\n"
	            "    DM21: %d\n"
	            "    DM12: %d\n"
	            "    DM03: %d\n"
	            "    DM01: %d\n"
	            "    DM33: %d\n"
	            "    DM30: %d\n"
	            "    DM11: %d\n"
	            "    DM10: %d\n"
	            "    DM20: %d\n"
	            "    DM32: %d\n"
	            "    DM00: %d\n"
	            "    DM13: %d\n\n",
		DIMX.DM22, DIMX.DM23, DIMX.DM31, DIMX.DM02, DIMX.DM21, DIMX.DM12, DIMX.DM03, DIMX.DM01, DIMX.DM33, DIMX.DM30, DIMX.DM11, DIMX.DM10, DIMX.DM20, DIMX.DM32, DIMX.DM00, DIMX.DM13);

	fprintf(fp, "DTHE:\n"
	            "    DTHE: %u\n\n",
		DTHE.DTHE);

	fprintf(fp, "COLCLAMP:\n"
	            "    CLAMP: %u\n\n",
		COLCLAMP.CLAMP);

	fprintf(fp, "PABE:\n"
	            "    PABE: %u\n\n",
		PABE.PABE);

	fprintf(fp, "BITBLTBUF:\n"
	            "    SBW: %u\n"
	            "    SBP: 0x%x\n"
	            "    SPSM: %u # %s\n"
	            "    DBW: %u\n"
	            "    DPSM: %u # %s\n"
	            "    DBP: 0x%x\n\n",
		BITBLTBUF.SBW, BITBLTBUF.SBP, BITBLTBUF.SPSM, GSUtil::GetPSMName(BITBLTBUF.SPSM), BITBLTBUF.DBW, BITBLTBUF.DPSM, GSUtil::GetPSMName(BITBLTBUF.DPSM), BITBLTBUF.DBP);

	fprintf(fp, "TRXDIR:\n"
	            "    XDIR: %u\n\n",
		TRXDIR.XDIR);

	fprintf(fp, "TRXPOS:\n"
	            "    DIRY: %u\n"
	            "    SSAY: %u\n"
	            "    SSAX: %u\n"
	            "    DIRX: %u\n"
	            "    DSAX: %u\n"
	            "    DSAY: %u\n\n",
		TRXPOS.DIRY, TRXPOS.SSAY, TRXPOS.SSAX, TRXPOS.DIRX, TRXPOS.DSAX, TRXPOS.DSAY);

	fprintf(fp, "TRXREG:\n"
	            "    RRH: %u\n"
	            "    RRW: %u\n\n",
		TRXREG.RRH, TRXREG.RRW);

	fclose(fp);
}