/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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
#include "MIPSAnalyst.h"
#include "Debug.h"
#include "DebugInterface.h"
#include "SymbolMap.h"
#include "DebugInterface.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"

static std::vector<MIPSAnalyst::AnalyzedFunction> functions;

#define MIPS_MAKE_J(addr)   (0x08000000 | ((addr)>>2))
#define MIPS_MAKE_JAL(addr) (0x0C000000 | ((addr)>>2))
#define MIPS_MAKE_JR_RA()   (0x03e00008)
#define MIPS_MAKE_NOP()     (0)

#define INVALIDTARGET 0xFFFFFFFF

#define FULLOP_JR_RA 0x03e00008
#define OP_SYSCALL   0x0000000c
#define OP_SYSCALL_MASK 0xFC00003F
#define _RS   ((op>>21) & 0x1F)
#define _RT   ((op>>16) & 0x1F)

namespace MIPSAnalyst
{
	u32 GetJumpTarget(u32 addr)
	{
		u32 op = r5900Debug.read32(addr);
		const R5900::OPCODE& opcode = R5900::GetInstruction(op);

		if ((opcode.flags & IS_BRANCH) && (opcode.flags & BRANCHTYPE_MASK) == BRANCHTYPE_JUMP)
		{
			u32 target = (addr & 0xF0000000) | ((op&0x03FFFFFF) << 2);
			return target;
		}
		else
			return INVALIDTARGET;
	}

	u32 GetBranchTarget(u32 addr)
	{
		u32 op = r5900Debug.read32(addr);
		const R5900::OPCODE& opcode = R5900::GetInstruction(op);
		
		int branchType = (opcode.flags & BRANCHTYPE_MASK);
		if ((opcode.flags & IS_BRANCH) && (branchType == BRANCHTYPE_BRANCH || branchType == BRANCHTYPE_BC1))
			return addr + 4 + ((signed short)(op&0xFFFF)<<2);
		else
			return INVALIDTARGET;
	}
	
	u32 GetBranchTargetNoRA(u32 addr)
	{
		u32 op = r5900Debug.read32(addr);
		const R5900::OPCODE& opcode = R5900::GetInstruction(op);
		
		int branchType = (opcode.flags & BRANCHTYPE_MASK);
		if ((opcode.flags & IS_BRANCH) && (branchType == BRANCHTYPE_BRANCH || branchType == BRANCHTYPE_BC1))
		{
			if (!(opcode.flags & IS_LINKED))
				return addr + 4 + ((signed short)(op&0xFFFF)<<2);
			else
				return INVALIDTARGET;
		}
		else
			return INVALIDTARGET;
	}

	u32 GetSureBranchTarget(u32 addr)
	{
		u32 op = r5900Debug.read32(addr);
		const R5900::OPCODE& opcode = R5900::GetInstruction(op);
		
		if ((opcode.flags & IS_BRANCH) && (opcode.flags & BRANCHTYPE_MASK) == BRANCHTYPE_BRANCH)
		{
			bool sure = false;
			bool takeBranch = false;
			switch (opcode.flags & CONDTYPE_MASK)
			{
			case CONDTYPE_EQ:
				sure = _RS == _RT;
				takeBranch = true;
				break;

			case CONDTYPE_NE:
				sure = _RS == _RT;
				takeBranch = false;
				break;

			case CONDTYPE_LEZ:
			case CONDTYPE_GEZ:
				sure = _RS == 0;
				takeBranch = true;
				break;

			case CONDTYPE_LTZ:
			case CONDTYPE_GTZ:
				sure = _RS == 0;
				takeBranch = false;
				break;

			default:
				break;
			}

			if (sure && takeBranch)
				return addr + 4 + ((signed short)(op&0xFFFF)<<2);
			else if (sure && !takeBranch)
				return addr + 8;
			else
				return INVALIDTARGET;
		}
		else
			return INVALIDTARGET;
	}

	static const char *DefaultFunctionName(char buffer[256], u32 startAddr) {
		sprintf(buffer, "z_un_%08x", startAddr);
		return buffer;
	}


