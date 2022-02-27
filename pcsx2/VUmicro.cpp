/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "Common.h"
#include "VUmicro.h"
#include "MTVU.h"
#include "GS.h"
#include "Gif_Unit.h"

__inline u32 CalculateMinRunCycles(u32 cycles, bool requiresAccurateCycles)
{
	// If we're running an interlocked COP2 operation
	// run for an exact amount of cycles
	if(requiresAccurateCycles)
		return cycles;

	// Allow a minimum of 16 cycles to avoid running small blocks
	// Running a block of like 3 cycles is highly inefficient
	// so while sync isn't tight, it's okay to run ahead a little bit.
	return std::max(16U, cycles);
}

// Executes a Block based on EE delta time
void BaseVUmicroCPU::ExecuteBlock(bool startUp)
{
	const u32& stat = VU0.VI[REG_VPU_STAT].UL;
	const int test = m_Idx ? 0x100 : 1;

	if (m_Idx && THREAD_VU1)
	{
		vu1Thread.Get_MTVUChanges();
		return;
	}

	if (!(stat & test))
	{
		// VU currently flushes XGKICK on VU1 end so no need for this, yet
		/*if (m_Idx == 1 && VU1.xgkickenable)
		{
			_vuXGKICKTransfer((cpuRegs.cycle - VU1.xgkicklastcycle), false);
		}*/
		return;
	}

	if (startUp)
	{
		Execute(CalculateMinRunCycles(0, false));
	}
	else // Continue Executing
	{
		u32 cycle = m_Idx ? VU1.cycle : VU0.cycle;
		s32 delta = (s32)(u32)(cpuRegs.cycle - cycle);

		if (delta > 0)
			Execute(CalculateMinRunCycles(delta, false));
	}
}

// This function is called by VU0 Macro (COP2) after transferring some
// EE data to VU0's registers. We want to run VU0 Micro right after this
// to ensure that the register is used at the correct time.
// This fixes spinning/hanging in some games like Ratchet and Clank's Intro.
void BaseVUmicroCPU::ExecuteBlockJIT(BaseVUmicroCPU* cpu, bool interlocked)
{
	const u32& stat = VU0.VI[REG_VPU_STAT].UL;
	constexpr int test = 1;

	if (stat & test)
	{ // VU is running
		s32 delta = (s32)(u32)(cpuRegs.cycle - VU0.cycle);

		if (delta > 0)
		{
			cpu->Execute(CalculateMinRunCycles(delta, interlocked)); // Execute the time since the last call
		}
	}
}
