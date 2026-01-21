#include <Windows.h>
#include <Psapi.h>
#include <cstdio>

#include "Main.h"
#include "Analyse.h"
#include "Processor.h"
#include "Hacking.h"
#include "Windows.h" // Breakpoint window
#include "MainList.h"

HANDLE hAttach;

UINT32 memoryPointer = 0x00000000;
UINT32 memorySize;

Breakpoint breakpoints[10];
UINT32* internalBreakpoints;

#ifdef _DLL
PCSX2SharedData* lxSharedData = NULL;
#endif

// Temporary TODO
void UpdateMemoryRegion(UINT32 address, UINT32 amount);

void OpenPCSX2Process()
{
	// Find PCSX2 handle and open it if possible
	DWORD procids[1000];
	DWORD numprocesses;

	EnumProcesses(procids, 1000 * sizeof (DWORD), &numprocesses);

	numprocesses /= sizeof (DWORD);

	for (int i = 0; i < numprocesses; i ++)
	{
		HANDLE temp = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, 0, procids[i]);
		char name[20];

		GetModuleBaseName(temp, NULL, name, 20);

		if (strstr(name, "pcsx2") != NULL)
		{
			hAttach = temp;
			break;
		}

		CloseHandle(temp);
	}

	// If possible, scan PCSX2 for PS2 memory address
	if (! hAttach)
		return;

	UINT32 lastmsg = 0;
	unsigned char _ratiofunction[28] = 
	{0xB0, 0xFF, 0xBD, 0x27, 0x20, 0x00, 0xB1, 0xFF, 0x10, 0x00, 0xB0, 0xFF, 0x2D, 0x88, 0xA0, 0x00, 
	 0x2D, 0x80, 0x80, 0x00, 0x30, 0x00, 0xB2, 0xFF, 0x40, 0x00, 0xBF, 0xFF};
	unsigned int foundaddresses[10];
	int numfoundaddresses = 0;
	// 0x08A10000 - PCSX2 latest version default

	for (UINT32 address = 0x00000000; address < 0x30000000; )
	{
		MEMORY_BASIC_INFORMATION mbi;

		if (! VirtualQueryEx(hAttach, (void*) address, &mbi, sizeof (mbi)))
		{
			printf("WARNING: VirtualQueryEx error during scan!\n");
			break;
		}

		address = (UINT32) mbi.BaseAddress;

		if (address - lastmsg >= 0x01000000)
		{
			printf("Checking: %08X\n", address);
			lastmsg = address;
		}

		SIZE_T intendedamt = mbi.RegionSize, amt;
		char* check, *maxcheck;
		char* cmpbuffer = (char*) Alloc(intendedamt + sizeof (_ratiofunction));

		memset(cmpbuffer, 0xCA, intendedamt + sizeof (_ratiofunction));
		ReadProcessMemory(hAttach, (void*) address, cmpbuffer, intendedamt, &amt);

		maxcheck = &cmpbuffer[amt];

		for (check = cmpbuffer; check < maxcheck; check ++)
		{
			if (check - cmpbuffer + address == 0x220C24E8)
				check = check;
			if (*check != (char) (0xB0)) continue;

			if (! memcmp(check, _ratiofunction, sizeof (_ratiofunction)))
			{
				UINT32 thisaddress = (check - cmpbuffer) + address;
				UINT32 _b2dps2address = ((*(UINT32*) (&check[28])) & 0x03FFFFFF) << 2;
				UINT32 _ratiops2address = _b2dps2address + 0x300;

				foundaddresses[numfoundaddresses ++] = thisaddress - _ratiops2address;

				printf("Found address: %08X\n", thisaddress - _ratiops2address);
			}
		}

		Free(cmpbuffer);

		if (numfoundaddresses >= 10) break;

		address += mbi.RegionSize;

		if (mbi.RegionSize == 0)
			address += 0x10;
	}

	// Set PCSX2 mem offset to the first found address (for now)
	if (numfoundaddresses <= 0)
	{
		CloseHandle(hAttach);

		hAttach = NULL;
		return;
	}

	memoryPointer = foundaddresses[0];
	memorySize = 0x02000000;
}

