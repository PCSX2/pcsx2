#include "PrecompiledHeader.h"

#include "DebugInterface.h"
#include "Memory.h"
#include "r5900.h"
#include "AppCoreThread.h"
#include "Debug.h"
#include "../VU.h"

extern AppCoreThread CoreThread;

DebugInterface debug;

enum { CAT_GPR, CAT_CP0, CAT_CP1, CAT_CP2F, CAT_CP2I, CAT_COUNT };

u32 DebugInterface::read8(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead8(address);
}

u32 DebugInterface::read16(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead16(address);
}

u32 DebugInterface::read32(u32 address)
{
	if (!isValidAddress(address))
		return -1;
	return memRead32(address);
}



int DebugInterface::getRegisterCategoryCount()
{
	return CAT_COUNT;
}

const char* DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
	case CAT_GPR:
		return "GPR";
	case CAT_CP0:
		return "CP0";
	case CAT_CP1:
		return "CP1";
	case CAT_CP2F:
		return "CP2f";
	case CAT_CP2I:
		return "CP2i";
	default:
		return "Invalid";
	}
}

int DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
	case CAT_GPR:
	case CAT_CP2F:
		return 128;
	case CAT_CP0:
	case CAT_CP1:
	case CAT_CP2I:
		return 32;
	default:
		return 0;
	}
}

int DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
	case CAT_GPR:
		return 35;	// 32 + pc + hi + lo
	case CAT_CP0:
	case CAT_CP1:
	case CAT_CP2F:
	case CAT_CP2I:
		return 32;
	default:
		return 0;
	}
}

DebugInterface::RegisterType DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
	case CAT_GPR:
	case CAT_CP0:
	case CAT_CP2F:
	case CAT_CP2I:
	default:
		return NORMAL;
	case CAT_CP1:
		return SPECIAL;
	}
}

const char* DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
	case CAT_GPR:
		switch (num)
		{
		case 32:	// pc
			return "pc";
		case 33:	// hi
			return "hi";
		case 34:	// lo
			return "lo";
		default:
			return R5900::disRNameGPR[num];
		}
	case CAT_CP0:
		return R5900::disRNameCP0[num];
	case CAT_CP1:
		return R5900::disRNameCP1[num];
	case CAT_CP2F:
		return disRNameCP2f[num];
	case CAT_CP2I:
		return disRNameCP2i[num];
	default:
		return "Invalid";
	}
}

u128 DebugInterface::getRegister(int cat, int num)
{
	u128 result;
	switch (cat)
	{
	case CAT_GPR:
		switch (num)
		{
		case 32:	// pc
			result = u128::From32(cpuRegs.pc);
			break;
		case 33:	// hi
			result = cpuRegs.HI.UQ;
			break;
		case 34:	// lo
			result = cpuRegs.LO.UQ;
			break;
		default:
			result = cpuRegs.GPR.r[num].UQ;
			break;
		}
		break;
	case CAT_CP0:
		result = u128::From32(cpuRegs.CP0.r[num]);
		break;
	case CAT_CP1:
		result = u128::From32(fpuRegs.fpr[num].UL);
		break;
	case CAT_CP2F:
		result = VU1.VF[num].UQ;
		break;
	case CAT_CP2I:
		result = u128::From32(VU1.VI[num].UL);
		break;
	default:
		result.From32(0);
		break;
	}

	return result;
}

wxString DebugInterface::getRegisterString(int cat, int num)
{
	switch (cat)
	{
	case CAT_GPR:
	case CAT_CP0:
		return getRegister(cat,num).ToString();
	case CAT_CP1:
		{
			char str[64];
			sprintf(str,"%f",fpuRegs.fpr[num].f);
			return wxString(str,wxConvUTF8);
		}
	default:
		return L"";
	}
}


u128 DebugInterface::getHI()
{
	return cpuRegs.HI.UQ;
}

u128 DebugInterface::getLO()
{
	return cpuRegs.LO.UQ;
}

u32 DebugInterface::getPC()
{
	return cpuRegs.pc;
}

bool DebugInterface::isAlive()
{
	return GetCoreThread().IsOpen();
}

bool DebugInterface::isCpuPaused()
{
	return GetCoreThread().IsPaused();
}

void DebugInterface::pauseCpu()
{
	SysCoreThread& core = GetCoreThread();
	if (!core.IsPaused())
		core.Pause();
}

void DebugInterface::resumeCpu()
{
	SysCoreThread& core = GetCoreThread();
	if (core.IsPaused())
		core.Resume();
}

std::string DebugInterface::disasm(u32 address)
{
	std::string out;

	u32 op = read32(address);
	R5900::disR5900Fasm(out,op,address);
	return out;
}