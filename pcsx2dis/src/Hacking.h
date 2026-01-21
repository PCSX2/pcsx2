#pragma once

#ifndef _BASETSD_H_
#include <BaseTsd.h>
#endif

#define NEW_BREAKPOINTS

enum BreakId
{
	BREAKPOINT_READ = 0,
	BREAKPOINT_WRITE = 1,
	BREAKPOINT_PC0 = 2,
	BREAKPOINT_PC1 = 3,
	BREAKPOINT_PC2 = 4,
	BREAKPOINT_PC3 = 5,
	BREAKPOINT_PC4 = 6,
	BREAKPOINT_PC5 = 7,
	BREAKPOINT_PC6 = 8,
	BREAKPOINT_PC7 = 9,
	BREAKPOINT_PC8 = 10,
	BREAKPOINT_PC9 = 11,
	BREAKPOINT_MAX = 12,
	// etc?
};

enum BreakStatus
{
	BREAK_RUNNING = 0,
	BREAK_PAUSED = 1,
	BREAK_REQUESTCONTINUE = 2,
	BREAK_REQUESTSTEP = 3,
	BREAK_GRANTINGREQUEST = 4,
};

#pragma pack(push, 1)
struct GameSharkCode
{
	UINT32 address;
	UINT32 value;
};

struct PCSX2SharedData
{
	void* ps2Memory;
	void (_cdecl *setMemoryFunction)(UINT32 ps2Address, void* data, UINT32 dataLength);

	UINT32* internalBreakpoints;
	UINT8 numInternalBreakpoints;

	INT8 breakStatus;
	UINT32 breakRegs[32];
	UINT32 breakPc;

	GameSharkCode* gameSharkCodes;
	INT32 numGameSharkCodes;
};
#pragma pack(pop)

struct Breakpoint
{
	bool enabled;

	UINT32 address; // Address of the breakpoint
	UINT32 toAddress; // Address that the breakpoint jumps to

	UINT32 originalValues[4]; // Values to restore to this address when disabling/removing breakpoint. Includes delayed instruction
};

void UpdateMemoryRegion(UINT32 address, UINT32 amount);
void SetMemory(UINT32 address, void* data, int datasize);
void GameDataShutdown();

void ToggleBreakpoint(UINT32 address);
void SetBreakpoint(UINT32 address);
void RemoveBreakpoint(UINT32 address);
void ClearBreakpoints();
void HandleBreakpoints();

extern UINT32 memoryPointer;
extern UINT32 memorySize;

#ifdef _DLL
extern PCSX2SharedData* lxSharedData;
#endif

extern Breakpoint breakpoints[10];