void OpenePSXeProcess()
{
	DWORD procids[1000];
	DWORD numprocesses;

	EnumProcesses(procids, 1000 * sizeof (DWORD), &numprocesses);

	numprocesses /= sizeof (DWORD);

	for (int i = 0; i < numprocesses; i ++)
	{
		HANDLE temp = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, 0, procids[i]);
		char name[20];

		if (! temp)
			continue;

		GetModuleBaseName(temp, NULL, name, 20);

		if (strstr(name, "ePSXe") != NULL)
		{
			hAttach = temp;
			break;
		}

		CloseHandle(temp);
	}

	if (! hAttach)
		return;

	memoryPointer = 0x005B6E40;
	memorySize = 0x00200000;
}

void OpenPCSXProcess()
{
	DWORD procids[1000];
	DWORD numprocesses;

	EnumProcesses(procids, 1000 * sizeof (DWORD), &numprocesses);

	numprocesses /= sizeof (DWORD);

	for (int i = 0; i < numprocesses; i ++)
	{
		HANDLE temp = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, 0, procids[i]);
		char name[20] = "";

		if (! temp)
			continue;

		GetModuleBaseName(temp, NULL, name, 20);

		if (strstr(name, "pcsx") != NULL)
		{
			hAttach = temp;
			break;
		}

		CloseHandle(temp);
	}

	if (! hAttach)
		return;

	SIZE_T amt;
	ReadProcessMemory(hAttach, (void*) 0x0073DF68, &memoryPointer, 4, &amt);

//	memoryPointer = 0x02AD0020;
	memorySize = 0x00200000;
}

void GameDataInit()
{
#ifdef _DLL
	memorySize = 0x02000000;
#endif

	// Setup pointers for internal memory copy
	if (! mem || memlen8 != memorySize)
	{
		if (mem)
			Free(mem);

		mem = (unsigned int*) Alloc(memorySize);
		mem8 = (char*) mem; mem16 = (short*) mem;
		memlen8 = memorySize;
		memlen16 = memorySize / 2;
		memlen32 = memorySize / 4;
		
		DWORD test = GetLastError();
		// Update the entire region
		UpdateMemoryRegion(0x00000000, memorySize);
	}

	lines = (line_t*) Alloc((memorySize * sizeof (line_t)) / 4);

	if (mem == NULL || lines == NULL)
		printf("Failed to allocate buffer!\n");

	for (int i = 0; i < memlen32; i ++)
	{
		lines[i].datatype = DATATYPE_CODE;
		lines[i].reference = 0xFFFFFFFF;
		lines[i].referenced = 0;
	}

	// Find information (labels, etc.) while preserving current user-defined labels
	Label* preserveLabels = (Label*) Alloc(numlabels * sizeof (Label));
	int numPreserveLabels = 0;
	for (int i = 0; i < numlabels; i ++)
	{
		if (! labels[i].autoGenerated)
			preserveLabels[numPreserveLabels ++] = labels[i];
	}

	Free(labels);

	labels = preserveLabels;
	numlabels = numPreserveLabels;
	
	FindLabels();
	FindReferences();
	FindFunctions();
}

void GameDataShutdown()
{
	CloseHandle(hAttach);

	hAttach = NULL;
	memoryPointer = 0;
	memorySize = 0;

	memlen32 = memlen16 = memlen8 = 0;

	Free(lines);
}