	static u32 ScanAheadForJumpback(u32 fromAddr, u32 knownStart, u32 knownEnd) {
		static const u32 MAX_AHEAD_SCAN = 0x1000;
		// Maybe a bit high... just to make sure we don't get confused by recursive tail recursion.
		static const u32 MAX_FUNC_SIZE = 0x20000;

		if (fromAddr > knownEnd + MAX_FUNC_SIZE) {
			return INVALIDTARGET;
		}

		// Code might jump halfway up to before fromAddr, but after knownEnd.
		// In that area, there could be another jump up to the valid range.
		// So we track that for a second scan.
		u32 closestJumpbackAddr = INVALIDTARGET;
		u32 closestJumpbackTarget = fromAddr;

		// We assume the furthest jumpback is within the func.
		u32 furthestJumpbackAddr = INVALIDTARGET;

		for (u32 ahead = fromAddr; ahead < fromAddr + MAX_AHEAD_SCAN; ahead += 4) {
			u32 aheadOp = r5900Debug.read32(ahead);
			u32 target = GetBranchTargetNoRA(ahead);
			if (target == INVALIDTARGET && ((aheadOp & 0xFC000000) == 0x08000000)) {
				target = GetJumpTarget(ahead);
			}

			if (target != INVALIDTARGET) {
				// Only if it comes back up to known code within this func.
				if (target >= knownStart && target <= knownEnd) {
					furthestJumpbackAddr = ahead;
				}
				// But if it jumps above fromAddr, we should scan that area too...
				if (target < closestJumpbackTarget && target < fromAddr && target > knownEnd) {
					closestJumpbackAddr = ahead;
					closestJumpbackTarget = target;
				}
			}
			if (aheadOp == MIPS_MAKE_JR_RA()) {
				break;
			}
		}

		if (closestJumpbackAddr != INVALIDTARGET && furthestJumpbackAddr == INVALIDTARGET) {
			for (u32 behind = closestJumpbackTarget; behind < fromAddr; behind += 4) {
				u32 behindOp = r5900Debug.read32(behind);
				u32 target = GetBranchTargetNoRA(behind);
				if (target == INVALIDTARGET && ((behindOp & 0xFC000000) == 0x08000000)) {
					target = GetJumpTarget(behind);
				}

				if (target != INVALIDTARGET) {
					if (target >= knownStart && target <= knownEnd) {
						furthestJumpbackAddr = closestJumpbackAddr;
					}
				}
			}
		}

		return furthestJumpbackAddr;
	}

