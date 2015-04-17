/*  GSnull
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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

#ifndef __GS_H__
#define __GS_H__

#include "Pcsx2Defs.h"
#include <stdio.h>
#define GSdefs
#include "PS2Edefs.h"
#include "PS2Eext.h"

#include "ConfigCommon.h"

#include "gsnull_GifTransfer.h"

typedef struct
{
	u32 CSRw;
	pathInfo path[4];
	bool Path3transfer;
	float q;
	u32 imageTransfer;
	int MultiThreaded;
	int nPath3Hack;

	GIFReg regs;
	GIFCTXTReg ctxt_regs[2];
} GSVars;

extern GSVars gs;

extern ConfigLogCombination clcGS;

#endif
