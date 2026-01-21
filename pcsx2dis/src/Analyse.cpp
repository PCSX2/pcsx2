#include "Main.h"
#include "Processor.h"
#include "Analyse.h"
#include "Hacking.h" // FindPattern
#include "Windows.h" // FindPattern
#include "MainList.h" // FindPattern
#include "Dialogs.h" // Progress dialog

#include <cstring>
#include <cstdio>

reg_t regs[32];
reg_t fRegs[32];

UINT32 gpAddr = 0x00000000;
bool gpLock = 1;

inline reg_t* GetWriteReg(UINT32 code)
{
	int id = GET_OPERATION(code);
	const char* def = asmdef[id];
	
	char* plus = strstr(asmdef[id], "+w");

	if (plus == NULL)
		return NULL;

	while (plus[0] != ' ' && plus[0] != ',' && plus > def) plus --;
	plus ++;

	if (! strncmp(plus, "rd", 2))
		return &regs[GET_RD(code)];
	else if (! strncmp(plus, "rs", 2))
		return &regs[GET_RS(code)];
	else if (! strncmp(plus, "rt", 2))
		return &regs[GET_RT(code)];
	else
		return NULL;
}

void AppendValue(reg_t* r, const char* value)
{
	if (r == &regs[GP] && gpLock) return;

	if (r->value.known || ! strcmp(r->value.str, r->name))
	{
		r->value.known = 0;
		r->value.str[0] = '\0';
	}

	if (strlen(value) + strlen(r->value.str) + 1 >= 256)
		return;

	strcat(r->value.str, value);
}

void SetExactValue(reg_t* r, UINT32 value)
{
	if (r == &regs[GP] && gpLock) return;

	r->value.v = value;
	r->value.known = 1;
	sprintf(r->value.str, "%08X", value);
}

void SetUnknownValue(reg_t* r, const char* value)
{
	if (r == &regs[GP] && gpLock) return;

	if (strlen(r->value.str) + strlen(value) >= 256)
	{
		r->value.known = 0;
		strcpy(r->value.str, r->name);
		return;
	}

	r->value.known = 0;
	strcpy(r->value.str, value);
}

void SetValue(value_t* v, UINT32 value)
{
	v->v = value;
	v->known = 1;
	sprintf(v->str, "%08X", value);
}

void SetValueVariableLen(value_t* v, UINT32 value)
{
	v->v = value;
	v->known = 1;
	sprintf(v->str, "%X", value);
}

enum optype_t
{
	OPTYPE_NONE, 
	OPTYPE_ADD, 
	OPTYPE_LOAD, 
	OPTYPE_STORE, 
	OPTYPE_OR, 
	OPTYPE_AND, 
	OPTYPE_XOR, 
	OPTYPE_LSHIFT
};

reg_t zeroreg = {"zero", "zero", {0x00000000, "zero", 1}};

void ClearRegisters()
{
	regs[0] = zeroreg;

	for (int i = 1; i < 32; i ++)
	{
		strcpy(regs[i].name, registers[i]);
		strcpy(fRegs[i].name, f_registers[i]);

		SetUnknownValue(&regs[i], regs[i].name);
		SetUnknownValue(&fRegs[i], fRegs[i].name);
	}

	SetExactValue(&regs[GP], gpAddr);
}