void UpdateMemoryRegion(UINT32 address, UINT32 amount)
{
#ifndef _DLL
	bool lastProcessOpened = (hAttach != NULL);

	// Attempt to attach to whatever's open
	if (! hAttach)
		OpenPCSX2Process();

	if (! hAttach)
		OpenPCSXProcess();

	if (! hAttach)
		OpenePSXeProcess();

	if (hAttach && ! lastProcessOpened)
		GameDataInit();

	if (hAttach)
	{
		DWORD amt = 0;
		
		ReadProcessMemory(hAttach, (void*) (memoryPointer + address), &mem8[address], amount, &amt);

		if (address >= 0 && address <= memorySize && ! amt) // PCSX2 closed?
			GameDataShutdown();
	}
#else
	static int lastOffset;

	if (address + amount >= memlen8)
		amount = memlen8 - address;

	if (! mem8)
		GameDataInit();

	if (! memoryPointer)
	{
		memset(&mem8[address], 0, amount);
		return;
	}
	else if (lastOffset != memoryPointer)
	{
		// Refresh everything and search for labels
		address = 0;
		amount = 0x02000000;

		GameDataInit();

		lastOffset = memoryPointer;

		printf("Lookin' fer labels...\n");
	}

	memcpy(&mem8[address], (void*) (memoryPointer + address), amount);
#endif
}

void SetMemory(UINT32 address, void* data, int datasize)
{
#ifdef _DLL
	if (lxSharedData->setMemoryFunction)
		lxSharedData->setMemoryFunction(address, data, datasize);

	return;
#else
	if (address + datasize > memorySize)
		return;

	if (! hAttach)
	{
		// Just write memory locally
		memcpy(&mem8[address], data, datasize);
		return;
	}

	DWORD amt;
	UINT32 pcsx2addr = memoryPointer + address;

	MEMORY_BASIC_INFORMATION mbi;

	VirtualQueryEx(hAttach, (void*) pcsx2addr, &mbi, sizeof (mbi));

	if (mbi.Protect & PAGE_READONLY)
	{
		if (! VirtualProtectEx(hAttach, (void*) pcsx2addr, datasize, PAGE_READWRITE, &mbi.Protect))
		{
			printf("Error: could not write to %08X (%08X app address)\n", address, pcsx2addr);
			return;
		}
	}

	memcpy(&mem8[address], data, datasize);

	if (! WriteProcessMemory(hAttach, (void*) (memoryPointer + address), data, datasize, &amt))
		printf("Error writing memory: %i\n", GetLastError());
#endif
}

#define PERSONA_ADDR 0x01FF4000
#define RATCHET_ADDR 0x01FF8000

#define FUNC_ADDR PERSONA_ADDR

UINT32 breakFuncAddress = FUNC_ADDR; // Address of the function that's executed during a breakpoint
UINT32 breakDataAddress = FUNC_ADDR + 0x0100; // Starting address of the values stored at a breakpoint minus 4 (+0 reserved for breakpoint ID)
UINT32 breakUniqueAddress = FUNC_ADDR + 0x0100 + (32 * 0x10); // Starting address of the per-breakpoint branch functions

#define WRITEASM(string) {UINT32 _code; ASMToCode(curAddr, &_code, NULL, string); SetMemory(curAddr, &_code, 4); curAddr += 4;}
#define WRITELI(reg, value) {UINT32 op[2] = {0, 0}; \
							 SET_OPERATION(op[0], LUI); SET_IMMEDIATE(op[0], ((value) >> 16)); SET_RT(op[0], reg); \
							 SET_OPERATION(op[1], ORI); SET_IMMEDIATE(op[1], ((value) & 0xFFFF)); SET_RT(op[1], reg); SET_RS(op[1], reg); \
							 SetMemory(curAddr, op, 8); curAddr += 8;}
#define WRITEJ(address) {UINT32 _val = 0; SET_OPERATION(_val, J); SET_ADDRESS(_val, address); SetMemory(curAddr, &_val, 4); curAddr += 4;}

#define STOREWIDTH 4

void UpdateBreakWindow(int breakId);

