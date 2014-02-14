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
	return memRead8(address);
}

u32 DebugInterface::read16(u32 address)
{
	return memRead16(address);
}

u32 DebugInterface::read32(u32 address)
{
	return memRead32(address);
}

u128 DebugInterface::getGPR(int num)
{
	return cpuRegs.GPR.r[num].UQ;
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
	R5900::disR5900F(out,memRead32(address));
	return out;
}