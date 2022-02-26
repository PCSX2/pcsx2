/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "iR5900Analysis.h"
#include "Memory.h"
#include "DebugTools/Debug.h"

using namespace R5900;

// This should be moved to analysis...
extern int cop2flags(u32 code);

AnalysisPass::AnalysisPass() = default;

AnalysisPass::~AnalysisPass() = default;

void AnalysisPass::Run(u32 start, u32 end, EEINST* inst_cache)
{
}

template <class F>
void __fi AnalysisPass::ForEachInstruction(u32 start, u32 end, EEINST* inst_cache, const F& func)
{
	EEINST* eeinst = inst_cache;
	for (u32 apc = start; apc < end; apc += 4, eeinst++)
	{
		cpuRegs.code = memRead32(apc);
		if (!func(apc, eeinst))
			break;
	}
}

template <class F>
void __fi R5900::AnalysisPass::DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache, const F& func)
{
	std::string d;
	EEINST* eeinst = inst_cache;
	for (u32 apc = start; apc < end; apc += 4, eeinst++)
	{
		d.clear();
		disR5900Fasm(d, memRead32(apc), apc, false);
		func(apc, eeinst, d);
		Console.WriteLn("  %08X %s", apc, d.c_str());
	}
}

COP2FlagHackPass::COP2FlagHackPass()
	: AnalysisPass()
{
}

COP2FlagHackPass::~COP2FlagHackPass() = default;

void COP2FlagHackPass::Run(u32 start, u32 end, EEINST* inst_cache)
{
	m_status_denormalized = false;
	m_last_status_write = nullptr;
	m_last_mac_write = nullptr;
	m_last_clip_write = nullptr;
	m_cfc2_pc = start;

	ForEachInstruction(start, end, inst_cache, [this, end](u32 apc, EEINST* inst) {
		// catch SB/SH/SW to potential DMA->VIF0->VU0 exec.
		// this is very unlikely in a cop2 chain.
		if (_Opcode_ == 050 || _Opcode_ == 051 || _Opcode_ == 053)
		{
			CommitAllFlags();
			return true;
		}
		else if (_Opcode_ != 022)
		{
			// not COP2
			return true;
		}

		// Detect ctc2 Status, zero, ..., cfc2 v0, Status pattern where we need accurate sticky bits.
		// Test case: Tekken Tag Tournament.
		if (_Rs_ == 6 && _Rd_ == REG_STATUS_FLAG)
		{
			// Read ahead, looking for cfc2.
			m_cfc2_pc = apc;
			ForEachInstruction(apc, end, inst, [this](u32 capc, EEINST*) {
				if (_Opcode_ == 022 && _Rs_ == 2 && _Rd_ == REG_STATUS_FLAG)
				{
					m_cfc2_pc = capc;
					return false;
				}
				return true;
			});
#ifdef PCSX2_DEVBUILD
			if (m_cfc2_pc != apc)
				DevCon.WriteLn("CTC2 at %08X paired with CFC2 %08X", apc, m_cfc2_pc);
#endif
		}

		// CFC2/CTC2
		if (_Rs_ == 6 || _Rs_ == 2)
		{
			switch (_Rd_)
			{
				case REG_STATUS_FLAG:
					CommitStatusFlag();
					break;
				case REG_MAC_FLAG:
					CommitMACFlag();
					break;
				case REG_CLIP_FLAG:
					CommitClipFlag();
					break;
				case REG_FBRST:
				{
					// only apply to CTC2, is FBRST readable?
					if (_Rs_ == 2)
						CommitAllFlags();
				}
				break;
			}
		}

		if (((cpuRegs.code >> 25 & 1) == 1) && ((cpuRegs.code >> 2 & 15) == 14))
		{
			// VCALLMS, everything needs to be up to date
			CommitAllFlags();
		}

		// 1 - status, 2 - mac, 3 - clip
		const int flags = cop2flags(cpuRegs.code);
		if (flags == 0)
			return true;

		// STATUS
		if (flags & 1)
		{
			if (!m_status_denormalized)
			{
				inst->info |= EEINST_COP2_DENORMALIZE_STATUS_FLAG;
				m_status_denormalized = true;
			}

			// if we're still behind the next CFC2 after the sticky bits got cleared, we need to update flags
			if (apc < m_cfc2_pc)
				inst->info |= EEINST_COP2_STATUS_FLAG;

			m_last_status_write = inst;
		}

		// MAC
		if (flags & 2)
		{
			m_last_mac_write = inst;
		}

		// CLIP
		if (flags & 4)
		{
			// we don't track the clip flag yet..
			// but it's unlikely that we'll have more than 4 clip flags in a row, because that would be pointless?
			inst->info |= EEINST_COP2_CLIP_FLAG;
			m_last_clip_write = inst;
		}

		return true;
	});

	CommitAllFlags();

#if 0
	if (m_cfc2_pc != start)
		DumpAnnotatedBlock(start, end, inst_cache);
#endif
}

void COP2FlagHackPass::DumpAnnotatedBlock(u32 start, u32 end, EEINST* inst_cache)
{
	AnalysisPass::DumpAnnotatedBlock(start, end, inst_cache, [](u32, EEINST* eeinst, std::string& d) {
		if (eeinst->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG)
			d.append(" COP2_DENORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_NORMALIZE_STATUS_FLAG)
			d.append(" COP2_NORMALIZE_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_STATUS_FLAG)
			d.append(" COP2_STATUS_FLAG");
		if (eeinst->info & EEINST_COP2_MAC_FLAG)
			d.append(" COP2_MAC_FLAG");
		if (eeinst->info & EEINST_COP2_CLIP_FLAG)
			d.append(" COP2_CLIP_FLAG");
	});
}

void COP2FlagHackPass::CommitStatusFlag()
{
	if (m_last_status_write)
	{
		m_last_status_write->info |= EEINST_COP2_STATUS_FLAG | EEINST_COP2_NORMALIZE_STATUS_FLAG;
		m_status_denormalized = false;
	}
}

void COP2FlagHackPass::CommitMACFlag()
{
	if (m_last_mac_write)
		m_last_mac_write->info |= EEINST_COP2_MAC_FLAG;
}

void COP2FlagHackPass::CommitClipFlag()
{
	if (m_last_clip_write)
		m_last_clip_write->info |= EEINST_COP2_CLIP_FLAG;
}

void COP2FlagHackPass::CommitAllFlags()
{
	CommitStatusFlag();
	CommitMACFlag();
	CommitClipFlag();
}
