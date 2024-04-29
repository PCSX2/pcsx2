// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DebugInterface.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "Debug.h"
#include "VU.h"
#include "GS.h" // Required for gsNonMirroredRead()
#include "Counters.h"

#include "R3000A.h"
#include "IopMem.h"
#include "SymbolMap.h"
#include "VMManager.h"

#include "common/StringUtil.h"

#ifdef __clang__
// TODO: The sprintf() usage here needs to be rewritten...
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

R5900DebugInterface r5900Debug;
R3000DebugInterface r3000Debug;

enum ReferenceIndexType
{
	REF_INDEX_PC       = 32,
	REF_INDEX_HI       = 33,
	REF_INDEX_LO       = 34,
	REF_INDEX_OPTARGET = 0x800,
	REF_INDEX_OPSTORE  = 0x1000,
	REF_INDEX_OPLOAD   = 0x2000,
	REF_INDEX_IS_OPSL  = REF_INDEX_OPTARGET | REF_INDEX_OPSTORE | REF_INDEX_OPLOAD,
	REF_INDEX_FPU      = 0x4000,
	REF_INDEX_FPU_INT  = 0x8000,
	REF_INDEX_VFPU     = 0x10000,
	REF_INDEX_VFPU_INT = 0x20000,
	REF_INDEX_IS_FLOAT = REF_INDEX_FPU | REF_INDEX_VFPU,

};


class MipsExpressionFunctions : public IExpressionFunctions
{
public:
	explicit MipsExpressionFunctions(DebugInterface* cpu)
		: cpu(cpu){};

	virtual bool parseReference(char* str, u64& referenceIndex)
	{
		for (int i = 0; i < 32; i++)
		{
			char reg[8];
			std::snprintf(reg, std::size(reg), "r%d", i);

			if (StringUtil::Strcasecmp(str, reg) == 0 || StringUtil::Strcasecmp(str, cpu->getRegisterName(0, i)) == 0)
			{
				referenceIndex = i;
				return true;
			}
		}

		if (StringUtil::Strcasecmp(str, "pc") == 0)
		{
			referenceIndex = REF_INDEX_PC;
			return true;
		}

		if (StringUtil::Strcasecmp(str, "hi") == 0)
		{
			referenceIndex = REF_INDEX_HI;
			return true;
		}

		if (StringUtil::Strcasecmp(str, "lo") == 0)
		{
			referenceIndex = REF_INDEX_LO;
			return true;
		}

		if (StringUtil::Strcasecmp(str, "target") == 0)
		{
			referenceIndex = REF_INDEX_OPTARGET;
			return true;
		}

		if (StringUtil::Strcasecmp(str, "load") == 0)
		{
			referenceIndex = REF_INDEX_OPLOAD;
			return true;
		}

		if (StringUtil::Strcasecmp(str, "store") == 0)
		{
			referenceIndex = REF_INDEX_OPSTORE;
			return true;
		}
		return false;
	}

	virtual bool parseSymbol(char* str, u64& symbolValue)
	{
		u32 value;
		bool result = cpu->GetSymbolMap().GetLabelValue(str, value);
		symbolValue = value;
		return result;
	}

	virtual u64 getReferenceValue(u64 referenceIndex)
	{
		if (referenceIndex < 32)
			return cpu->getRegister(0, referenceIndex)._u64[0];
		if (referenceIndex == REF_INDEX_PC)
			return cpu->getPC();
		if (referenceIndex == REF_INDEX_HI)
			return cpu->getHI()._u64[0];
		if (referenceIndex == REF_INDEX_LO)
			return cpu->getLO()._u64[0];
		if (referenceIndex & REF_INDEX_IS_OPSL)
		{
			const u32 OP = memRead32(cpu->getPC());
			const R5900::OPCODE& opcode = R5900::GetInstruction(OP);
			if (opcode.flags & IS_MEMORY)
			{
				// Fetch the address in the base register 
				u32 target = cpuRegs.GPR.r[(OP >> 21) & 0x1F].UD[0];
				// Add the offset (lower 16 bits)
				target += static_cast<u16>(OP);

				if (referenceIndex & REF_INDEX_OPTARGET)
				{
					return target;
				}
				else if (referenceIndex & REF_INDEX_OPLOAD)
				{
					return (opcode.flags & IS_LOAD) ? target : 0;
				}
				else if (referenceIndex & REF_INDEX_OPSTORE)
				{
					return (opcode.flags & IS_STORE) ? target : 0;
				}
			}
			return 0;
		}
		return -1;
	}

