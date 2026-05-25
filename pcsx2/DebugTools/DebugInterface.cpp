// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugInterface.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "Debug.h"
#include "VU.h"
#include "GS.h" // Required for gsNonMirroredRead()
#include "Counters.h"

#include "Host.h"
#include "R3000A.h"
#include "IopMem.h"
#include "VMManager.h"
#include "vtlb.h"

#include "common/StringUtil.h"

R5900DebugInterface r5900Debug;
R3000DebugInterface r3000Debug;

enum ReferenceIndexType
{
	REF_INDEX_PC = 32,
	REF_INDEX_HI = 33,
	REF_INDEX_LO = 34,
	REF_INDEX_OPTARGET = 0x800,
	REF_INDEX_OPSTORE = 0x1000,
	REF_INDEX_OPLOAD = 0x2000,
	REF_INDEX_IS_OPSL = REF_INDEX_OPTARGET | REF_INDEX_OPSTORE | REF_INDEX_OPLOAD,
	REF_INDEX_FPU = 0x4000,
	REF_INDEX_FPU_INT = 0x8000,
	REF_INDEX_VFPU = 0x10000,
	REF_INDEX_VFPU_INT = 0x20000,
	REF_INDEX_IS_FLOAT = REF_INDEX_FPU | REF_INDEX_VFPU,
};

//
// DebugInterface
//

bool DebugInterface::m_pause_on_entry = false;