void EvalInstruction(UINT32 code, UINT32 address, bool setcomments)
{
	int op = GET_OPERATION(code);
	reg_t* r, *r2, *r3;
	value_t src1, src2;
	optype_t optype = OPTYPE_NONE;
	bool u = 0, f = 0;

	if (! asmdef[op]) return;

	switch (op)
	{
		// OPTYPE_ADD -----------
		case DADDIU:
		case ADDIU: u = 1;
		case DADDI:
		case ADDI:
			r = &regs[GET_RT(code)];
			src1 = regs[GET_RS(code)].value;
			SetValue(&src2, (signed short) GET_IMMEDIATE(code));
			optype = OPTYPE_ADD;
			break;
		case DADDU:
		case ADDU: u = 1;
		case DADD:
		case ADD:
			r = &regs[GET_RD(code)];
			src1 = regs[GET_RS(code)].value;
			src2 = regs[GET_RT(code)].value;
			optype = OPTYPE_ADD;
			break;
		case LUI:
			r = &regs[GET_RT(code)];
			SetValue(&src1, GET_IMMEDIATE(code) << 16);
			SetValue(&src2, 0);
			optype = OPTYPE_ADD;
			break;
		// OPTYPE_LOAD
		case LWC1:
		case LWU:
		case LHU:
		case LBU: u = 1;
		case LW:
		case LH:
		case LB:
			r = &regs[GET_RT(code)];
			src1 = regs[GET_RS(code)].value;
			optype = OPTYPE_LOAD;
			break;
		// OPTYPE_STORE
		case SWC1:
		case SW:
		case SH:
		case SB:
			r = &regs[GET_RT(code)];
			src1 = regs[GET_RS(code)].value;
			optype = OPTYPE_STORE;
			break;
		// OPTYPE_OR
		case OR:
			r = &regs[GET_RD(code)];
			src1 = regs[GET_RS(code)].value;
			src2 = regs[GET_RT(code)].value;
			optype = OPTYPE_OR;
			break;
		case ORI:
			r = &regs[GET_RT(code)];
			src1 = regs[GET_RS(code)].value;
			SetValue(&src2, GET_IMMEDIATE(code));
			optype = OPTYPE_OR;
			break;
		// OPTYPE_LSHIFT
		case SLL:
		case DSLL:
			if (! code)
				return; // nop

			r = &regs[GET_RD(code)];
			src1 = regs[GET_RT(code)].value;
			SetValueVariableLen(&src2, GET_SHIFT(code));
			optype = OPTYPE_LSHIFT;
			break;
		case JR:
			if (GET_RS(code) == 31)
				ClearRegisters(); // jr ra!
			optype = OPTYPE_NONE;
			break;
		default:
			return;
	}

	char tempstr[512];
	if (optype == OPTYPE_ADD)
	{
		if (src1.known && src2.known)
			SetExactValue(r, src1.v + src2.v);
		else if (setcomments)
		{
			if (src1.known && ! strcmp(src1.str, "zero"))
				sprintf(tempstr, "%s", src2.str);
			else if (src2.known && ! strcmp(src2.str, "zero"))
				sprintf(tempstr, "%s", src1.str);
			else
			{
				if (! src2.known)
					sprintf(tempstr, "%s+%s", src1.str, src2.str);
				else
				{
					if (src2.v >= 0x80000000)
						sprintf(tempstr, "%s-%X", src1.str, (UINT32) (0xFFFFFFFF - src2.v + 1));
					else
						sprintf(tempstr, "%s+%X", src2.str, src2.v);
				}
			}

			SetUnknownValue(r, tempstr);
		}
	}
	else if (optype == OPTYPE_OR)
	{
		if (src1.known && src2.known)
			SetExactValue(r, src1.v | src2.v);
		else if (setcomments)
		{
			if (src1.known && ! strcmp(src1.str, "zero"))
				sprintf(tempstr, "%s", src2.str);
			else if (src2.known && ! strcmp(src2.str, "zero"))
				sprintf(tempstr, "%s", src1.str);
			else
				sprintf(tempstr, "%s|%s", src1.str, src2.str);

			SetUnknownValue(r, tempstr);
		}
	}
	else if (optype == OPTYPE_LSHIFT)
	{
		if (src1.known && src2.known)
			SetExactValue(r, src1.v << src2.v);
		else if (setcomments)
		{
			char tempstr[512];

			sprintf(tempstr, "%s<<%s", src1.str, src2.str);

			SetUnknownValue(r, tempstr);
		}
	}
	else if (optype == OPTYPE_LOAD)
	{
		if (src1.known)
		{
			UINT32 addr = src1.v + ((signed short) GET_IMMEDIATE(code));

			if (addr < memlen8)
			{
				int value = 0x00000000;

				if (op == LWU || op == LW) value = ((int*) mem)[addr / 4];
				else if (op == LH)  value = mem16[addr / 2];
				else if (op == LHU) value = mem16[addr / 2] & 0xFFFF;
				else if (op == LB)  value = mem8[addr];
				else if (op == LBU) value = mem8[addr] & 0xFF;

				SetExactValue(r, (UINT32) value);
				
				SetReference(addr, address);
			}
			else if (setcomments)
				SetUnknownValue(r, r->name);
		}
		else if (setcomments)
			SetUnknownValue(r, r->name);
	}
	else if (optype == OPTYPE_STORE)
	{
		if (src1.known)
		{
			UINT32 addr = src1.v + ((signed short) GET_IMMEDIATE(code));

			SetReference(addr, address);
		}
	}
	else if (optype == OPTYPE_NONE)
		return;

	for (int i = 1; i < 32; i ++)
	{
		if (strstr(regs[i].value.str, r->name) && r != &regs[i])
			SetUnknownValue(&regs[i], regs[i].name);
	}

	// Set comment
	if (setcomments && (optype == OPTYPE_ADD || optype == OPTYPE_OR || optype == OPTYPE_LSHIFT))
	{
		char drAWRGH[512];
		int labelid;

		sprintf(drAWRGH, "%s = %s", r->name, r->value.str);

		if (r->value.known) // Try using label name instead of number?
		{
			for (labelid = 0; labelid < numlabels; labelid ++)
			{
				if (labels[labelid].address == r->value.v)
					break;
			}

			if (labelid < numlabels)
				sprintf(drAWRGH, "%s = %s", r->name, labels[labelid].string);
		}

		SetComment(address, drAWRGH, 1);
	}
	else if (optype == OPTYPE_LOAD && setcomments)
	{
		char comment[512];

		if (src1.known)
		{
			int labelid;
			for (labelid = 0; labelid < numlabels; labelid ++)
			{
				if (labels[labelid].address == src1.v + ((signed short) GET_IMMEDIATE(code)))
					break;
			}

			if (labelid == numlabels)
				sprintf(comment, "%s = *%08X", r->name, src1.v + ((signed short) GET_IMMEDIATE(code)));
			else
				sprintf(comment, "%s = %s", r->name, labels[labelid].string);
		}
		else
			sprintf(comment, "%s = %s", r->name, r->value.str); // unknown

		SetComment(address, comment, 1);
	}
	else if (optype == OPTYPE_STORE && setcomments)
	{
		char comment[512];

		if (src1.known)
		{
			UINT32 storeAddr = src1.v + ((signed short) GET_IMMEDIATE(code));
			int labelid;
			for (labelid = 0; labelid < numlabels; labelid ++)
			{
				if (labels[labelid].address == storeAddr)
					break;
			}

			if (labelid == numlabels)
				sprintf(comment, "*%08X = %s", storeAddr, r->name);
			else
				sprintf(comment, "%s = %s", labels[labelid].string, r->name);
		}
		else
			sprintf(comment, "%s = %s", r->name, r->value.str); // unknown

		SetComment(address, comment, 1);
	}

	if (r->value.known && r->value.v > 0 && r->value.v < memlen8 && optype != OPTYPE_LOAD && optype != OPTYPE_STORE)
	{
		lines[address / 4].reference = r->value.v;
		lines[r->value.v / 4].referenced |= 1 << (r->value.v & 3);
	}
}