	virtual ExpressionType getReferenceType(u64 referenceIndex)
	{
		if (referenceIndex & REF_INDEX_IS_FLOAT)
		{
			return EXPR_TYPE_FLOAT;
		}
		return EXPR_TYPE_UINT;
	}

	virtual bool getMemoryValue(u32 address, int size, u64& dest, char* error)
	{
		switch (size)
		{
			case 1:
			case 2:
			case 4:
			case 8:
				break;
			default:
				sprintf(error, "Invalid memory access size %d", size);
				return false;
		}

		if (address % size)
		{
			sprintf(error, "Invalid memory access (unaligned)");
			return false;
		}

		switch (size)
		{
			case 1:
				dest = cpu->read8(address);
				break;
			case 2:
				dest = cpu->read16(address);
				break;
			case 4:
				dest = cpu->read32(address);
				break;
			case 8:
				dest = cpu->read64(address);
				break;
		}

		return true;
	}

private:
	DebugInterface* cpu;
};

//
// DebugInterface
//

bool DebugInterface::m_pause_on_entry = false;

bool DebugInterface::isAlive()
{
	return VMManager::HasValidVM() && g_FrameCount > 0;
}

bool DebugInterface::isCpuPaused()
{
	return VMManager::GetState() == VMState::Paused;
}

void DebugInterface::pauseCpu()
{
	VMManager::SetPaused(true);
}

void DebugInterface::resumeCpu()
{
	VMManager::SetPaused(false);
}

char* DebugInterface::stringFromPointer(u32 p)
{
	const int BUFFER_LEN = 25;
	static char buf[BUFFER_LEN] = {0};

	if (!isValidAddress(p))
		return NULL;

	// This is going to blow up if it hits a TLB miss..
	// Hopefully the checks in isValidAddress() are sufficient.
	for (u32 i = 0; i < BUFFER_LEN; i++)
	{
		char c = read8(p + i);
		buf[i] = c;

		if (c == 0)
		{
			return i > 0 ? buf : NULL;
		}
		else if (c < 0x20 || c >= 0x7f)
		{
			// non printable character
			return NULL;
		}
	}

	buf[BUFFER_LEN - 1] = 0;
	buf[BUFFER_LEN - 2] = '~';
	return buf;
}

bool DebugInterface::initExpression(const char* exp, PostfixExpression& dest)
{
	MipsExpressionFunctions funcs(this);
	return initPostfixExpression(exp, &funcs, dest);
}

bool DebugInterface::parseExpression(PostfixExpression& exp, u64& dest)
{
	MipsExpressionFunctions funcs(this);
	return parsePostfixExpression(exp, &funcs, dest);
}


//
// R5900DebugInterface
//

BreakPointCpu R5900DebugInterface::getCpuType()
{
	return BREAKPOINT_EE;
}

u32 R5900DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;

	return memRead8(address);
}

u32 R5900DebugInterface::read8(u32 address, bool& valid)
{
	if (!(valid = isValidAddress(address)))
		return -1;

	return memRead8(address);
}


u32 R5900DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address) || address % 2)
		return -1;

	return memRead16(address);
}

u32 R5900DebugInterface::read16(u32 address, bool& valid)
{
	if (!(valid = (isValidAddress(address) || address % 2)))
		return -1;

	return memRead16(address);
}

u32 R5900DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address) || address % 4)
		return -1;

	return memRead32(address);
}

u32 R5900DebugInterface::read32(u32 address, bool& valid)
{
	if (!(valid = (isValidAddress(address) || address % 4)))
		return -1;

	return memRead32(address);
}

u64 R5900DebugInterface::read64(u32 address)
{
	if (!isValidAddress(address) || address % 8)
		return -1;

	return memRead64(address);
}

u64 R5900DebugInterface::read64(u32 address, bool& valid)
{
	if (!(valid = (isValidAddress(address) || address % 8)))
		return -1;

	return memRead64(address);
}

u128 R5900DebugInterface::read128(u32 address)
{
	alignas(16) u128 result;
	if (!isValidAddress(address) || address % 16)
	{
		result.hi = result.lo = -1;
		return result;
	}

	memRead128(address, result);
	return result;
}

void R5900DebugInterface::write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return;

	memWrite8(address, value);
}

void R5900DebugInterface::write32(u32 address, u32 value)
{
	if (!isValidAddress(address))
		return;

	memWrite32(address, value);
}


