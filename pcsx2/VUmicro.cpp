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

// Executes a Block based on EE delta time
void BaseVUmicroCPU::ExecuteBlock(bool startUp)
{
	const u32& stat = VU0.VI[REG_VPU_STAT].UL;
	const int test = m_Idx ? 0x100 : 1;
	const int s = EmuConfig.Gamefixes.VUKickstartHack ? 16 : 0; // Kick Start Cycles (Jak needs at least 4 due to writing values after they're read

	if (m_Idx && THREAD_VU1)
	{
		vu1Thread.Get_MTVUChanges();
		return;
	}

	if (!(stat & test))
	{
		if (m_Idx == 1)
		{
			if (VU1.xgkickenable && (cpuRegs.cycle - VU1.xgkicklastcycle) >= 2)
			{
				_vuXGKICKTransfer(&VU1, (cpuRegs.cycle - VU1.xgkicklastcycle), false);
			}
		}
		return;
	}

	// You might be looking at this and thinking, what the hell is going on? What's with all these conditions?
	// Well, basically M-Bit timed games are REALLY picky, so we need some extra checks in to make sure the VU
	// doesn't go too long without updating/syncing as games will wait for an M-Bit then transfer a bunch of stuff
	// since they will know what the timing is going to be on them, so we need to keep it somewhat tight.
	// For everything else (Especially stuff that needs kickstart), they can do what they like.
	if (startUp) // Start Executing a microprogram (When kickstarted)
	{
		Execute(s); // Kick start VU

		if (stat & test)
		{
			if (m_Idx)
				cpuRegs.cycle = VU1.cycle;
			else
				cpuRegs.cycle = VU0.cycle;

			u32 nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;

			if((VU0.flags & VUFLAG_MFLAGSET) || VU0.blockhasmbit)
				cpuSetNextEventDelta(nextblockcycles);
			else if(s)
				cpuSetNextEventDelta(s);
		}
	}
	else // Continue Executing
	{
		u32 cycle = m_Idx ? VU1.cycle : VU0.cycle;
		s32 delta = (s32)(u32)(cpuRegs.cycle - cycle);
		s32 nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;

		if (EmuConfig.Gamefixes.VUKickstartHack)
		{
			if (delta > 0)  // When kickstarting we just need 1 cycle for run ahead
			Execute(delta);
		}
		else
		{
			if (delta >= nextblockcycles && delta > 0) // When running behind, make sure we have enough cycles passed for the block to run
				Execute(delta);
		}

		if ((stat & test) && !EmuConfig.Gamefixes.VUKickstartHack)
		{
			// Queue up next required time to run a block
			nextblockcycles = m_Idx ? VU1.nextBlockCycles : VU0.nextBlockCycles;
			cycle = m_Idx ? VU1.cycle : VU0.cycle;
			nextblockcycles = nextblockcycles - (cycle - cpuRegs.cycle);

			if (nextblockcycles > 0 || (VU0.flags & VUFLAG_MFLAGSET) || VU0.blockhasmbit)
			{
				cpuSetNextEventDelta(nextblockcycles);
			}
		}
	}
}

// This function is called by VU0 Macro (COP2) after transferring some
// EE data to VU0's registers. We want to run VU0 Micro right after this
// to ensure that the register is used at the correct time.
// This fixes spinning/hanging in some games like Ratchet and Clank's Intro.
void BaseVUmicroCPU::ExecuteBlockJIT(BaseVUmicroCPU* cpu)
{
	const u32& stat = VU0.VI[REG_VPU_STAT].UL;
	const int test = 1;

	//DevCon.Warning("Was set %d cycles ago", cpuRegs.cycle - setcycle);
	if (stat & test)
	{ // VU is running
		s32 delta = (s32)(u32)(cpuRegs.cycle - VU0.cycle);

		if (delta > 0)
		{
			cpu->Execute(delta); // Execute the time since the last call
		}
	}
}