void AnalyseRegion(UINT32 start, UINT32 end, bool setComments)
{
	RegOverride* involvedRegOverrides[600];
	int numInvolvedRegOverrides = 0;
	
	ClearRegisters();

	// Evaluate the code
	for (UINT32 addrMajor = (start / 0x10000) * 0x10000; addrMajor < end; addrMajor += 0x10000)
	{
		UINT curStart = addrMajor, curEnd = addrMajor + 0x10000;

		if (curStart < start) curStart = (start / 4) * 4;
		if (curEnd > end) curEnd = (end / 4) * 4;

		// Find regoverrides involved with this code range
		numInvolvedRegOverrides = 0;

		for (int i = 0; i < numRegOverrides && numInvolvedRegOverrides < 600; i ++)
		{
			if (regOverrides[i].address >= curStart && regOverrides[i].address < curEnd)
				involvedRegOverrides[numInvolvedRegOverrides ++] = &regOverrides[i];
		}

		for (UINT32 address = curStart; address < curEnd; address += 4)
		{
			// Check if a breakpoint's been hit and update the registers if so!
#ifdef _DLL
			if (lxSharedData->breakStatus == BREAK_PAUSED)
			{
				if (address == lxSharedData->breakPc)
				{
					for (int i = 0; i < 32; i ++)
						SetExactValue(&regs[i], lxSharedData->breakRegs[i]);
				}
			}
#endif

			// Apply any register name and value overrides BEFORE
			for (int i = 0; i < numInvolvedRegOverrides; i ++)
			{
				if (address != involvedRegOverrides[i]->address)
					continue;

				RegOverride* regOverride = involvedRegOverrides[i];

				for (int j = 0; j < regOverride->numRegNamers; j ++)
					strcpy(regs[regOverride->regNamers[j].reg].name, regOverride->regNamers[j].name);

				for (int j = 0; j < regOverride->numRegSetters; j ++)
					SetExactValue(&regs[regOverride->regSetters[j].reg], regOverride->regSetters[j].value);
			}

			// Evaluate the inI hstruction
			EvalInstruction(mem[address / 4], address, setComments);

			// Apply additional register value overrides AFTER
			for (int i = 0; i < numInvolvedRegOverrides; i ++)
			{
				if (address != involvedRegOverrides[i]->address)
					continue;

				RegOverride* regOverride = involvedRegOverrides[i];

				for (int j = 0; j < regOverride->numRegSetters; j ++)
					SetExactValue(&regs[regOverride->regSetters[j].reg], regOverride->regSetters[j].value);
			}

			// Update on progress
			if (! (address % 0x00100000) && end - start >= 0x00100000)
				UpdateProgressDialog((address >> 4) * 950 / (end >> 4) + 50);
		}
	}
}