int R5900DebugInterface::getRegisterCategoryCount()
{
	return EECAT_COUNT;
}

const char* R5900DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
		case EECAT_GPR:
			return "GPR";
		case EECAT_CP0:
			return "CP0";
		case EECAT_FPR:
			return "FPR";
		case EECAT_FCR:
			return "FCR";
		case EECAT_VU0F:
			return "VU0f";
		case EECAT_VU0I:
			return "VU0i";
		case EECAT_GSPRIV:
			return "GS";
		default:
			return "Invalid";
	}
}

int R5900DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
		case EECAT_GPR:
		case EECAT_VU0F:
			return 128;
		case EECAT_CP0:
		case EECAT_FPR:
		case EECAT_FCR:
		case EECAT_VU0I:
			return 32;
		case EECAT_GSPRIV:
			return 64;
		default:
			return 0;
	}
}

int R5900DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
		case EECAT_GPR:
			return 35; // 32 + pc + hi + lo
		case EECAT_CP0:
		case EECAT_FPR:
		case EECAT_FCR:
		case EECAT_VU0I:
			return 32;
		case EECAT_VU0F:
			return 33; // 32 + ACC
		case EECAT_GSPRIV:
			return 19;
		default:
			return 0;
	}
}

DebugInterface::RegisterType R5900DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
		case EECAT_GPR:
		case EECAT_CP0:
		case EECAT_VU0I:
		case EECAT_FCR:
		case EECAT_GSPRIV:
		default:
			return NORMAL;
		case EECAT_FPR:
		case EECAT_VU0F:
			return SPECIAL;
	}
}

const char* R5900DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
		case EECAT_GPR:
			switch (num)
			{
				case 32: // pc
					return "pc";
				case 33: // hi
					return "hi";
				case 34: // lo
					return "lo";
				default:
					return R5900::GPR_REG[num];
			}
		case EECAT_CP0:
			return R5900::COP0_REG[num];
		case EECAT_FPR:
			return R5900::COP1_REG_FP[num];
		case EECAT_FCR:
			return R5900::COP1_REG_FCR[num];
		case EECAT_VU0F:
			switch (num)
			{
				case 32: // ACC
					return "ACC";
				default:
					return R5900::COP2_REG_FP[num];
			}
		case EECAT_VU0I:
			return R5900::COP2_REG_CTL[num];
		case EECAT_GSPRIV:
			return R5900::GS_REG_PRIV[num];
		default:
			return "Invalid";
	}
}

u128 R5900DebugInterface::getRegister(int cat, int num)
{
	u128 result;
	switch (cat)
	{
		case EECAT_GPR:
			switch (num)
			{
				case 32: // pc
					result = u128::From32(cpuRegs.pc);
					break;
				case 33: // hi
					result = cpuRegs.HI.UQ;
					break;
				case 34: // lo
					result = cpuRegs.LO.UQ;
					break;
				default:
					result = cpuRegs.GPR.r[num].UQ;
					break;
			}
			break;
		case EECAT_CP0:
			result = u128::From32(cpuRegs.CP0.r[num]);
			break;
		case EECAT_FPR:
			result = u128::From32(fpuRegs.fpr[num].UL);
			break;
		case EECAT_FCR:
			result = u128::From32(fpuRegs.fprc[num]);
			break;
		case EECAT_VU0F:
			switch (num)
			{
				case 32: // ACC
					result = VU0.ACC.UQ;
					break;
				default:
					result = VU0.VF[num].UQ;
					break;
			}
			break;
		case EECAT_VU0I:
			result = u128::From32(VU0.VI[num].UL);
			break;
		case EECAT_GSPRIV:
			result = gsNonMirroredRead(0x12000000 | R5900::GS_REG_PRIV_ADDR[num]);
			break;
		default:
			result = u128::From32(0);
			break;
	}

	return result;
}

std::string R5900DebugInterface::getRegisterString(int cat, int num)
{
	switch (cat)
	{
		case EECAT_GPR:
		case EECAT_CP0:
		case EECAT_FCR:
		case EECAT_VU0F:
			return StringUtil::U128ToString(getRegister(cat, num));
		case EECAT_FPR:
			return StringUtil::StdStringFromFormat("%f", fpuRegs.fpr[num].f);
		default:
			return {};
	}
}


u128 R5900DebugInterface::getHI()
{
	return cpuRegs.HI.UQ;
}

u128 R5900DebugInterface::getLO()
{
	return cpuRegs.LO.UQ;
}