	void ScanForFunctions(SymbolMap& map, u32 startAddr, u32 endAddr, bool insertSymbols) {
		AnalyzedFunction currentFunction = {startAddr};

		u32 furthestBranch = 0;
		bool looking = false;
		bool end = false;
		bool isStraightLeaf = true;

		functions.clear();

		u32 addr;
		for (addr = startAddr; addr <= endAddr; addr += 4) {
			// Use pre-existing symbol map info if available. May be more reliable.
			SymbolInfo syminfo;
			if (map.GetSymbolInfo(&syminfo, addr, ST_FUNCTION)) {
				addr = syminfo.address + syminfo.size - 4;

				// We still need to insert the func for hashing purposes.
				currentFunction.start = syminfo.address;
				currentFunction.end = syminfo.address + syminfo.size - 4;
				functions.push_back(currentFunction);
				currentFunction.start = addr + 4;
				furthestBranch = 0;
				looking = false;
				end = false;
				continue;
			}

			u32 op = r5900Debug.read32(addr);

			u32 target = GetBranchTargetNoRA(addr);
			if (target != INVALIDTARGET) {
				isStraightLeaf = false;
				if (target > furthestBranch) {
					furthestBranch = target;
				}
			} else if ((op & 0xFC000000) == 0x08000000) {
				u32 sureTarget = GetJumpTarget(addr);
				// Check for a tail call.  Might not even have a jr ra.
				if (sureTarget != INVALIDTARGET && sureTarget < currentFunction.start) {
					if (furthestBranch > addr) {
						looking = true;
						addr += 4;
					} else {
						end = true;
					}
				} else if (sureTarget != INVALIDTARGET && sureTarget > addr && sureTarget > furthestBranch) {
					// A jump later.  Probably tail, but let's check if it jumps back.
					u32 knownEnd = furthestBranch == 0 ? addr : furthestBranch;
					u32 jumpback = ScanAheadForJumpback(sureTarget, currentFunction.start, knownEnd);
					if (jumpback != INVALIDTARGET && jumpback > addr && jumpback > knownEnd) {
						furthestBranch = jumpback;
					} else {
						if (furthestBranch > addr) {
							looking = true;
							addr += 4;
						} else {
							end = true;
						}
					}
				}
			}

			if (op == MIPS_MAKE_JR_RA()) {
				// If a branch goes to the jr ra, it's still ending here.
				if (furthestBranch > addr) {
					looking = true;
					addr += 4;
				} else {
					end = true;
				}
			}

			if (looking) {
				if (addr >= furthestBranch) {
					u32 sureTarget = GetSureBranchTarget(addr);
					// Regular j only, jals are to new funcs.
					if (sureTarget == INVALIDTARGET && ((op & 0xFC000000) == 0x08000000)) {
						sureTarget = GetJumpTarget(addr);
					}

					if (sureTarget != INVALIDTARGET && sureTarget < addr) {
						end = true;
					} else if (sureTarget != INVALIDTARGET) {
						// Okay, we have a downward jump.  Might be an else or a tail call...
						// If there's a jump back upward in spitting distance of it, it's an else.
						u32 knownEnd = furthestBranch == 0 ? addr : furthestBranch;
						u32 jumpback = ScanAheadForJumpback(sureTarget, currentFunction.start, knownEnd);
						if (jumpback != INVALIDTARGET && jumpback > addr && jumpback > knownEnd) {
							furthestBranch = jumpback;
						}
					}
				}
			}
			
			if (end) {
				// most functions are aligned to 8 or 16 bytes
				// add the padding to this one
				while (((addr+8) % 16)  && r5900Debug.read32(addr+8) == 0)
					addr += 4;

				currentFunction.end = addr + 4;
				currentFunction.isStraightLeaf = isStraightLeaf;
				functions.push_back(currentFunction);
				furthestBranch = 0;
				addr += 4;
				looking = false;
				end = false;
				isStraightLeaf = true;

				currentFunction.start = addr+4;
			}
		}

		currentFunction.end = addr + 4;
		functions.push_back(currentFunction);

		for (auto iter = functions.begin(); iter != functions.end(); iter++) {
			iter->size = iter->end - iter->start + 4;
			if (insertSymbols) {
				char temp[256];
				map.AddFunction(DefaultFunctionName(temp, iter->start), iter->start, iter->end - iter->start + 4);
			}
		}
	}