void FindLabels()
{
	Label* preserveLabels = (Label*) Alloc(numlabels * sizeof (Label));
	int numPreserveLabels = 0;

	// Remake the label list, preserving the user-defined labels and removing the automatically-generated ones.
	// Additionally, for the automatically-generated ones, change the data type of the surrounding region back from 'byte' to the default
	for (int i = 0; i < numlabels; i ++)
	{
		if (labels[i].autoGenerated)
		{
			int labelLen = strlen(labels[i].string);

			for (int j = labels[i].address >> 2; j < ((labels[i].address + labelLen + 2) >> 2); j ++)
				lines[j].datatype = DATATYPE_CODE;
		}
		else
			preserveLabels[numPreserveLabels ++] = labels[i];
	}

	memcpy(labels, preserveLabels, numPreserveLabels * sizeof (Label));
	labels = (Label*) Realloc(labels, (numlabels + 256) * 256 / 256 * sizeof (Label));
	numlabels = numPreserveLabels;
	Free(preserveLabels);

	for (addr i = 0x00000000; i < memlen8; i += 4)
	{
		char label[256]; // TODO: Sort out the character limit.
		int  label_length;
		bool got_label = 0;

		for (int l = 0; l < 256; l ++)
		{
			if (i + l >= memlen8)
				break;

			char character = mem8[i + l];

			label[l] = character;

			if (character == 0 && l > 4 && mem8[i - 1] == 0) // Make sure the previous character, before this string, is 0.
																   // We don't really want labels that are partially cut-off.
			{
				got_label = 1;

				label_length = l;

				break;
			}

			if ((character < 0x20 && character != 0x0D && character != 0x0A) || 
				character >= 0x7F)
				break;
		}

		if (got_label)
		{
			// Add some double-quotes before adding the label.
			char add_label[256 + 5];

			sprintf(add_label, "\"%s\"", label);

			AddLabel(i, add_label, 1);

			// Change the data type of this portion of the code.
			for (int l = i >> 2; l < (i + label_length) >> 2; l ++)
				lines[l].datatype = DATATYPE_BYTE;

			i += label_length / 4 * 4 + 4;
		}
	}
}

void AnalyseVisible()
{
	UINT32 function = FindFunctionStart(list.address);
	UINT32 functionEnd = FindFunctionEnd(GetSelAddress(list.maxitems - 1));

	if (functionEnd - function >= 0x00008000) // Function size limit; there's gotta be one!
	{
		function = list.address;
		functionEnd = GetSelAddress(list.maxitems - 1);
	}

	UpdateMemoryRegion(function, functionEnd - function);

	ClearAutoGeneratedComments();
	AnalyseRegion(function, functionEnd, 1);
}

