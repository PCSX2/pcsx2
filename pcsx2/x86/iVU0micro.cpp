/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "VUmicro.h"
#include "iVUzerorec.h"

namespace VU0micro
{

	static void recAlloc()
	{
		SuperVUAlloc(0);
	}

	static void recReset()
	{
		SuperVUReset(0);

		// these shouldn't be needed, but shouldn't hurt anythign either.
		x86FpuState = FPU_STATE;
		iCWstate = 0;
	}

	static void recStep()
	{
	}

	static void recExecuteBlock()
	{
		if((VU0.VI[REG_VPU_STAT].UL & 1) == 0)
			return;

		FreezeXMMRegs(1);
		SuperVUExecuteProgram(VU0.VI[ REG_TPC ].UL & 0xfff, 0);
		FreezeXMMRegs(0);
	}

	static void recClear(u32 Addr, u32 Size)
	{
		SuperVUClear(Addr, Size*4, 0);
	}

	static void recShutdown()
	{
		SuperVUDestroy( 0 );
	}
}

using namespace VU0micro;

VUmicroCpu recVU0 = 
{
	recAlloc
,	recReset
,	recStep
,	recExecuteBlock
,	recClear
,	recShutdown
};