	MipsOpcodeInfo GetOpcodeInfo(DebugInterface* cpu, u32 address) {
		MipsOpcodeInfo info;
		memset(&info, 0, sizeof(info));

		if (!cpu->isValidAddress(address))
			return info;

		info.cpu = cpu;
		info.opcodeAddress = address;
		info.encodedOpcode = cpu->read32(address);
		u32 op = info.encodedOpcode;
		const R5900::OPCODE& opcode = R5900::GetInstruction(op);

		// extract all the branch related information
		info.isBranch = (opcode.flags & IS_BRANCH) != 0;
		if (info.isBranch)
		{
			info.isLinkedBranch = (opcode.flags & IS_LINKED) != 0;
			info.isLikelyBranch = (opcode.flags & IS_LIKELY) != 0;

			u64 rs,rt;
			u32 value;
			switch (opcode.flags & BRANCHTYPE_MASK)
			{
			case BRANCHTYPE_JUMP:
				info.isConditional = false;
				info.branchTarget =  (info.opcodeAddress & 0xF0000000) | ((op&0x03FFFFFF) << 2);
				break;
			case BRANCHTYPE_BRANCH:
				info.isConditional = true;
				info.branchTarget = info.opcodeAddress + 4 + ((s16)(op&0xFFFF)<<2);
			
				rs = info.cpu->getRegister(0,MIPS_GET_RS(op))._u64[0];
				rt = info.cpu->getRegister(0,MIPS_GET_RT(op))._u64[0];
				switch (opcode.flags & CONDTYPE_MASK)
				{
				case CONDTYPE_EQ:
					info.conditionMet = (rt == rs);
					if (MIPS_GET_RT(op) == MIPS_GET_RS(op))	// always true
						info.isConditional = false;
					break;
				case CONDTYPE_NE:
					info.conditionMet = (rt != rs);
					if (MIPS_GET_RT(op) == MIPS_GET_RS(op))	// always false
						info.isConditional = false;
					break;
				case CONDTYPE_LEZ:
					info.conditionMet = (((s64)rs) <= 0);
					break;
				case CONDTYPE_GTZ:
					info.conditionMet = (((s64)rs) > 0);
					break;
				case CONDTYPE_LTZ:
					info.conditionMet = (((s64)rs) < 0);
					break;
				case CONDTYPE_GEZ:
					info.conditionMet = (((s64)rs) >= 0);
					break;
				}
				
				break;
			case BRANCHTYPE_REGISTER:
				info.isConditional = false;
				info.isBranchToRegister = true;
				info.branchRegisterNum = (int)MIPS_GET_RS(op);
				info.branchTarget = info.cpu->getRegister(0,info.branchRegisterNum)._u32[0];
				break;
			case BRANCHTYPE_SYSCALL:
				info.isConditional = false;
				info.isSyscall = true;
				info.branchTarget = 0x80000000+0x180;
				break;
			case BRANCHTYPE_ERET:
				info.isConditional = false;
				// probably shouldn't be hard coded like this...
				if (cpuRegs.CP0.n.Status.b.ERL)
				{
					info.branchTarget = cpuRegs.CP0.n.ErrorEPC;
				} else {
					info.branchTarget = cpuRegs.CP0.n.EPC;
				}
				break;
			case BRANCHTYPE_BC1:
				info.isConditional = true;
				value = info.cpu->getRegister(EECAT_FCR,31)._u32[0] & 0x00800000;
				info.branchTarget = info.opcodeAddress + 4 + ((s16)(op&0xFFFF)<<2);

				switch (opcode.flags & CONDTYPE_MASK)
				{
				case CONDTYPE_EQ:
					info.conditionMet = value == 0;
					break;
				case CONDTYPE_NE:
					info.conditionMet = value != 0;
					break;
				}
				break;
			}
		}

		// extract the accessed memory address
		info.isDataAccess = (opcode.flags & IS_MEMORY) != 0;
		if (info.isDataAccess)
		{
			if (opcode.flags & IS_LEFT)
				info.lrType = LOADSTORE_LEFT;
			else if (opcode.flags & IS_RIGHT)
				info.lrType = LOADSTORE_RIGHT;
			
			u32 rs = info.cpu->getRegister(0, (int)MIPS_GET_RS(op));
			s16 imm16 = op & 0xFFFF;
			info.dataAddress = rs + imm16;

			switch (opcode.flags & MEMTYPE_MASK)
			{
			case MEMTYPE_BYTE:
				info.dataSize = 1;
				break;
			case MEMTYPE_HALF:
				info.dataSize = 2;
				break;
			case MEMTYPE_WORD:
				info.dataSize = 4;
				if (info.lrType == LOADSTORE_LEFT)
					info.dataAddress -= 3;
				break;
			case MEMTYPE_DWORD:
				info.dataSize = 8;
				if (info.lrType == LOADSTORE_LEFT)
					info.dataAddress -= 7;
				break;
			case MEMTYPE_QWORD:
				info.dataSize = 16;
				break;
			}
			
			info.hasRelevantAddress = true;
			info.releventAddress = info.dataAddress;
		}

		// gather relevant address for alu operations
		if (opcode.flags & IS_ALU)
		{
			u64 rs,rt;
			rs = info.cpu->getRegister(0,MIPS_GET_RS(op))._u64[0];
			rt = info.cpu->getRegister(0,MIPS_GET_RT(op))._u64[0];

			switch (opcode.flags & ALUTYPE_MASK)
			{
			case ALUTYPE_ADDI:
				info.hasRelevantAddress = true;
				info.releventAddress = rs+((s16)(op & 0xFFFF));
				break;
			case ALUTYPE_ADD:
				info.hasRelevantAddress = true;
				info.releventAddress = rs+rt;
				break;
			case ALUTYPE_SUB:
				info.hasRelevantAddress = true;
				info.releventAddress = rs-rt;
				break;
			case ALUTYPE_CONDMOVE:
				info.isConditional = true;

				switch (opcode.flags & CONDTYPE_MASK)
				{
				case CONDTYPE_EQ:
					info.conditionMet = (rt == 0);
					break;
				case CONDTYPE_NE:
					info.conditionMet = (rt != 0);
					break;
				}
				break;
			}
		}

		return info;
	}
}