bool DebugInterface::isAlive()
{
	return VMManager::HasValidVM();
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
	const int BUFFER_LEN = 64;
	static char buf[BUFFER_LEN] = {0};

	if (!isValidAddress(p))
		return NULL;

	// This is going to blow up if it hits a TLB miss..
	// Hopefully the checks in isValidAddress() are sufficient.
	for (u32 i = 0; i < BUFFER_LEN; i++)
	{
		char c = Read8(p + i);
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

std::optional<u32> DebugInterface::getCallerStackPointer(const ccc::Function& currentFunction)
{
	u32 sp = getRegister(EECAT_GPR, 29);
	u32 pc = getPC();

	if (pc != currentFunction.address().value)
	{
		std::optional<u32> stack_frame_size = getStackFrameSize(currentFunction);
		if (!stack_frame_size.has_value())
			return std::nullopt;

		sp += *stack_frame_size;
	}

	return sp;
}

std::optional<u32> DebugInterface::getStackFrameSize(const ccc::Function& function)
{
	s32 stack_frame_size = function.stack_frame_size;

	if (stack_frame_size < 0)
	{
		// The stack frame size isn't stored in the symbol table, so we try
		// to extract it from the code by checking for an instruction at the
		// start of the current function that is in the form of
		// "addiu $sp, $sp, frame_size" instead.

		u32 instruction = Read32(function.address().value);

		if ((instruction & 0xffff0000) == 0x27bd0000)
			stack_frame_size = -static_cast<s16>(instruction & 0xffff);

		if (stack_frame_size < 0)
			return std::nullopt;
	}

	return static_cast<u32>(stack_frame_size);
}

bool DebugInterface::evaluateExpression(const char* expression, u64& dest, std::string& error)
{
	PostfixExpression postfix;

	if (!initExpression(expression, postfix, error))
		return false;

	if (!parseExpression(postfix, dest, error))
		return false;

	return true;
}

bool DebugInterface::initExpression(const char* exp, PostfixExpression& dest, std::string& error)
{
	MipsExpressionFunctions funcs(this, nullptr, true);
	return initPostfixExpression(exp, &funcs, dest, error);
}

bool DebugInterface::parseExpression(PostfixExpression& exp, u64& dest, std::string& error)
{
	MipsExpressionFunctions funcs(this, nullptr, false);
	return parsePostfixExpression(exp, &funcs, dest, error);
}

DebugInterface& DebugInterface::get(BreakPointCpu cpu)
{
	switch (cpu)
	{
		case BREAKPOINT_EE:
			return r5900Debug;
		case BREAKPOINT_IOP:
			return r3000Debug;
		default:
		{
		}
	}

	pxFailRel("DebugInterface::get called with invalid cpu enum.");
	return r5900Debug;
}

const char* DebugInterface::cpuName(BreakPointCpu cpu)
{
	switch (cpu)
	{
		case BREAKPOINT_EE:
			return "EE";
		case BREAKPOINT_IOP:
			return "IOP";
		default:
		{
		}
	}

	pxFailRel("DebugInterface::cpuName called with invalid cpu enum.");
	return "";
}

const char* DebugInterface::longCpuName(BreakPointCpu cpu)
{
	switch (cpu)
	{
		case BREAKPOINT_EE:
			return TRANSLATE("DebugInterface", "Emotion Engine");
		case BREAKPOINT_IOP:
			return TRANSLATE("DebugInteface", "Input Output Processor");
		default:
		{
		}
	}

	pxFailRel("DebugInterface::longCpuName called with invalid cpu enum.");
	return "";
}

// *****************************************************************************

BreakPointCpu R5900DebugInterface::getCpuType()
{
	return BREAKPOINT_EE;
}

static int InvalidReadResult(bool* valid)
{
	if (valid)
		*valid = false;
	return -1;
}

u8 R5900DebugInterface::Read8(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u8>(InvalidReadResult(valid));

	u8 value;
	if (!vtlb_memSafeReadBytes(address, &value, sizeof(value)))
		return static_cast<u8>(InvalidReadResult(valid));

	if (valid)
		*valid = true;
	return value;
}

u16 R5900DebugInterface::Read16(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u16>(InvalidReadResult(valid));

	u16 value;
	if (!vtlb_memSafeReadBytes(address, &value, sizeof(value)))
		return static_cast<u16>(InvalidReadResult(valid));

	if (valid)
		*valid = true;
	return value;
}

u32 R5900DebugInterface::Read32(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u32>(InvalidReadResult(valid));

	u32 value;
	if (!vtlb_memSafeReadBytes(address, &value, sizeof(value)))
		return static_cast<u32>(InvalidReadResult(valid));

	if (valid)
		*valid = true;
	return value;
}

u64 R5900DebugInterface::Read64(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u64>(InvalidReadResult(valid));

	u64 value;
	if (!vtlb_memSafeReadBytes(address, &value, sizeof(value)))
		return static_cast<u64>(InvalidReadResult(valid));

	if (valid)
		*valid = true;
	return value;
}

static u128 InvalidReadResult128(bool* valid)
{
	if (valid)
		*valid = false;

	u128 value;
	value.lo = static_cast<u64>(-1);
	value.hi = static_cast<u64>(-1);
	return value;
}

u128 R5900DebugInterface::Read128(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return InvalidReadResult128(valid);

	u128 value;
	if (!vtlb_memSafeReadBytes(address, &value, sizeof(u128)))
		return InvalidReadResult128(valid);

	if (valid)
		*valid = true;

	return value;
}

bool R5900DebugInterface::ReadBytes(u32 address, void* dest, u32 size)
{
	return vtlb_memSafeReadBytes(address, dest, size);
}

bool R5900DebugInterface::Write8(u32 address, u8 value)
{
	return isValidAddress(address) && vtlb_memSafeWriteBytes(address, &value, sizeof(value));
}

bool R5900DebugInterface::Write16(u32 address, u16 value)
{
	return isValidAddress(address) && vtlb_memSafeWriteBytes(address, &value, sizeof(value));
}

bool R5900DebugInterface::Write32(u32 address, u32 value)
{
	return isValidAddress(address) && vtlb_memSafeWriteBytes(address, &value, sizeof(value));
}

bool R5900DebugInterface::Write64(u32 address, u64 value)
{
	return isValidAddress(address) && vtlb_memSafeWriteBytes(address, &value, sizeof(value));
}

bool R5900DebugInterface::Write128(u32 address, u128 value)
{
	return isValidAddress(address) && vtlb_memSafeWriteBytes(address, &value, sizeof(value));
}

bool R5900DebugInterface::WriteBytes(u32 address, void* src, u32 size)
{
	return vtlb_memSafeWriteBytes(address, src, size);
}

bool R5900DebugInterface::CompareBytes(u32 address, void* src, u32 size)
{
	return vtlb_memSafeCmpBytes(address, src, size) == 0;
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

	u32 op = Read32(address);
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
		case 0xA:
			if (lopart <= 0xFFFFF)
				return true;
			break;
		case 9:
		case 0xB:
			// [ 8000_0000 - BFFF_FFFF ] kernel
			if (lopart >= 0xFC00000)
				return true;
			break;
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

SymbolGuardian& R5900DebugInterface::GetSymbolGuardian() const
{
	return R5900SymbolGuardian;
}

SymbolImporter* R5900DebugInterface::GetSymbolImporter() const
{
	return &R5900SymbolImporter;
}

std::vector<std::unique_ptr<BiosThread>> R5900DebugInterface::GetThreadList() const
{
	return getEEThreads();
}

std::vector<MipsStackWalk::StackFrame> R5900DebugInterface::StackTrace(const BiosThread& thread)
{
	if (thread.Status() == ThreadStatus::THS_RUN)
	{
		return MipsStackWalk::Walk(this, getPC(), getRegister(0, 31), getRegister(0, 29),
			thread.EntryPoint());
	}

	EEInternalCtx* ctx = static_cast<EEInternalCtx*>(PSM(thread.RegCtx()));
	u32 pc = thread.PC();
	// $zero is not in the array so subtract 1
	u32 ra = ctx->gpr[31 - 1]._u32[0];
	u32 sp = ctx->gpr[29 - 1]._u32[0];

	return MipsStackWalk::Walk(this, pc, ra, sp, thread.EntryPoint());
}

std::vector<IopMod> R5900DebugInterface::GetModuleList() const
{
	return {};
}

//
// R3000DebugInterface
//

BreakPointCpu R3000DebugInterface::getCpuType()
{
	return BREAKPOINT_IOP;
}

u8 R3000DebugInterface::Read8(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u8>(InvalidReadResult(valid));

	u8 value = iopMemRead8(address);

	if (valid)
		*valid = true;
	return value;
}

u16 R3000DebugInterface::Read16(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u16>(InvalidReadResult(valid));

	u16 value = iopMemRead16(address);

	if (valid)
		*valid = true;
	return value;
}

u32 R3000DebugInterface::Read32(u32 address, bool* valid)
{
	if (!isValidAddress(address))
		return static_cast<u32>(InvalidReadResult(valid));

	u32 value = iopMemRead32(address);

	if (valid)
		*valid = true;
	return value;
}

u64 R3000DebugInterface::Read64(u32 address, bool* valid)
{
	return InvalidReadResult(valid);
}

u128 R3000DebugInterface::Read128(u32 address, bool* valid)
{
	return InvalidReadResult128(valid);
}

bool R3000DebugInterface::ReadBytes(u32 address, void* dest, u32 size)
{
	return iopMemSafeReadBytes(address, dest, size);
}

bool R3000DebugInterface::Write8(u32 address, u8 value)
{
	if (!isValidAddress(address))
		return false;

	iopMemWrite8(address, value);
	return true;
}

bool R3000DebugInterface::Write16(u32 address, u16 value)
{
	if (!isValidAddress(address))
		return false;

	iopMemWrite16(address, value);
	return true;
}

bool R3000DebugInterface::Write32(u32 address, u32 value)
{
	if (!isValidAddress(address))
		return false;

	iopMemWrite32(address, value);
	return true;
}

bool R3000DebugInterface::Write64(u32 address, u64 value)
{
	if (!isValidAddress(address))
		return false;

	iopMemWrite32(address + 0, value);
	iopMemWrite32(address + 4, value >> 32);
	return true;
}

bool R3000DebugInterface::Write128(u32 address, u128 value)
{
	if (!isValidAddress(address))
		return false;

	iopMemWrite32(address + 0x0, value._u32[0]);
	iopMemWrite32(address + 0x4, value._u32[1]);
	iopMemWrite32(address + 0x8, value._u32[2]);
	iopMemWrite32(address + 0xc, value._u32[3]);
	return true;
}

bool R3000DebugInterface::WriteBytes(u32 address, void* src, u32 size)
{
	return iopMemSafeWriteBytes(address, src, size);
}

bool R3000DebugInterface::CompareBytes(u32 address, void* src, u32 size)
{
	return iopMemSafeCmpBytes(address, src, size) == 0;
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

	u32 op = Read32(address);
	R5900::disR5900Fasm(out, op, address, simplify);
	return out;
}

bool R3000DebugInterface::isValidAddress(u32 addr)
{
	addr &= 0x1fffffff;
	if (addr >= 0x1D000000 && addr < 0x1E000000)
	{
		return true;
	}

	if (addr >= 0x1F400000 && addr < 0x1FA00000)
	{
		return true;
	}

	if (addr >= 0x1FC00000 && addr < 0x20000000)
	{
		return true;
	}

	if (addr < Ps2MemSize::ExposedIopRam)
	{
		return true;
	}

	return false;
}

u32 R3000DebugInterface::getCycles()
{
	return psxRegs.cycle;
}

SymbolGuardian& R3000DebugInterface::GetSymbolGuardian() const
{
	return R3000SymbolGuardian;
}

SymbolImporter* R3000DebugInterface::GetSymbolImporter() const
{
	return nullptr;
}

std::vector<std::unique_ptr<BiosThread>> R3000DebugInterface::GetThreadList() const
{
	return getIOPThreads();
}

std::vector<MipsStackWalk::StackFrame> R3000DebugInterface::StackTrace(const BiosThread& thread)
{
	if (thread.Status() == ThreadStatus::THS_RUN)
	{
		return MipsStackWalk::Walk(this, getPC(), getRegister(0, 31), getRegister(0, 29),
			thread.EntryPoint());
	}

	u32 p = thread.RegCtx();
	u32 pc = Read32(p + 0x8c);
	u32 ra = Read32(p + 0x7c);
	u32 sp = Read32(p + 0x74);

	return MipsStackWalk::Walk(this, pc, ra, sp, thread.EntryPoint());
}

std::vector<IopMod> R3000DebugInterface::GetModuleList() const
{
	return getIOPModules();
}

// *****************************************************************************

ElfMemoryReader::ElfMemoryReader(const ccc::ElfFile& elf)
	: m_elf(elf)
{
}

u8 ElfMemoryReader::Read8(u32 address, bool* valid)
{
	std::optional<u8> result = m_elf.get_object_virtual<u8>(address);
	if (!result.has_value())
		return InvalidReadResult(valid);

	if (valid)
		*valid = true;
	return *result;
}

u16 ElfMemoryReader::Read16(u32 address, bool* valid)
{
	std::optional<u16> result = m_elf.get_object_virtual<u16>(address);
	if (!result.has_value())
		return InvalidReadResult(valid);

	if (valid)
		*valid = true;
	return *result;
}

u32 ElfMemoryReader::Read32(u32 address, bool* valid)
{
	std::optional<u32> result = m_elf.get_object_virtual<u32>(address);
	if (!result.has_value())
		return InvalidReadResult(valid);

	if (valid)
		*valid = true;
	return *result;
}

u64 ElfMemoryReader::Read64(u32 address, bool* valid)
{
	std::optional<u64> result = m_elf.get_object_virtual<u64>(address);
	if (!result.has_value())
		return InvalidReadResult(valid);

	if (valid)
		*valid = true;
	return *result;
}

u128 ElfMemoryReader::Read128(u32 address, bool* valid)
{
	std::optional<u128> result = m_elf.get_object_virtual<u128>(address);
	if (!result.has_value())
		return InvalidReadResult128(valid);

	if (valid)
		*valid = true;
	return *result;
}

bool ElfMemoryReader::ReadBytes(u32 address, void* dest, u32 size)
{
	std::optional<std::span<const u8>> bytes = m_elf.get_virtual(address, size);
	if (!bytes.has_value())
		return false;

	std::memcpy(dest, bytes->data(), size);

	return true;
}

bool ElfMemoryReader::Write8(u32 address, u8 value)
{
	return false;
}

bool ElfMemoryReader::Write16(u32 address, u16 value)
{
	return false;
}

bool ElfMemoryReader::Write32(u32 address, u32 value)
{
	return false;
}

bool ElfMemoryReader::Write64(u32 address, u64 value)
{
	return false;
}

bool ElfMemoryReader::Write128(u32 address, u128 value)
{
	return false;
}

bool ElfMemoryReader::WriteBytes(u32 address, void* src, u32 size)
{
	return false;
}

bool ElfMemoryReader::CompareBytes(u32 address, void* src, u32 size)
{
	std::optional<std::span<const u8>> bytes = m_elf.get_virtual(address, size);
	if (!bytes.has_value())
		return false;

	return std::memcmp(src, bytes->data(), size) == 0;
}

// *****************************************************************************

MipsExpressionFunctions::MipsExpressionFunctions(
	DebugInterface* cpu, const ccc::SymbolDatabase* symbolDatabase, bool shouldEnumerateSymbols)
	: m_cpu(cpu)
	, m_database(symbolDatabase)
{
	if (!shouldEnumerateSymbols)
		return;

	if (symbolDatabase)
	{
		enumerateSymbols(*symbolDatabase);
	}
	else
	{
		m_cpu->GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
			enumerateSymbols(database);
		});
	}
}

void MipsExpressionFunctions::enumerateSymbols(const ccc::SymbolDatabase& database)
{
	// TODO: Add mangled symbol name maps to CCC and remove this.

	for (const ccc::Function& function : database.functions)
		m_mangled_function_names_to_handles.emplace(function.mangled_name(), function.handle());

	for (const ccc::GlobalVariable& global : database.global_variables)
		m_mangled_global_names_to_handles.emplace(global.mangled_name(), global.handle());
}

bool MipsExpressionFunctions::parseReference(char* str, u64& referenceIndex)
{
	for (int i = 0; i < 32; i++)
	{
		char reg[8];
		std::snprintf(reg, std::size(reg), "r%d", i);
		if (StringUtil::Strcasecmp(str, reg) == 0 || StringUtil::Strcasecmp(str, m_cpu->getRegisterName(0, i)) == 0)
		{
			referenceIndex = i;
			return true;
		}

		std::snprintf(reg, std::size(reg), "f%d", i);
		if (StringUtil::Strcasecmp(str, reg) == 0)
		{
			referenceIndex = i | REF_INDEX_FPU;
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

bool MipsExpressionFunctions::parseSymbol(char* str, u64& symbolValue)
{
	if (m_database)
		return parseSymbol(str, symbolValue, *m_database);

	bool success = false;
	m_cpu->GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		success = parseSymbol(str, symbolValue, database);
	});
	return success;
}

bool MipsExpressionFunctions::parseSymbol(char* str, u64& symbolValue, const ccc::SymbolDatabase& database)
{
	std::string name = str;

	// Check for mangled function names.
	auto function_iterator = m_mangled_function_names_to_handles.find(name);
	if (function_iterator != m_mangled_function_names_to_handles.end())
	{
		const ccc::Function* function = database.functions.symbol_from_handle(function_iterator->second);
		if (function && function->address().valid())
		{
			symbolValue = function->address().value;
			return true;
		}
	}

	// Check for mangled global variable names.
	auto global_iterator = m_mangled_global_names_to_handles.find(name);
	if (global_iterator != m_mangled_global_names_to_handles.end())
	{
		const ccc::GlobalVariable* global = database.global_variables.symbol_from_handle(global_iterator->second);
		if (global && global->address().valid())
		{
			symbolValue = global->address().value;
			return true;
		}
	}

	// Check for regular unmangled names.
	const ccc::Symbol* symbol = database.symbol_with_name(name);
	if (symbol && symbol->address().valid())
	{
		symbolValue = symbol->address().value;
		return true;
	}

	return false;
}

u64 MipsExpressionFunctions::getReferenceValue(u64 referenceIndex)
{
	if (referenceIndex < 32)
		return m_cpu->getRegister(0, referenceIndex)._u64[0];
	if (referenceIndex == REF_INDEX_PC)
		return m_cpu->getPC();
	if (referenceIndex == REF_INDEX_HI)
		return m_cpu->getHI()._u64[0];
	if (referenceIndex == REF_INDEX_LO)
		return m_cpu->getLO()._u64[0];
	if (referenceIndex & REF_INDEX_IS_OPSL)
	{
		const u32 OP = m_cpu->Read32(m_cpu->getPC());
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
	if (referenceIndex & REF_INDEX_FPU)
	{
		return m_cpu->getRegister(EECAT_FPR, referenceIndex & 0x1F)._u64[0];
	}
	return -1;
}

ExpressionType MipsExpressionFunctions::getReferenceType(u64 referenceIndex)
{
	if (referenceIndex & REF_INDEX_IS_FLOAT)
	{
		return EXPR_TYPE_FLOAT;
	}
	return EXPR_TYPE_UINT;
}

bool MipsExpressionFunctions::getMemoryValue(u32 address, int size, u64& dest, std::string& error)
{
	switch (size)
	{
		case 1:
		case 2:
		case 4:
		case 8:
			break;
		default:
			error = StringUtil::StdStringFromFormat(
				TRANSLATE("ExpressionParser", "Invalid memory access size %d."), size);
			return false;
	}

	if (address % size)
	{
		error = TRANSLATE("ExpressionParser", "Invalid memory access (unaligned).");
		return false;
	}

	switch (size)
	{
		case 1:
			dest = m_cpu->Read8(address);
			break;
		case 2:
			dest = m_cpu->Read16(address);
			break;
		case 4:
			dest = m_cpu->Read32(address);
			break;
		case 8:
			dest = m_cpu->Read64(address);
			break;
	}

	return true;
}
