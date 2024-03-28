// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "SymbolGuardian.h"

class DebugInterface;


#define MIPS_GET_OP(op)   ((op>>26) & 0x3F)
#define MIPS_GET_FUNC(op) (op & 0x3F)
#define MIPS_GET_SA(op)   ((op>>6) & 0x1F)

#define MIPS_GET_RS(op) ((op>>21) & 0x1F)
#define MIPS_GET_RT(op) ((op>>16) & 0x1F)
#define MIPS_GET_RD(op) ((op>>11) & 0x1F)

namespace MIPSAnalyst
{
	struct AnalyzedFunction {
		u32 start;
		u32 end;
		u64 hash;
		bool isStraightLeaf;
		bool hasHash;
		bool suspectedNoReturn;
		bool usesVFPU;
		char name[64];
	};

	void ScanForFunctions(ccc::SymbolDatabase& database, u32 startAddr, u32 endAddr);

	enum LoadStoreLRType { LOADSTORE_NORMAL, LOADSTORE_LEFT, LOADSTORE_RIGHT };

	typedef struct {
		DebugInterface* cpu;
		u32 opcodeAddress;
		u32 encodedOpcode;

		// shared between branches and conditional moves
		bool isConditional;
		bool conditionMet;

		// branches
		u32 branchTarget;
		bool isSyscall;
		bool isBranch;
		bool isLinkedBranch;
		bool isLikelyBranch;
		bool isBranchToRegister;
		int branchRegisterNum;

		// data access
		bool isDataAccess;
		LoadStoreLRType lrType;
		int dataSize;
		u32 dataAddress;

		bool hasRelevantAddress;
		u32 releventAddress;
	} MipsOpcodeInfo;

	MipsOpcodeInfo GetOpcodeInfo(DebugInterface* cpu, u32 address);
};