#include <Windows.h>
void FindReferences()
{
	unsigned char* gotFuncs = (unsigned char*) Alloc(memorySize / 4 / 8);
	DWORD startTime = GetTickCount();

	memset(gotFuncs, 0, memorySize / 4 / 8);

	// Clear all by-references first (TODO: should we remove to-references too?)
	for (int i = 0; i < memlen32; i ++)
		lines[i].referenced = 0;

	// Scan for simple references
	for (UINT32 addr = 0; addr < memlen32; addr ++)
	{
		int op = GET_OPERATION(mem[addr]);

		if (ISBRANCHOP(op))
			lines[addr].reference = (addr + ((signed short) GET_IMMEDIATE(mem[addr]) + 1)) * 4;
		else if (op == J || op == JAL)
		{
			lines[addr].reference = GET_ADDRESS(mem[addr]);

			if (op == JAL)
			{
				UINT32 funcAddr = GET_ADDRESS(mem[addr]);

				if (funcAddr >= 0 && funcAddr < memlen8 && 
					! (gotFuncs[funcAddr / 4 / 8] & (1 << (funcAddr / 4 & 0x07))))
				{
					char labelName[20];

					sprintf(labelName, "FUNC_%08X", funcAddr);

					AddLabel(funcAddr, labelName, 1);
					gotFuncs[funcAddr / 4 / 8] |= 1 << ((funcAddr / 4) & 0x07);
				}
			}
		}
		else
			lines[addr].reference = 0xFFFFFFFF;

		if (lines[addr].reference < memlen32)
			lines[lines[addr].reference / 4].referenced |= 1 << (lines[addr].reference & 3);

		// Address references
		if (mem[addr] > 0 && mem[addr] < memlen8)
			lines[mem[addr] / 4].referenced |= 1 << ((mem[addr] & 3) + 4);
	}

	Free(gotFuncs);

	printf("Took %i ms\n", GetTickCount() - startTime);
}

void UpdateReference(UINT32 address)
{
	UINT32 val = mem[address / 4];
	int op = GET_OPERATION(val);
	UINT32 addr = address / 4;

	if (lines[addr].reference == 0xFFFFFFFF && lines[addr].datatype == DATATYPE_CODE)
	{
		if (op == J || op == JAL)
			lines[addr].reference = GET_ADDRESS(val);
		else if (op == BEQ || op == BNE || op == BEQL || op == BNEL || /*op == BGEZ || */op == BLEZ || op == BGTZ || op == BLTZ)
			lines[address / 4].reference = address + ((short) GET_IMMEDIATE(val)) * 4 + 4;
	}

	if (lines[address / 4].reference < memlen32)
		lines[lines[address / 4].reference / 4].referenced |= 1 << (lines[address / 4].reference & 3);
}

void FindFunctions()
{
	/*for (UINT32 addr = 0; addr < memlen32; addr ++)
	{
		int op = GET_OPERATION(mem[addr]);

		if (op == JAL)
		{
			UINT32 funcAddr = GET_ADDRESS(mem[addr]);

			if (funcAddr >= 0 && funcAddr < memlen8)
			{
				char labelName[20];

				sprintf(labelName, "FUNC_%08X", funcAddr);

				AddLabel(labelName, funcAddr);
			}
		}
	}*/
	return;
}

UINT32 FindFunctionStart(UINT32 address)
{
	/*UINT32 startAddress = 0x00000000;

	for (int i = 0; i < numlabels; i ++)
	{
		if (labels[i].address > address)
			continue; // Can't be further than the searched address!

		if (strstr(labels[i].string, "FUNC_"))
		{
			if (labels[i].address >= startAddress)
				startAddress = labels[i].address;
		}
	}
	
	return startAddress;*/
	if (! mem)
		return 0x00000000;

	UINT32 addr;
	for (addr = address / 4; addr > 2; addr --)
	{
		if (mem[addr - 2] == 0x03E00008) // JR RA
			break;
	}

	return addr * 4;
}

UINT32 FindFunctionEnd(UINT32 address)
{
	if (! mem)
		return 0x00000000;

	UINT32 addr;

	for (addr = address / 4; addr < memlen32 - 2; addr ++)
	{
		if (mem[addr] == 0x03E00008) // JR RA
			break;
	}

	addr += 4;

	return addr * 4;
}