void ToggleBreakpoint(UINT32 address)
{
	// Check if a breakpoint is there to be removed
	for (int i = 0; i < 10; i ++)
	{
#ifndef NEW_BREAKPOINTS
		if ((breakpoints[i].address == address || breakpoints[i].address == address + 4) && breakpoints[i].enabled)
#else
		if (breakpoints[i].address == address && breakpoints[i].enabled)
#endif
		{
			RemoveBreakpoint(address); // Remove breakpoint
			return;
		}
	}

	// Otherwise set it
	SetBreakpoint(address);
}

void SetBreakpoint(UINT32 address)
{
	for (int i = 0; i < 10; i ++)
	{
#ifndef NEW_BREAKPOINTS
		if ((breakpoints[i].address == address || breakpoints[i].address == address + 4) && breakpoints[i].enabled)
#else
		if (breakpoints[i].address == address && breakpoints[i].enabled)
#endif
			return; // Breakpoint already exists
	}

	// Look for a Free breakpoint
	// In addition, make sure to avoid replacing the one that's currently broken (if applicable!)
	UpdateMemoryRegion(breakDataAddress, 4);

	int breakId;
	for (breakId = 0; breakId < 10; breakId ++)
	{
		if (! breakpoints[breakId].enabled && breakId != mem8[breakDataAddress] - 1)
			break;
	}

	if (breakId == 10)
	{
		MessageBox(NULL, "We've reached breaking point!", "Sorry: Too many breakpoints! Try clearing them?", MB_OK);
		return; // Too many breakpoints!
	}

	// TODO: Ensure above line isn't a beq
	// TODO: Proper stack traversal when breaking on a jr ra (after ra is restored)

	// Set up the breakpoint
	Breakpoint* bp = &breakpoints[breakId];

	// Preserve the original code
	UpdateMemoryRegion(address, 0x10); // 4 lines

	memcpy(bp->originalValues, &mem8[address], 4 * 4);

	// Set parameters
	bp->address = address;
	bp->toAddress = breakUniqueAddress + breakId * 0x30;

	// Enable breakpoint
	bp->enabled = 1;
}

void RemoveBreakpoint(UINT32 address)
{
	Breakpoint* bp = NULL;

	for (int i = 0; i < 10; i ++)
	{
		if (breakpoints[i].address == address && breakpoints[i].enabled)
			bp = &breakpoints[i];
	}

	if (! bp)
		return;

#ifndef NEW_BREAKPOINTS
	// TODO: Check whether it's already active
	SetMemory(bp->address, bp->originalValues, 8);
#endif
	bp->enabled = 0;
}

void ClearBreakpoints()
{
	for (int i = 0; i < 10; i ++)
		RemoveBreakpoint(breakpoints[i].address);
}

void WriteBreakpoint(int breakId)
{
	// Create the specific breakpoint function
	UINT32 curAddr = breakpoints[breakId].toAddress;

	WRITEASM("addiu sp,sp,$FFE0");
	WRITEASM("sq v0,$0000(sp)");
	WRITEASM("sq v1,$0010(sp)");

	WRITELI(V0, breakId + 1);
	WRITELI(V1, breakDataAddress);
	WRITEASM("sw v0,$0000(v1)");

	WRITEASM("lq v0,$0000(sp)");
	WRITEASM("lq v1,$0010(sp)");
	WRITEJ(breakFuncAddress);
	WRITEASM("addiu sp,sp,$0020");

	// Write breakpoint jump @ breakpoint.address
	UINT32 breakpointValues[2] = {0x00000000, 0x00000000};

	SET_OPERATION(breakpointValues[0], J);
	SET_ADDRESS(breakpointValues[0], breakpoints[breakId].toAddress);

	SetMemory(breakpoints[breakId].address, breakpointValues, 8);

	UpdateReference(breakpoints[breakId].address);
}

