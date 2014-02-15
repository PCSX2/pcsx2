#include "PrecompiledHeader.h"
#include "MIPSAnalyst.h"
#include "Debug.h"
#include "DebugInterface.h"
#include "SymbolMap.h"
#include "DebugInterface.h"

static std::vector<MIPSAnalyst::AnalyzedFunction> functions;

#define MIPS_MAKE_J(addr)   (0x08000000 | ((addr)>>2))
#define MIPS_MAKE_JAL(addr) (0x0C000000 | ((addr)>>2))
#define MIPS_MAKE_JR_RA()   (0x03e00008)
#define MIPS_MAKE_NOP()     (0)

namespace MIPSAnalyst
{
	
	static const char *DefaultFunctionName(char buffer[256], u32 startAddr) {
		sprintf(buffer, "z_un_%08x", startAddr);
		return buffer;
	}

	void ScanForFunctions(u32 startAddr, u32 endAddr, bool insertSymbols) {
		AnalyzedFunction currentFunction = {startAddr};

		u32 furthestBranch = 0;
		bool looking = false;
		bool end = false;
		bool isStraightLeaf = true;

		u32 addr;
		for (addr = startAddr; addr <= endAddr; addr += 4) {
			// Use pre-existing symbol map info if available. May be more reliable.
			SymbolInfo syminfo;
			if (symbolMap.GetSymbolInfo(&syminfo, addr, ST_FUNCTION)) {
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

			u32 op = debug.read32(addr);
/*
			MIPSOpcode op = Memory::Read_Instruction(addr);
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
			}*/
			if (op == MIPS_MAKE_JR_RA()) {
				// If a branch goes to the jr ra, it's still ending here.
				if (furthestBranch > addr) {
					looking = true;
					addr += 4;
				} else {
					end = true;
				}
			}

	/*		if (looking) {
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
			*/
			if (end) {
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
				symbolMap.AddFunction(DefaultFunctionName(temp, iter->start), iter->start, iter->end - iter->start + 4);
			}
		}
	}


	enum BranchType { NONE, JUMP, BRANCH };
	struct BranchInfo
	{
		BranchType type;
		bool link;
		bool likely;
		bool toRegister;
	};

	bool getBranchInfo(MipsOpcodeInfo& info)
	{
		BranchType type = NONE;
		bool link = false;
		bool likely = false;
		bool toRegister = false;

		u32 op = info.encodedOpcode;
		switch (MIPS_GET_OP(op))
		{
		case 0x00:		// special
			switch (MIPS_GET_FUNC(op))
			{
			case 0x08:	// jr
				type = JUMP;
				toRegister = true;
				break;
			case 0x09:	// jalr
				type = JUMP;
				toRegister = true;
				link = true;
				break;
			}
			break;
		case 0x01:		// regimm
			switch (MIPS_GET_RT(op))
			{
			case 0x00:		// bltz
			case 0x01:		// bgez
				type = BRANCH;
				break;
			case 0x02:		// bltzl
			case 0x03:		// bgezl
				type = BRANCH;
				likely = true;
				break;
			case 0x10:		// bltzal
			case 0x11:		// bgezal
				type = BRANCH;
				link = true;
				break;
			case 0x12:		// bltzall
			case 0x13:		// bgezall
				type = BRANCH;
				likely = true;
				link = true;
				break;
			}
			break;
		case 0x02:		// j
			type = JUMP;
			break;
		case 0x03:		// jal
			type = JUMP;
			link = true;
			break;
		case 0x04:		// beq
		case 0x05:		// bne
		case 0x06:		// blez
		case 0x07:		// bgtz
			type = BRANCH;
			break;
		case 0x14:		// beql
		case 0x15:		// bnel
		case 0x16:		// blezl
		case 0x17:		// bgtzl
			type = BRANCH;
			likely = true;
			break;
		}

		if (type == NONE)
			return false;

		info.isBranch = true;
		info.isLinkedBranch = link;
		info.isLikelyBranch = true;
		info.isBranchToRegister = toRegister;
		info.isConditional = type == BRANCH;
		
		switch (type)
		{
		case JUMP:
			if (toRegister)
			{
				info.branchRegisterNum = (int)MIPS_GET_RS(op);
				info.branchTarget = info.cpu->getGPR(info.branchRegisterNum)._u32[0];
			} else {
				info.branchTarget =  (info.opcodeAddress & 0xF0000000) | ((op&0x03FFFFFF) << 2);
			}
			break;
		case BRANCH:
			info.branchTarget = info.opcodeAddress + 4 + ((signed short)(op&0xFFFF)<<2);
			break;
		}

		return true;
	}

	MipsOpcodeInfo GetOpcodeInfo(DebugInterface* cpu, u32 address) {
		MipsOpcodeInfo info;
		memset(&info, 0, sizeof(info));

		if (isValidAddress(address) == false) {
			return info;
		}

		info.cpu = cpu;
		info.opcodeAddress = address;
		info.encodedOpcode = cpu->read32(address);
		u32 op = info.encodedOpcode;

		if (getBranchInfo(info) == true)
			return info;

		// gather relevant address for alu operations
		// that's usually the value of the dest register
		switch (MIPS_GET_OP(op)) {
		case 0:		// special
			switch (MIPS_GET_FUNC(op)) {
			case 0x20:	// add
			case 0x21:	// addu
				info.hasRelevantAddress = true;
				info.releventAddress = cpu->getGPR(MIPS_GET_RS(op))._u32[0]+cpu->getGPR(MIPS_GET_RT(op))._u32[0];
				break;
			case 0x22:	// sub
			case 0x23:	// subu
				info.hasRelevantAddress = true;
				info.releventAddress = cpu->getGPR(MIPS_GET_RS(op))._u32[0]-cpu->getGPR(MIPS_GET_RT(op))._u32[0];
				break;
			}
			break;
		case 0x08:	// addi
		case 0x09:	// adiu
			info.hasRelevantAddress = true;
			info.releventAddress = cpu->getGPR(MIPS_GET_RS(op))._u32[0]+((s16)(op & 0xFFFF));
			break;
		}

		// TODO: rest
/*		// movn, movz
		if (opInfo & IS_CONDMOVE) {
			info.isConditional = true;

			u32 rt = cpu->GetRegValue(0, (int)MIPS_GET_RT(op));
			switch (opInfo & CONDTYPE_MASK) {
			case CONDTYPE_EQ:
				info.conditionMet = (rt == 0);
				break;
			case CONDTYPE_NE:
				info.conditionMet = (rt != 0);
				break;
			}
		}

		// beq, bgtz, ...
		if (opInfo & IS_CONDBRANCH) {
			info.isBranch = true;
			info.isConditional = true;
			info.branchTarget = GetBranchTarget(address);

			if (opInfo & OUT_RA) {  // link
				info.isLinkedBranch = true;
			}

			u32 rt = cpu->GetRegValue(0, (int)MIPS_GET_RT(op));
			u32 rs = cpu->GetRegValue(0, (int)MIPS_GET_RS(op));
			switch (opInfo & CONDTYPE_MASK) {
			case CONDTYPE_EQ:
				if (opInfo & IN_FPUFLAG) {	// fpu branch
					info.conditionMet = currentMIPS->fpcond == 0;
				} else {
					info.conditionMet = (rt == rs);
					if (MIPS_GET_RT(op) == MIPS_GET_RS(op))	{	// always true
						info.isConditional = false;
					}
				}
				break;
			case CONDTYPE_NE:
				if (opInfo & IN_FPUFLAG) {	// fpu branch
					info.conditionMet = currentMIPS->fpcond != 0;
				} else {
					info.conditionMet = (rt != rs);
					if (MIPS_GET_RT(op) == MIPS_GET_RS(op))	{	// always true
						info.isConditional = false;
					}
				}
				break;
			case CONDTYPE_LEZ:
				info.conditionMet = (((s32)rs) <= 0);
				break;
			case CONDTYPE_GTZ:
				info.conditionMet = (((s32)rs) > 0);
				break;
			case CONDTYPE_LTZ:
				info.conditionMet = (((s32)rs) < 0);
				break;
			case CONDTYPE_GEZ:
				info.conditionMet = (((s32)rs) >= 0);
				break;
			}
		}

		// lw, sh, ...
		if ((opInfo & IN_MEM) || (opInfo & OUT_MEM)) {
			info.isDataAccess = true;
			switch (opInfo & MEMTYPE_MASK) {
			case MEMTYPE_BYTE:
				info.dataSize = 1;
				break;
			case MEMTYPE_HWORD:
				info.dataSize = 2;
				break;
			case MEMTYPE_WORD:
			case MEMTYPE_FLOAT:
				info.dataSize = 4;
				break;

			case MEMTYPE_VQUAD:
				info.dataSize = 16;
			}

			u32 rs = cpu->GetRegValue(0, (int)MIPS_GET_RS(op));
			s16 imm16 = op & 0xFFFF;
			info.dataAddress = rs + imm16;

			info.hasRelevantAddress = true;
			info.releventAddress = info.dataAddress;
		}*/

		return info;
	}
}