u32 R5900DebugInterface::getPC()
{
	return cpuRegs.pc;
}

// Taken from COP0.cpp
bool R5900DebugInterface::getCPCOND0()
{
	return (((dmacRegs.stat.CIS | ~dmacRegs.pcr.CPC) & 0x3FF) == 0x3ff);
}

void R5900DebugInterface::setPc(u32 newPc)
{
	cpuRegs.pc = newPc;
}

void R5900DebugInterface::setRegister(int cat, int num, u128 newValue)
{
	switch (cat)
	{
		case EECAT_GPR:
			switch (num)
			{
				case 32: // pc
					cpuRegs.pc = newValue._u32[0];
					break;
				case 33: // hi
					cpuRegs.HI.UQ = newValue;
					break;
				case 34: // lo
					cpuRegs.LO.UQ = newValue;
					break;
				default:
					cpuRegs.GPR.r[num].UQ = newValue;
					break;
			}
			break;
		case EECAT_CP0:
			cpuRegs.CP0.r[num] = newValue._u32[0];
			break;
		case EECAT_FPR:
			fpuRegs.fpr[num].UL = newValue._u32[0];
			break;
		case EECAT_FCR:
			fpuRegs.fprc[num] = newValue._u32[0];
			break;
		case EECAT_VU0F:
			switch (num)
			{
				case 32: // ACC
					VU0.ACC.UQ = newValue;
					break;
				default:
					VU0.VF[num].UQ = newValue;
					break;
			}
			break;
		case EECAT_VU0I:
			VU0.VI[num].UL = newValue._u32[0];
			break;
		case EECAT_GSPRIV:
			memWrite64(0x12000000 | R5900::GS_REG_PRIV_ADDR[num], newValue.lo);
			break;
		default:
			break;
	}
}

std::string R5900DebugInterface::disasm(u32 address, bool simplify)
{
	std::string out;

	u32 op = read32(address);
	R5900::disR5900Fasm(out, op, address, simplify);
	return out;
}

bool R5900DebugInterface::isValidAddress(u32 addr)
{
	u32 lopart = addr & 0xfFFffFF;

	// get rid of ee ram mirrors
	switch (addr >> 28)
	{
		case 0:
		case 2:
			// case 3: throw exception (not mapped ?)
			// [ 0000_8000 - 01FF_FFFF ] RAM
			// [ 2000_8000 - 21FF_FFFF ] RAM MIRROR
			// [ 3000_8000 - 31FF_FFFF ] RAM MIRROR
			if (lopart >= 0x80000 && lopart < Ps2MemSize::ExposedRam)
				return !!vtlb_GetPhyPtr(lopart);
			break;
		case 1:
			// [ 1000_0000 - 1000_CFFF ] EE register
			if (lopart <= 0xcfff)
				return true;

			// [ 1100_0000 - 1100_FFFF ] VU mem
			if (lopart >= 0x1000000 && lopart <= 0x100FFff)
				return true;

			// [ 1200_0000 - 1200_FFFF ] GS regs
			if (lopart >= 0x2000000 && lopart <= 0x20010ff)
				return true;

			// [ 1E00_0000 - 1FFF_FFFF ] ROM
			// if (lopart >= 0xe000000)
			// 	return true; throw exception (not mapped ?)
			break;
		case 7:
			// [ 7000_0000 - 7000_3FFF ] Scratchpad
			if (lopart <= 0x3fff)
				return true;
			break;
		case 8:
		case 9:
		case 0xA:
		case 0xB:
			// [ 8000_0000 - BFFF_FFFF ] kernel
			if (lopart >= 0xFC00000)
				return true;
		case 0xF:
			// [ 8000_0000 - BFFF_FFFF ] IOP or kernel stack
			if (lopart >= 0xfff8000)
				return true;
			break;
	}

	return false;
}

u32 R5900DebugInterface::getCycles()
{
	return cpuRegs.cycle;
}

SymbolMap& R5900DebugInterface::GetSymbolMap() const
{
	return R5900SymbolMap;
}

std::vector<std::unique_ptr<BiosThread>> R5900DebugInterface::GetThreadList() const
{
	return getEEThreads();
}


//
// R3000DebugInterface
//


BreakPointCpu R3000DebugInterface::getCpuType()
{
	return BREAKPOINT_IOP;
}

u32 R3000DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead8(address);
}

u32 R3000DebugInterface::read8(u32 address, bool& valid)
{
	if (!(valid = isValidAddress(address)))
		return -1;
	return iopMemRead8(address);
}