void HandleBreakpoints()
{
#ifdef NEW_BREAKPOINTS
	/* Handle DATA BREAKPOINTS! Yay! */
#ifdef _DLL
	if (! memoryPointer)
		return;

	// Write PC breakpoints
	for (int i = 0; i < 10; i ++)
	{
		if (breakpoints[i].enabled)
			lxSharedData->internalBreakpoints[i + BREAKPOINT_PC0] = breakpoints[i].address;
		else
			lxSharedData->internalBreakpoints[i + BREAKPOINT_PC0] = 0x00000000;
	}

	// Write data breakpoints
	if (lxSharedData->internalBreakpoints != NULL)
	{
		UINT readBrk = GetBreakWindowReadBreakpoint();
		UINT writeBrk = GetBreakWindowWriteBreakpoint();

		lxSharedData->internalBreakpoints[BREAKPOINT_READ] = (UINT32) memoryPointer + (readBrk & 0x03FFFFFF);
		lxSharedData->internalBreakpoints[BREAKPOINT_WRITE] = (UINT32) memoryPointer + (writeBrk & 0x03FFFFFF);
	}
	
	// Check if a breakpoint has been reached
	if (lxSharedData->breakStatus == BREAK_PAUSED)
	{
		if (! wndBreak.isUpdated)
		{
			UpdateBreakWindow(0);

			if (GetAddressSel(lxSharedData->breakPc) == -1)
				Goto(lxSharedData->breakPc);
			else
			{
				int lastSel = list.sel;

				list.address = lxSharedData->breakPc;

				ScrollUp(lastSel);
			}

			// Re-analyse current function area with awareness of the breakpoint's registers
			UINT32 curFunction = FindFunctionStart(GetSelAddress(-1)), curFunctionEnd = FindFunctionEnd(GetSelAddress(-1));

			if (curFunctionEnd - curFunction >= 0x8000) //Hack: impose function length limit
			{
				curFunction = GetSelAddress(-1);
				curFunctionEnd = curFunction + 0x1000;
			}

			ClearAutoGeneratedComments();
			AnalyseRegion(curFunction, curFunctionEnd, 1);

			// Update the list and window
			UpdateList();
			wndBreak.isUpdated = true;
		}

		if (wndBreak.continuePressed || wndBreak.stepPressed || wndBreak.autoContinue)
		{
			if (wndBreak.continuePressed || wndBreak.autoContinue)
			{
				SetBreakWindowStatus("Running");
				lxSharedData->breakStatus = BREAK_REQUESTCONTINUE;
			}
			else if (wndBreak.stepPressed)
			{
				SetBreakWindowStatus("Stepping");
				lxSharedData->breakStatus = BREAK_REQUESTSTEP;
			}
			
			wndBreak.isUpdated = false;
			
			// Update the registers
			int strLen = SendMessage(wndBreak.edRegs1, WM_GETTEXTLENGTH, 0, 0);
			char* regsString = (char*) Alloc(strLen + 1);
			char* curChar = regsString;

			SendMessage(wndBreak.edRegs1, WM_GETTEXT, SendMessage(wndBreak.edRegs1, WM_GETTEXTLENGTH, 0, 0) + 1, (LPARAM) regsString);

			int line;
			UINT32 regValues[32];
			for (line = 0; line < 32; line ++)
			{
				char* startChar = curChar;

				while (*curChar != 0x0D && *curChar != 0x0A && *curChar != '\0') curChar ++;

				char lastChar = *curChar;

				*curChar = '\0';

				UINT32 regValue;
				if (sscanf(startChar, "%08X", &regValue) == 1)
					regValues[line] = regValue;

				if (lastChar == 0x0D)
					curChar += 2;
				else if (lastChar == 0x0A)
					curChar ++;
				else
					break;
			}

			if (line == 32)
				memcpy(lxSharedData->breakRegs, regValues, sizeof (lxSharedData->breakRegs));

			Free(regsString);
		}
	}
	
	char breakStatus[30] = "";

	switch (lxSharedData->breakStatus)
	{
		case BREAK_PAUSED:
			sprintf(breakStatus, "BREAK AT %08X", lxSharedData->breakPc);
			break;
		case BREAK_RUNNING:
			strcpy(breakStatus, "Running");
			break;
		case BREAK_REQUESTCONTINUE:
		case BREAK_REQUESTSTEP:
		case BREAK_GRANTINGREQUEST:
			sprintf(breakStatus, "Responding... (%08X)", lxSharedData->breakPc);
			break;
	}

	SetBreakWindowStatus(breakStatus);

	wndBreak.continuePressed = false;
	wndBreak.stepPressed = false;
#endif
	return;
#else // NEW_BREAKPOINTS
	if (memlen8 < breakDataAddress || memlen8 < breakFuncAddress)
		return; // Hack for now

	// Set the breakpoint function
	UINT32 curAddr = breakFuncAddress;

	// Write beginning of the break function: change sp and preserve v1
	WRITEASM("addiu sp,sp,$FFF0");
	WRITEASM("sq v1,$0000(sp)");

	// Write V1: The breakDataAddress + 4 (0000 reserved for breakpoint ID)
	WRITELI(V1, breakDataAddress + 4 * STOREWIDTH);

	// Write reg-writing code
	for (int i = 1; i < 32; i ++)
	{
		UINT32 code = 0;

		if (i == V1) // Skip; do later
			continue;

		SET_OPERATION(code, SQ);
		SET_RS(code, V1);
		SET_RT(code, i);
		SET_IMMEDIATE(code, i * 4 * STOREWIDTH);

		SetMemory(curAddr, &code, 4);
		curAddr += 4;
	}

	// Write last register: v1
	WRITEASM("lq v0,$0000(sp)");
	WRITEASM("sq v0,$0030(v1)");

	// Write waiting code
	// (Loop begin)
	WRITEASM("lw v0,$FFF0(v1)");

	UINT32 beqVal = 0x1440FFFE;
	SetMemory(curAddr, &beqVal, 4); // bne v1,zero,-$0004 (i.e. 1 line above)
	curAddr += 4;

	WRITEASM("nop");

	// Prepare the return!
	// Check active breakpoint (if there is one) -------------------------
	UpdateMemoryRegion(breakDataAddress, 4);

	if (mem[breakDataAddress / 4] != 0 && mem[breakDataAddress / 4] - 1 < 10)
	{
		Breakpoint* bp =&breakpoints[mem[breakDataAddress / 4] - 1];

		// Replace the code that was used to jump here with its original value
		WRITELI(A0, bp->address);
		WRITELI(V0, bp->originalValues[0]);
		WRITEASM("sw v0,$0000(a0)");

		WRITELI(V0, bp->originalValues[1]);
		WRITEASM("sw v0,$0004(a0)");

		// Restore v0 and a0
		WRITEASM("lq v0,$0020(v1)");
		WRITEASM("lq a0,$0040(v1)");

		// Write restore stuff (restore V1)
		WRITEASM("lq v1,$0000(sp)");
		WRITEASM("addiu sp,sp,$0010");

		// Jump back!
		WRITEJ(bp->address);
		WRITEASM("nop");

		// Show breakpoint window
		if (! wndBreak.isUpdated)
		{
			char statusString[52];

			sprintf(statusString, "BREAK %i", mem8[breakDataAddress], bp->address);

			SetBreakWindowStatus(statusString);
			UpdateBreakWindow(mem8[breakDataAddress] - 1);

			wndBreak.isUpdated = true;
		}

		// When allowed, continue the game
		if (wndBreak.autoContinue || wndBreak.continuePressed)
		{
			UINT32 zero = 0;

			SetMemory(breakDataAddress, &zero, 4);
			SetBreakWindowStatus("Running");

			wndBreak.isUpdated = false; // Break window may now be out of date!
		}
	}

	// Write actual breakpoints! ---------------------------------
	for (int i = 0; i < 10; i ++)
	{
		Breakpoint* bp = &breakpoints[i];

		if (! bp->enabled)
			continue;

		WriteBreakpoint(i);
	}

	wndBreak.continuePressed = 0;
#endif
}