void FindPattern(const char* string, int type, bool reverse)
{
	int add = reverse ? -1 : 1;
	UINT32 gotoaddr = GetSelAddress(list.sel);
	UINT32 startaddr = GetSelAddress(list.sel);
	UINT32 addr;
	INT32 startOffset = datasizes[lines[startaddr / 4].datatype];

	if (add < 0 && startaddr >= 4)
		startOffset = -datasizes[lines[startaddr / 4 - 1].datatype];

	UpdateMemoryRegion(0x00000000, memlen8);

	// STRING SEARCH
	if (type == 0 || type == 3)
	{
		char lowercase[256];
		int strl = strlen(string);
		UINT32 addr;
		bool casesens = (type == 0) ? 0 : 1;

		// Make a lowercase version of 'string'
		for (int i = 0; i <= strl; i ++)
		{
			lowercase[i] = string[i];

			if (string[i] >= 'A' && string[i] <= 'Z')
				lowercase[i] -= ('A' - 'a');
		}

		// Begin search
		for (addr = startaddr + startOffset; addr != startaddr; addr += add)
		{
			if (addr + strl >= memlen8) addr = 0x00000000; // Wrapped around bottom
			if (addr > memlen8 && add == -1) addr = memlen8 - strl - 1; // Wrapped around top

			if (! casesens)
			{
				int i;

				for (i = 0; i < strl; i ++)
				{
					char c = mem8[addr + i];

					if (c != lowercase[i] && ! (c >= 'A' && c <= 'Z' && c - ('A' - 'a') == lowercase[i]))
						break;
				}

				if (i == strl) // Found! Yay!
					break;
			}
			else
			{
				if (! strncmp(string, (const char*) &mem8[addr], strl))
					break;
			}
		}

		gotoaddr = addr;
	}
	// VALUE/HEX PATTERN SEARCH
	else if (type == 1 || type == 2 || type == 4)
	{
		char data[256];
		int datalen = 0;
		bool badstring = false;
		int strl = strlen(string);
		int addamt = 1 * add;

		if (type == 1 || type == 2)
		{
			for (int i = 0; i + 2 <= strl; )
			{
				UINT32 value;
				char tmpstr[3] = {string[i], string[i + 1], '\0'};

				if (sscanf(tmpstr, "%X", &value))
				{
					data[datalen ++] = value & 0xFF;
					i += 2;

					while ((string[i] < 'A' || string[i] > 'F') && (string[i] < 'a' || string[i] > 'f') && (string[i] < '0' || string[i] > '9') && 
							string[i] != '\0')
						i ++;
				}
				else
					{badstring = true; break;}
			}
		}
		else if (type == 4)
		{
			UINT32 value;
			bool isFloat = (strstr(string, ".") != NULL);

			if ((! isFloat && sscanf(string, "%i", &value) == 1) || (isFloat && sscanf(string, "%f", &value) == 1))
			{
				if (value >> 24) datalen = 4;
				else if (value >> 16) datalen = 3;
				else if (value >> 8) datalen = 2;
				else datalen = 1;

				memcpy(data, &value, datalen);
			}
			else
				badstring = true;
		}

		if (badstring)
		{
			MessageBox(main_hwnd, "Bad hex string! ...I don't even!!", "Do you even hex bro", MB_ICONWARNING);
			return;
		}

		// Reverse the data if searching for a hex value
		if (type == 1)
		{
			char newdata[256];

			for (int i = 0; i < datalen; i ++)
				newdata[i] = data[datalen - i - 1];

			memcpy(data, newdata, sizeof (data));
		}

		if (type == 1 || type == 4)
		{
			// Align the search
			int align = 1;
			if (datalen <= 4) align = 4;
			else if (datalen <= 2) align = 2;
			else if (datalen <= 1) align = 1;

			startaddr = (startaddr / align) * align;
			addamt = align * add;
		}

		for (addr = startaddr + startOffset; addr / abs(addamt) != startaddr / abs(addamt); addr += addamt)
		{
			if (addr + datalen >= memlen8)
			{
				if (addamt > 0)
					addr = 0x00000000;
				else
					addr = memlen8 - datalen - 1;
			}

			if (! memcmp(data, (const char*) &mem8[addr], datalen))
				break;
		}

		gotoaddr = addr;
	}
	// ASM SEARCH
	else if (type == 5)
	{
		UINT32 code, mask;

		if (! ASMToCode(0x00000000, &code, &mask, string))
		{
			MessageBox(main_hwnd, "Invalid code", "SORRY BUT...", MB_ICONWARNING);
			return;
		}

		UINT32 startaddr32 = startaddr / 4;
		for (addr = startaddr32 + startOffset; addr != startaddr32; addr += add)
		{
			if (addr >= memlen32)
			{
				if (add > 0)
					addr = 0x00000000;
				else
					addr = memlen32 - 1;
			}

			if ((mem[addr] & mask) == code)
				break;
		}

		gotoaddr = addr * 4;
	}
	// ABSTRACT PATTERN SEARCH
	else if (type == 6)
	{
		int patternLength = strlen(string);
		UINT32 charValues[256];
		bool doneChars[256];

		int dataSize[3] = {1, 2, 4};
		for (int stage = 0; stage < 3; stage ++)
		{
			int addAmt = dataSize[stage] * add, currentPatternLength = patternLength * dataSize[stage];

			for (addr = startaddr + startOffset; addr != startaddr; addr += add)
			{
				if (addr >= memlen8 && add < 0) addr = memlen8 - currentPatternLength;
				if (addr + currentPatternLength >= memlen8 && add > 0) addr = 0;

				// Clear char values
				for (int i = 0; i < patternLength; i ++)
					doneChars[string[i]] = false;

				// Search
#define STAGESTUFF(memtype) {memtype _mem = (memtype) &mem8[addr]; \
for (int i = 0; i < patternLength; i ++) { \
	if (! doneChars[string[i]]) { \
		for (int j = 0; j < i; j ++) {\
			if (charValues[string[j]] == _mem[i])\
				goto Failure;\
			}\
		charValues[string[i]] = _mem[i]; \
		doneChars[string[i]] = true;} \
	else {\
		if (charValues[string[i]] != _mem[i])\
			goto Failure;}}}
				switch (stage)
				{
					case 0:
						STAGESTUFF(UINT8*);
						break;
					case 1:
						STAGESTUFF(UINT16*);
						break;
					case 2:
						STAGESTUFF(UINT32*);
						break;
				}

				break;
				Failure:
				continue;
			}

			// If the result is the closest one we've had so far, assign it to newAddr
			if (addr != startaddr)
			{
				if (add > 0 && addr > startaddr && addr < gotoaddr)
					gotoaddr = addr;
				else if (add < 0 && addr < startaddr && addr > gotoaddr)
					gotoaddr = addr;
				else if (gotoaddr == startaddr)
					gotoaddr = addr; // No competitors. CLOSE ENOUGH.
			}
		}
	}

	if (gotoaddr != startaddr)
		Goto(gotoaddr, HIST_NOCHANGE);
	else
		MessageBox(main_hwnd, "No results found", "Under the rug", MB_ICONINFORMATION);
}