u32 R3000DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead16(address);
}

u32 R3000DebugInterface::read16(u32 address, bool& valid)
{
	if (!(valid = isValidAddress(address)))
		return -1;
	return iopMemRead16(address);
}

u32 R3000DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return iopMemRead32(address);
}

u32 R3000DebugInterface::read32(u32 address, bool& valid)
{
	if (!(valid = isValidAddress(address)))
		return -1;
	return iopMemRead32(address);

}

u64 R3000DebugInterface::read64(u32 address)
{
	return 0;
}

u64 R3000DebugInterface::read64(u32 address, bool& valid)
{
	return 0;
}


u128 R3000DebugInterface::read128(u32 address)
{
	return u128::From32(0);
}

void R3000DebugInterface::write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return;

	iopMemWrite8(address, value);
}

void R3000DebugInterface::write32(u32 address, u32 value)
{
	if (!isValidAddress(address))
		return;

	iopMemWrite32(address, value);
}

int R3000DebugInterface::getRegisterCategoryCount()
{
	return IOPCAT_COUNT;
}

const char* R3000DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			return "GPR";
		default:
			return "Invalid";
	}
}

int R3000DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			return 32;
		default:
			return 0;
	}
}

int R3000DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			return 35; // 32 + pc + hi + lo
		default:
			return 0;
	}
}

DebugInterface::RegisterType R3000DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
		case IOPCAT_GPR:
		default:
			return DebugInterface::NORMAL;
	}
}

const char* R3000DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			switch (num)
			{
				case 32: // pc
					return "pc";
				case 33: // hi
					return "hi";
				case 34: // lo
					return "lo";
				default:
					return R5900::GPR_REG[num];
			}
		default:
			return "Invalid";
	}
}

u128 R3000DebugInterface::getRegister(int cat, int num)
{
	u32 value;

	switch (cat)
	{
		case IOPCAT_GPR:
			switch (num)
			{
				case 32: // pc
					value = psxRegs.pc;
					break;
				case 33: // hi
					value = psxRegs.GPR.n.hi;
					break;
				case 34: // lo
					value = psxRegs.GPR.n.lo;
					break;
				default:
					value = psxRegs.GPR.r[num];
					break;
			}
			break;
		default:
			value = -1;
			break;
	}

	return u128::From32(value);
}

std::string R3000DebugInterface::getRegisterString(int cat, int num)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			return StringUtil::U128ToString(getRegister(cat, num));
		default:
			return "Invalid";
	}
}

u128 R3000DebugInterface::getHI()
{
	return u128::From32(psxRegs.GPR.n.hi);
}

u128 R3000DebugInterface::getLO()
{
	return u128::From32(psxRegs.GPR.n.lo);
}

u32 R3000DebugInterface::getPC()
{
	return psxRegs.pc;
}

bool R3000DebugInterface::getCPCOND0()
{
	return false;
}

void R3000DebugInterface::setPc(u32 newPc)
{
	psxRegs.pc = newPc;
}

void R3000DebugInterface::setRegister(int cat, int num, u128 newValue)
{
	switch (cat)
	{
		case IOPCAT_GPR:
			switch (num)
			{
				case 32: // pc
					psxRegs.pc = newValue._u32[0];
					break;
				case 33: // hi
					psxRegs.GPR.n.hi = newValue._u32[0];
					break;
				case 34: // lo
					psxRegs.GPR.n.lo = newValue._u32[0];
					break;
				default:
					psxRegs.GPR.r[num] = newValue._u32[0];
					break;
			}
			break;
		default:
			break;
	}
}

std::string R3000DebugInterface::disasm(u32 address, bool simplify)
{
	std::string out;

	u32 op = read32(address);
	R5900::disR5900Fasm(out, op, address, simplify);
	return out;
}

bool R3000DebugInterface::isValidAddress(u32 addr)
{
	if (addr >= 0x1D000000 && addr < 0x1E000000)
	{
		return true;
	}

	if (addr >= 0x1F400000 && addr < 0x1FA00000)
	{
		return true;
	}

	if (addr < 0x200000)
	{
		return true;
	}

	return false;
}

u32 R3000DebugInterface::getCycles()
{
	return psxRegs.cycle;
}

SymbolMap& R3000DebugInterface::GetSymbolMap() const
{
	return R3000SymbolMap;
}

std::vector<std::unique_ptr<BiosThread>> R3000DebugInterface::GetThreadList() const
{
	return getIOPThreads();
}