void UpdateBreakWindow(int breakId)
{
	UpdateMemoryRegion(0x00000000, memlen8);

	ShowBreakWindow(true);
#ifdef _DLL
	char regText[4096] = "";
	char tmpText[12];

	for (int i = 0; i < 32; i ++)
	{
#ifdef NEW_BREAKPOINTS
		sprintf(tmpText, "%08X\x0D\x0A", lxSharedData->breakRegs[i]);
#else
		sprintf(tmpText, "%08X\x0D\x0A", mem[breakDataAddress / 4 + STOREWIDTH + i * STOREWIDTH]);
#endif
		strcat(regText, tmpText);
	}

	SetBreakWindowText(regText);

	// Add the first address in the call stack: the breakpoint itself
	char breakAddr[16];
#ifdef NEW_BREAKPOINTS
	sprintf(breakAddr, "--> %08X", lxSharedData->breakPc);
#else
	sprintf(breakAddr, "--> %08X", breakpoints[breakId].address);
#endif

	ClearBreakWindowCalls();
	AddBreakWindowCall(breakAddr);
	
	// Step through the call stack
#ifdef NEW_BREAKPOINTS
	UINT32 curAddr = lxSharedData->breakPc;
	UINT32 curSp = lxSharedData->breakRegs[29];
#else
	UINT32 curAddr = breakpoints[breakId].address;
	UINT32 curSp = mem[(breakDataAddress / 4) + STOREWIDTH + 29 * STOREWIDTH] + 0x0010; // sp; adding 0x10 due to breakpoint's offset
#endif

	if (curSp >= 0x70000000)
		curSp -= 0x6E00F4C0; // Hack/Twinsanity fix! (note: doesn't really work all the time, guess the scratchpad is a real thing)

	bool firstRun = 1;
	while (curAddr < memlen8 && curAddr > 0x00000000)
	{
		UINT32 addr = curAddr;
		int spOffset = -1;
		int raOffset = -1;

		// Try and reach the end of the function, and confirm there's an sp offset.
		while (addr + 4 < memlen8)
		{
			UINT32 data1 = mem[addr / 4], data2 = mem[addr / 4 + 1];

			if (GET_OPERATION(data2) == ADDIU && GET_RT(data2) == 29 && GET_RS(data2) == 29)
			{
				if (spOffset != -1)
					break; // Not good - we've already got one and would have broken at jr ra

				spOffset = GET_IMMEDIATE(data2);
			}

			if ((GET_OPERATION(data1) == LW || GET_OPERATION(data1) == LQ || GET_OPERATION(data1) == LD) && GET_RT(data1) == 31 && GET_RS(data1) == 29)
				raOffset = GET_IMMEDIATE(data1);

			if (data1 == 0x03E00008) // jr ra
				break;

			addr += 4;
		}

		if (spOffset == -1 || raOffset == -1)
		{
			if (! firstRun)
				break; // We're done here.
			else
			{
#ifndef NEW_BREAKPOINTS
				curAddr = mem[(breakDataAddress / 4) + STOREWIDTH + 31 * STOREWIDTH]; // Try starting again, from ra's value instead?
#else
				curAddr = lxSharedData->breakRegs[29]; // Try starting again, from ra's value instead?
#endif
				firstRun = 0;
				continue;
			}
		}

		if (curSp + raOffset + 3 > memlen8)
			break;

		curAddr = mem[(curSp + raOffset) / 4];
		curSp += spOffset;

		char addrr[10];
		sprintf(addrr, "%08X", curAddr - 0x08);
		AddBreakWindowCall(addrr);
		firstRun = 0;
	}
#endif
}