void FindGp()
{
	gpAddr = 0x00000000;
	gpLock = 0;

	ClearRegisters();

	for (UINT32 addr = 0; addr < memlen32; addr ++)
	{
		int op = GET_OPERATION(mem[addr]);

		if (GET_RD(mem[addr]) == GP)
		{
			if (op != ADD && op != ADDU && op != DADDU && op != DADD && op != OR)
				continue;

			int startAddr = addr - 20, endAddr = addr + 20;

			if (startAddr < 0) startAddr = 0;
			if (endAddr >= memlen32) endAddr = memlen32 - 1;

			for (int testaddr = startAddr; testaddr < endAddr; testaddr ++)
				EvalInstruction(mem[testaddr], testaddr * 4, 0);

			if (regs[GP].value.known)
			{
				if (abs((int) regs[GP].value.v - 0x00500000) < abs((int) gpAddr - 0x00500000))
					gpAddr = regs[GP].value.v;
			}

			ClearRegisters();
		}
	}

	printf("Selected gp: %08X\n", gpAddr);
	SetExactValue(&regs[GP], gpAddr);
	gpLock = 1;
}

/* 
addiu v0,zero,$0000   // v0 = 0
addiu v1,zero,$1337   // v1 = 1337
lw a0,$0000(gp)       // a0 = a0 [unknown]
addiu v1,a0,v0        // v1 = v0 + a0
addiu v1,v1,v0        // v1 = v1 + v0 OR v1 = (v0 + a0) + v0
addiu v0,zero,$0000   // v0 = 0
addiu v1,v1,v0        // v1 = (?? + a0) + v0 */