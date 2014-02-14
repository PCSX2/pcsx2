#pragma once
#include "MemoryTypes.h"

class DebugInterface
{
public:
	u32 read8(u32 address);
	u32 read16(u32 address);
	u32 read32(u32 address);
	u128 getGPR(int num);
	u128 getHI();
	u128 getLO();
	u32 getPC();

	bool isCpuPaused();
	void pauseCpu();
	void resumeCpu();
	std::string disasm(u32 address);
};

extern DebugInterface debug;