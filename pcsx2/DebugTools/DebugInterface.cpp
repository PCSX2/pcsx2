#include "PrecompiledHeader.h"

#include "DebugInterface.h"
#include "Memory.h"
#include "r5900.h"
#include "AppCoreThread.h"
#include "Debug.h"

extern AppCoreThread CoreThread;

DebugInterface debug;

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
	return 1;
}

const char* DebugInterface::getRegisterCategoryName(int cat)
{
	switch (cat)
	{
	case 0:
		return "GPR";
	default:
		return "Invalid";
	}
}

int DebugInterface::getRegisterSize(int cat)
{
	switch (cat)
	{
	case 0:
		return 128;
	default:
		return 0;
	}
}

int DebugInterface::getRegisterCount(int cat)
{
	switch (cat)
	{
	case 0:
		return 35;	// 32 + pc + hi + lo
	default:
		return 0;
	}
}

DebugInterface::RegisterType DebugInterface::getRegisterType(int cat)
{
	switch (cat)
	{
	case 0:
	default:
		return NORMAL;
	}
}

const char* DebugInterface::getRegisterName(int cat, int num)
{
	switch (cat)
	{
	case 0:
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
	default:
		return "Invalid";
	}
}

u128 DebugInterface::getRegister(int cat, int num)
{
	u128 result;
	switch (cat)
	{
	case 0:
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
	case 0:
		return getRegister(cat,num).ToString();
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

bool DebugInterface::isRunning()
{
	return GetCoreThread().IsRunning();
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