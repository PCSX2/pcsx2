/*
 * Copyright (C) 2014-2014  PCSX2 Dev Team
 *
 * Imported from PPSSPP
 *
 * PCSX2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * PCSX2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"
#include "MipsStackWalk.h"
#include "SymbolMap.h"
#include "MIPSAnalyst.h"
#include "DebugInterface.h"
#include "R5900OpcodeTables.h"

#define _RS ((rawOp >> 21) & 0x1F)
#define _RT ((rawOp >> 16) & 0x1F)
#define _RD ((rawOp >> 11) & 0x1F)
#define _IMM16 ((signed short)(rawOp & 0xFFFF))
#define MIPS_REG_SP 29
#define MIPS_REG_FP 30
#define MIPS_REG_RA 31

#define INVALIDTARGET 0xFFFFFFFF

namespace MipsStackWalk {
	// In the worst case, we scan this far above the pc for an entry.
	const int MAX_FUNC_SIZE = 32768 * 4;
	// After this we assume we're stuck.
	const size_t MAX_DEPTH = 1024;

	static u32 GuessEntry(DebugInterface* cpu, u32 pc) {
		SymbolInfo info;
		if (cpu->GetSymbolMap().GetSymbolInfo(&info, pc)) {
			return info.address;
		}
		return INVALIDTARGET;
	}

	bool IsSWInstr(const R5900::OPCODE& op) {
		if ((op.flags & IS_MEMORY) && (op.flags & IS_STORE))
		{
			switch (op.flags & MEMTYPE_MASK)
			{
			case MEMTYPE_WORD:
			case MEMTYPE_DWORD:
			case MEMTYPE_QWORD:
				return true;
			}
		}

		return false;
	}

	bool IsAddImmInstr(const R5900::OPCODE& op) {
		if (op.flags & IS_ALU)
			return (op.flags & ALUTYPE_MASK) == ALUTYPE_ADDI;

		return false;
	}

	bool IsMovRegsInstr(const R5900::OPCODE& op, u32 rawOp) {
		if (op.flags & IS_ALU)
			return (op.flags & ALUTYPE_MASK) == ALUTYPE_ADDI && (_RS == 0 || _RT == 0);

		return false;
	}

	bool ScanForAllocaSignature(DebugInterface* cpu, u32 pc) {
		// In God Eater Burst, for example, after 0880E750, there's what looks like an alloca().
		// It's surrounded by "mov fp, sp" and "mov sp, fp", which is unlikely to be used for other reasons.

		// It ought to be pretty close.
		u32 stop = pc - 32 * 4;
		for (; cpu->isValidAddress(pc) && pc >= stop; pc -= 4) {
			u32 rawOp = cpu->read32(pc);
			const R5900::OPCODE& op = R5900::GetInstruction(rawOp);

			// We're looking for a "mov fp, sp" close by a "addiu sp, sp, -N".
			if (IsMovRegsInstr(op,rawOp) && _RD == MIPS_REG_FP && (_RS == MIPS_REG_SP || _RT == MIPS_REG_SP)) {
				return true;
			}
		}
		return false;
	}

	bool ScanForEntry(DebugInterface* cpu, StackFrame &frame, u32 entry, u32 &ra) {
		// Let's hope there are no > 1MB functions on the PSP, for the sake of humanity...
		const u32 LONGEST_FUNCTION = 1024 * 1024;
		// TODO: Check if found entry is in the same symbol?  Might be wrong sometimes...

		int ra_offset = -1;
		const u32 start = frame.pc;
		u32 stop = entry;
		if (entry == INVALIDTARGET) {
/*			if (start >= PSP_GetUserMemoryBase()) {
				stop = PSP_GetUserMemoryBase();
			} else if (start >= PSP_GetKernelMemoryBase()) {
				stop = PSP_GetKernelMemoryBase();
			} else if (start >= PSP_GetScratchpadMemoryBase()) {
				stop = PSP_GetScratchpadMemoryBase();
			}*/
			stop = 0x80000;
		}
		if (stop < start - LONGEST_FUNCTION) {
			stop = start - LONGEST_FUNCTION;
		}
		for (u32 pc = start; cpu->isValidAddress(pc) && pc >= stop; pc -= 4) {
			u32 rawOp = cpu->read32(pc);
			const R5900::OPCODE& op = R5900::GetInstruction(rawOp);

			// Here's where they store the ra address.
			if (IsSWInstr(op) && _RT == MIPS_REG_RA && _RS == MIPS_REG_SP) {
				ra_offset = _IMM16;
			}

			if (IsAddImmInstr(op) && _RT == MIPS_REG_SP && _RS == MIPS_REG_SP) {
				// A positive imm either means alloca() or we went too far.
				if (_IMM16 > 0) {
					// TODO: Maybe check for any alloca() signature and bail?
					continue;
				}
				if (ScanForAllocaSignature(cpu,pc)) {
					continue;
				}

				frame.entry = pc;
				frame.stackSize = -_IMM16;
				if (ra_offset != -1 && cpu->isValidAddress(frame.sp + ra_offset)) {
					ra = cpu->read32(frame.sp + ra_offset);
				}
				return true;
			}
		}
		return false;
	}

	bool DetermineFrameInfo(DebugInterface* cpu, StackFrame &frame, u32 possibleEntry, u32 threadEntry, u32 &ra) {
		if (ScanForEntry(cpu, frame, possibleEntry, ra)) {
			// Awesome, found one that looks right.
			return true;
		} else if (ra != INVALIDTARGET && possibleEntry != INVALIDTARGET) {
			// Let's just assume it's a leaf.
			frame.entry = possibleEntry;
			frame.stackSize = 0;
			return true;
		}

		// Okay, we failed to get one.  Our possibleEntry could be wrong, it often is.
		// Let's just scan upward.
		u32 newPossibleEntry = frame.pc > threadEntry ? threadEntry : frame.pc - MAX_FUNC_SIZE;
		return ScanForEntry(cpu, frame, newPossibleEntry, ra);
	}

	std::vector<StackFrame> Walk(DebugInterface* cpu, u32 pc, u32 ra, u32 sp, u32 threadEntry, u32 threadStackTop) {
		std::vector<StackFrame> frames;
		StackFrame current;
		current.pc = pc;
		current.sp = sp;
		current.entry = INVALIDTARGET;
		current.stackSize = -1;

		u32 prevEntry = INVALIDTARGET;
		while (pc != threadEntry) {
			u32 possibleEntry = GuessEntry(cpu, current.pc);
			if (DetermineFrameInfo(cpu, current, possibleEntry, threadEntry, ra)) {
				frames.push_back(current);
				if (current.entry == threadEntry || GuessEntry(cpu, current.entry) == threadEntry) {
					break;
				}
				if (current.entry == prevEntry || frames.size() >= MAX_DEPTH) {
					// Recursion, means we're screwed.  Let's just give up.
					break;
				}
				prevEntry = current.entry;

				current.pc = ra;
				current.sp += current.stackSize;
				ra = INVALIDTARGET;
				current.entry = INVALIDTARGET;
				current.stackSize = -1;
			} else {
				// Well, we got as far as we could.
				current.entry = possibleEntry;
				current.stackSize = 0;
				frames.push_back(current);
				break;
			}
		}

		return frames;
	}
};
