#pragma once
#include "MemoryTypes.h"

class DebugInterface
{
public:
	enum RegisterType { NORMAL, SPECIAL };

	u32 read8(u32 address);
	u32 read16(u32 address);
	u32 read32(u32 address);
	u64 read64(u32 address);
	u128 read128(u32 address);
	void write8(u32 address, u8 value);

	// register stuff
	int getRegisterCategoryCount();
	const char* getRegisterCategoryName(int cat);
	int getRegisterSize(int cat);
	int getRegisterCount(int cat);
	RegisterType getRegisterType(int cat);
	const char* getRegisterName(int cat, int num);
	u128 getRegister(int cat, int num);
	wxString getRegisterString(int cat, int num);
	u128 getHI();
	u128 getLO();
	u32 getPC();

	bool isAlive();
	bool isCpuPaused();
	void pauseCpu();
	void resumeCpu();
	std::string disasm(u32 address);
};

extern DebugInterface debug;