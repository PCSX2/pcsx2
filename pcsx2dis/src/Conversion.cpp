#include "Main.h"
#include "MainList.h"
#include "Processor.h"
#include "Hacking.h"
#include "Windows.h"
#include "Analyse.h" // UpdateReference
#include "GameShark.h" // For ConfirmEditWindow

#include <cstdlib>
#include <cstdio>
#include <cstring>

struct TempBranch
{
	unsigned int startAddress;
	unsigned int endAddress;
	TempBranch *prev, *next;
};

TempBranch* tempBranches;

int* tempLabels;
int numTempLabels;

int* tempComments;
int numTempComments;

RegOverride** tempOverrides;
int numTempOverrides;

void AddTempLabel(int id)
{
	tempLabels = (int*) Realloc(tempLabels, sizeof (int) * (numTempLabels + 1));
	tempLabels[numTempLabels ++] = id;
}

void ClearTempLabels()
{
	Free(tempLabels);
	tempLabels = NULL;
	numTempLabels = 0;
}

void AddTempComment(int id)
{
	tempComments = (int*) Realloc(tempComments, sizeof (int) * (numTempComments + 1));
	tempComments[numTempComments ++] = id;
}

void ClearTempComments()
{
	Free(tempComments);
	tempComments = NULL;
	numTempComments = 0;
}

void AddTempBranch(unsigned int startAddress, unsigned int endAddress)
{
	TempBranch* newBranch = (TempBranch*) Alloc(sizeof (TempBranch));

	if (tempBranches)
	{
		TempBranch* prevBranch = tempBranches;

		while (prevBranch->next)
			prevBranch = prevBranch->next;

		prevBranch->next = newBranch;
		newBranch->prev = prevBranch;
	}
	else
		tempBranches = newBranch;

	newBranch->next = NULL;
	newBranch->startAddress = startAddress;
	newBranch->endAddress = endAddress;
}

void RemoveTempBranch(TempBranch* branch)
{
	if (branch->prev)
		branch->prev->next = branch->next;
	else
		tempBranches = branch->next;

	if (branch->next)
		branch->next->prev = branch->prev;

	Free(branch);
}

void ClearTempBranches()
{
	TempBranch* curBranch = tempBranches;

	while (curBranch)
	{
		TempBranch* next = curBranch->next;
		Free(curBranch);
		curBranch = next;
	}

	tempBranches = NULL;
}

void AddTempOverride(RegOverride* dur)
{
	tempOverrides = (RegOverride**) Realloc(tempOverrides, (numTempOverrides + 1) * sizeof (RegOverride*));

	tempOverrides[numTempOverrides ++] = dur;
}

void ClearTempOverrides()
{
	Free(tempOverrides);
	tempOverrides = NULL;
	numTempOverrides = 0;
}

void Convert(unsigned int start, unsigned int end)
{
	unsigned int mempos = start;
	unsigned int maxendaddress = start + list.maxitems * 4;

	ClearTempLabels();
	ClearTempComments();

	// Store temp objects (things that appear in this covered address range)
	for (int i = 0; i < numlabels; i ++)
	{
		if (labels[i].address >= start && labels[i].address <= end)
			AddTempLabel(i);
	}

	for (int i = 0; i < numcomments; i ++)
	{
		if (comments[i].address >= start && comments[i].address <= end)
			AddTempComment(i);
	}

	for (int i = 0; i < numRegOverrides; i ++)
	{
		if (regOverrides[i].address >= start && regOverrides[i].address <= end)
			AddTempOverride(&regOverrides[i]);
	}

	// Initialise indentation and add branches
	int curIndentation = 0;

	for (int i = FindFunctionStart(mempos) / 4; i < mempos / 4 + list.maxitems && i < memlen32; i ++) // Hack/todo: list.maxitems may be over bounds
	{
		if (lines[i].datatype != DATATYPE_CODE)
			continue;

		int op = GET_OPERATION(mem[i]);

		if (ISBRANCHOP(op))
		{
			if ((op == BEQ || op == BEQL) && GET_RS(mem[i]) == 0 && GET_RT(mem[i]) == 0)
				continue;

			int offset = GET_OFFSET(mem[i]);

			if (i * 4 + offset >= mempos)
			{
				AddTempBranch(i * 4, i * 4 + offset);

				if (i < mempos / 4 - 1)
				{
					if (offset > 0)
						curIndentation ++;
				}
			}
		}

		if (curIndentation > 15)
		{
			curIndentation = 0;
			break; // Stop--this probably isn't a code area. I mean, come on! 15 indentations?! What kind of programming language is this, Whitespace?!
		}
	}

	/* ----------- BEGIN LIST CONVERSION! ---------- */
	int curItem = 0;
	for (int curItem = 0; mempos < memlen8 && curItem < list.maxitems; curItem ++)
	{
		unsigned int frontColour = RGB(128, 0, 0), backColours[4] = {RGB(160, 208, 255), 0, 0, 0};
		char* address = list.addresses[list.numitems], *label = list.labels[list.numitems], *code = list.code[list.numitems], *comment = list.comments[list.numitems];
		char dataType = lines[mempos / 4].datatype;
		UINT32 value32 = *((UINT32*) &mem8[mempos]), value16 = (value32 & 0xFFFF), value8 = (value32 & 0xFF);
		bool hasBreakpoint = false, hasOverride = false, isFrozen = false;
		int structInstId = -1; UINT32 structInstAddress;
		int numBackColours = 0;

		address[0] = label[0] = code[0] = comment[0] = '\0'; // Reset item text

		// Check if breakpoint is being used here
		for (int i = 0; i < 10; i ++)
		{
			if (breakpoints[i].enabled && breakpoints[i].address == mempos)
				hasBreakpoint = true;
		}
		
		// Check if a temp override is being used here
		for (int i = 0; i < numTempOverrides; i ++)
		{
			if (tempOverrides[i]->address == mempos)
				hasOverride = true;
		}

		// Check if this line is within a struct array
		for (int i = 0; i < numStructInsts; i ++)
		{
			if (mempos >= structInsts[i].address && mempos < structInsts[i].address + structInsts[i].numInsts * structDefs[structInsts[i].structDefId].size)
			{
				if (structInstId != -1 && structInstAddress > structInsts[i].address)
					// Get only the closest one to this address
					continue;

				structInstId = i;
				structInstAddress = structInsts[i].address;
			}
		}

		// Check whether the code is frozen
		for (int i = 0; i < numGameSharkCodes; i ++)
		{
			UINT32 actualMemAddress = gameSharkCodes[i].address & 0x03FFFFFF;

			switch (gameSharkCodes[i].address >> 28)
			{
				case 0x02:
				{
					if (mempos < actualMemAddress + 4 && mempos >= actualMemAddress)
						isFrozen = true;
					break;
				}
				case 0x01:
				{
					if (mempos < actualMemAddress + 2 && mempos >= actualMemAddress)
						isFrozen = true;
					break;
				}
				case 0x00:
				{
					if (mempos == actualMemAddress)
						isFrozen = true;
					break;
				}
			}
		}

#ifdef _DLL
#ifdef NEW_BREAKPOINTS
		// Added for data breakpoints which won't be in the PC breakpoint list
		hasBreakpoint |= (lxSharedData->breakStatus != BREAK_RUNNING && GetSelAddress(list.numitems) == lxSharedData->breakPc);
#endif
#endif

		// SET THE COLOURS
#define ADDCOLOUR(clr) {if (numBackColours < 4) backColours[numBackColours ++] = (clr);}
		if (curItem == list.sel)
		{
			frontColour = RGB(255, 255, 255);
			ADDCOLOUR(RGB(0, 0, 128));
		}
		if (mempos == list.markeraddress && list.markervisible)
		{
			frontColour = RGB(255, 255, 255);
			ADDCOLOUR(RGB(127, 126, 127));
		}
		if (hasBreakpoint)
		{
#if defined(NEW_BREAKPOINTS) && defined(_DLL)
			if (lxSharedData->breakStatus != BREAK_RUNNING && lxSharedData->breakPc == mempos)
			{
				frontColour = RGB(255, 255, 255);
				ADDCOLOUR(RGB(128, 0, 0));
			}
			else
#endif
			{
				frontColour = RGB(255, 255, 255);
				ADDCOLOUR(RGB(0, 128, 0));
			}
		}
		if (mempos == GetBreakWindowReadBreakpoint() || mempos == GetBreakWindowWriteBreakpoint())
		{
			if (mempos == GetBreakWindowReadBreakpoint() && mempos == GetBreakWindowWriteBreakpoint())
				frontColour = RGB(0, 0, 0);
			else if (mempos == GetBreakWindowWriteBreakpoint())
				frontColour = RGB(255, 0, 0);
			else if (mempos == GetBreakWindowReadBreakpoint())
				frontColour = RGB(0, 255, 0);
			ADDCOLOUR(RGB(0, 0, 255));
		}
		if (hasOverride)
		{
			frontColour = RGB(255, 255, 255);
			ADDCOLOUR(RGB(255, 128, 0));
		}
		if (structInstId != -1 && structInstAddress == mempos)
		{
			frontColour = RGB(255, 255, 255);
			ADDCOLOUR(RGB(140, 60, 140));
		}

		// Set address value, adding special characters if possible
		UINT32 curSelAddr = GetSelAddress(list.sel);
		char addressPrepend[5] = " ", valuePrepend[5] = " ";
		int offset;
		
		if (dataType == DATATYPE_CODE && mem[mempos / 4] == 0x03E00008) // Function end
			strcpy(addressPrepend, "\xB1\xF9\xB0");
		else if (lines[curSelAddr / 4].datatype == DATATYPE_CODE && GetOffset(&offset, curSelAddr, mem[curSelAddr / 4]))
		{
			UINT sAddr = curSelAddr, dAddr = sAddr + offset * 4;

			if (mempos >= sAddr && mempos < dAddr)
				strcpy(addressPrepend, "\xFC");
			else if (mempos <= sAddr && mempos > dAddr)
				strcpy(addressPrepend, "\xFB\xB0");
			else if (mempos == dAddr)
			{ //0048d9ac
				strcpy(addressPrepend, "\xFA");
				strcpy(valuePrepend, "\x01 ");
			}
		}
		else if (isFrozen)
			strcpy(addressPrepend, "*");

		// Finally, convert address and value to the appropriate string based on data type
		char* valueFormat = "%s%08X%s%08X";
		UINT32 value = value32;

		if (dataType == DATATYPE_BYTE)
		{
			char store_dashed[9];

			switch (mempos % 4)
			{
				case 0: valueFormat = "%s%08X%s------%02X"; break;
				case 1: valueFormat = "%s%08X%s----%02X--"; break;
				case 2: valueFormat = "%s%08X%s--%02X----"; break;
				case 3: valueFormat = "%s%08X%s%02X------"; break;
			}

			sprintf(code, ".byte   $%02X(%s%s%i '%c')", value8, (value8 > 99 ? "" : " "), (value8 > 9 ? "" : " "), value8, (value8 != 0 ? value8 : ' '));
			value = value8;
		}
		else if (dataType == DATATYPE_HWORD)
		{
			if ((mempos % 4) / 2 == 0)
				valueFormat = "%s%08X%s----%04X";
			else
				valueFormat = "%s%08X%s%04X----";

			sprintf(code, ".half   $%04X (%i)", value16, (signed short) value16);
			value = value16;
		}
		else if (dataType == DATATYPE_WORD)
		{
			Label* gotlabel = NULL;

			for (int i = 0; i < numlabels; i ++)
			{
				if (labels[i].address == value32)
					gotlabel = &labels[i];
			}

			if (gotlabel)
				sprintf(code, ".word   $%08X (%s) (%i)", value32, gotlabel->string, value32);
			else
				sprintf(code, ".word   $%08X (%i)", value32, value32);
		}
		else if (dataType == DATATYPE_FLOAT)
		{
			float iamtotallyafloat;
			*(UINT32*) &iamtotallyafloat = value32;
			sprintf(code, ".float   %f", iamtotallyafloat);
		}

		// Set the line's address/value
		sprintf(address, valueFormat, addressPrepend, mempos, valuePrepend, value);

		// Set the line's label
		int i;
		for (i = 0; i < numTempLabels; i ++)
		{
			if (labels[tempLabels[i]].address == mempos)
			{
				strcpy(label, labels[tempLabels[i]].string);
				break;
			}
		}

		if (i == numTempLabels)
		{
			bool gotVar = false;
			// Use a struct label if possible
			if (structInstId != -1)
			{
				StructDef* def = &structDefs[structInsts[structInstId].structDefId];
				UINT32 closestStartAddress = structInstAddress + ((mempos - structInstAddress) / def->size) * def->size;
				int varId = -1;
				int index = 0;

				for (i = 0; i < def->numVars; i ++)
				{
					if (mempos >= closestStartAddress + def->vars[i].offset && 
						mempos < closestStartAddress + def->vars[i].offset + (datasizes[def->vars[i].dataType]) * def->vars[i].numItems)
					{
						varId = i;
						index = mempos - (closestStartAddress + def->vars[i].offset) / datasizes[def->vars[i].dataType];
						break;
					}
				}

				if (varId != -1)
				{
					if (def->vars[i].numItems > 1)
						sprintf(label, "%s[%i]", def->vars[i].name, index);
					else
						strcpy(label, def->vars[i].name);

					gotVar = true;
				}
			}

			if (! gotVar)
			{
				// If not, look for references instead
				if (lines[mempos / 4].referenced & (1 << (mempos & 3)))
					sprintf(label, "__%08X", mempos );
				else if (lines[mempos / 4].referenced & (1 << ((mempos & 3) + 4))) // Potential pointer reference
					sprintf(label, "\x02__%08X", mempos );
			}
		}

		// Set the line's comment
		if (dataType == DATATYPE_CODE)
		{
			for (i = 0; i < numTempComments; i ++)
			{
				if (comments[tempComments[i]].address == mempos)
					strcpy(comment, comments[tempComments[i]].string);
			}
		}

		// Set the line's code (moved down here because massive code block)
		if (dataType == DATATYPE_CODE)
		{
			char codeSrc[100], codeDest[100];
			int op = GET_OPERATION(mem[mempos / 4]);
			int offset;
			UINT32 addrAligned = mempos / 4 * 4;

			CodeToASM(codeDest, mempos, mem[mempos / 4]);
			strcpy(codeSrc, codeDest);

			if (GetOffset(&offset, mempos, mem[addrAligned / 4]))
			{
				int labelId;

				for (labelId = 0; labelId < numlabels; labelId ++)
				{
					if (labels[labelId].address == addrAligned + (offset * 4))
						break;
				}

				sprintf(codeDest, "%s (\xB1\x04%i%s\xB0\x01%s%s)", codeSrc, offset, offset > 0 ? "\xff" : (offset < 0 ? "\xfe" : "\xfd"), 
					labelId != numlabels ? " " : "", labelId != numlabels ? labels[labelId].string : "");
				strcpy(codeSrc, codeDest);
			}

			if (lines[curSelAddr / 4].datatype == DATATYPE_CODE && GetOffset(&offset, curSelAddr, mem[curSelAddr / 4]))
			{
				UINT sAddr = curSelAddr, dAddr = sAddr + offset * 4;

				if ((addrAligned >= sAddr && addrAligned < dAddr) || (addrAligned <= sAddr && addrAligned > dAddr))
					sprintf(codeDest, "\x05%s", codeSrc);
				else if (addrAligned == dAddr)
					sprintf(codeDest, "\x03\xB1%s", codeSrc);

				strcpy(codeSrc, codeDest);
			}

			TempBranch* branchCheck = tempBranches;

			while (branchCheck)
			{
				if (branchCheck->endAddress > branchCheck->startAddress && mempos == branchCheck->endAddress)
					curIndentation --;
				if (branchCheck->endAddress > branchCheck->startAddress && mempos == branchCheck->startAddress + 4)
					curIndentation ++;
				if (branchCheck->endAddress < branchCheck->startAddress && mempos == branchCheck->startAddress)
					curIndentation --;
				if (branchCheck->endAddress < branchCheck->startAddress && mempos == branchCheck->endAddress)
					curIndentation ++;
				branchCheck = branchCheck->next;
			}

			if (curIndentation < 5)
			{
				for (int i = 100 - curIndentation*2 - 1; i >= 0; i --)
					codeDest[i + curIndentation*2] = codeDest[i];

				for (int i = 0; i < curIndentation*2; i += 2)
				{
					codeDest[i] = '>';
					codeDest[i + 1] = ' ';
				}
			}
			else
			{
				char prepend[12] = "";
				int prependLen;

				sprintf(prepend, ">%i ", curIndentation);

				prependLen = strlen(prepend);

				for (int i = 100 - prependLen - 1; i >= 0; i --)
					codeDest[i + prependLen] = codeDest[i];

				for (int i = 0; i < prependLen; i ++)
					codeDest[i] = prepend[i];
			}

			codeDest[99] = '\0';
			strcpy(code, codeDest);
		}

		// Add all relevant changes to the list!
		list.frontColours[curItem] = frontColour;
		list.datatypes[curItem] = dataType;
		list.numBackColours[curItem] = numBackColours;

		if (numBackColours == 0)
		{
			list.backColours[curItem][0] = backColours[0];
			list.numBackColours[curItem] = 1;
		}
		else
		{
			for (int i = 0; i < numBackColours; i ++)
				list.backColours[curItem][i] = backColours[i];
		}

		list.numitems ++;

		// Move mempos
		mempos += datasizes[dataType];
	}

	ClearTempBranches();
	ClearTempOverrides();
}

bool StringToValue(UINT32 addr, char* string, void** valueOut, int* valueLen)
{
	UINT32 value;

	switch (lines[addr / 4].datatype)
	{
		case DATATYPE_CODE:
		{
			char tmp[512];

			if (! ASMToCode(addr, &value, NULL, string))
				return 0;

			*valueOut = Alloc(4);
			*(UINT32*) *valueOut = value;
			*valueLen = 4;

			return 1;
		}
		case DATATYPE_WORD:
		case DATATYPE_HWORD:
		case DATATYPE_BYTE:
		{
			char data[256];
			int dataSize = datasizes[lines[addr / 4].datatype];

			if (sscanf(string, "$%X", &value))
				memcpy(data, &value, dataSize);
			else if (sscanf(string, "%d", &value))
				memcpy(data, &value, dataSize);
			else if (sscanf(string, "\"%[^\t\n]", string))
			{
				if (strlen(string) < 1)
					return 0;

				if (string[strlen(string) - 1] == '\"')
				{
					string[strlen(string) - 1] = '\0';
					dataSize = strlen(string) + 1;
				}
				else
					dataSize = strlen(string); // Ignore null terminator

				memcpy(data, string, dataSize);
			}
			else return 0;

			*valueOut = Alloc(dataSize);
			*valueLen = dataSize;

			memcpy(*valueOut, data, dataSize);

			return 1;
		}
		case DATATYPE_FLOAT:
		{
			float temp;

			if (sscanf(string, "%f", &temp))
			{
				*valueOut = Alloc(4);
				*(UINT32*) *valueOut = *(UINT32*) &temp;
				*valueLen = 4;
			}

			return 1;
		}
	}

	return 0;
}

void ConfirmEditWindow(UINT32 addr, char* string, int section, bool freeze)
{
	switch (section)
	{
		case 0:
			return;
		case 1:
		{
			UINT32 value;
			
			if (sscanf(string, "%X", &value) == 1)
			{
				SetMemory(addr, &value, datasizes[lines[addr / 4].datatype]);

				if (freeze)
				{
					UINT32 codeType = 0x00000000;
					UINT32 valueMask = 0x000000FF;

					if (datasizes[lines[addr / 4].datatype] == 2)
					{
						codeType = 0x10000000;
						valueMask = 0x0000FFFF;
					}
					else if (datasizes[lines[addr / 4].datatype] == 4)
					{
						codeType = 0x20000000;
						valueMask = 0xFFFFFFFF;
					}

					GameSharkAddCode(addr | codeType, value & valueMask);
				}
			}

			break;
		}
		case 2:
		{
			int labelid;

			// If the string is blank, remove any label here instead
			if (! strlen(string))
			{
				RemoveLabel(addr);
				break;
			}

			for (labelid = 0; labelid < numlabels; labelid ++)
			{
				if (labels[labelid].address == addr) break;
			}

			if (labelid == numlabels)
				AddLabel(addr, string, 0);
			else
			{
				strcpy(labels[labelid].string, string);
				labels[labelid].autoGenerated = 0;
			}

			break;
		}
		case 3:
		{
			void* value;
			int valueLen;

			if (StringToValue(addr, string, &value, &valueLen))
			{
				SetMemory(addr, value, valueLen);

				if (freeze)
				{
					UINT8* u8Value = (UINT8*) value;

					for (int curValuePos = 0; curValuePos < valueLen; )
					{
						int remaining = valueLen - curValuePos;

						if (remaining == 1)
							GameSharkAddCode((addr + curValuePos), u8Value[curValuePos]);
						else if (remaining == 2)
							GameSharkAddCode((addr + curValuePos) | 0x10000000, u8Value[curValuePos] | (u8Value[curValuePos + 1] << 8));
						else if (remaining == 3)
						{
							GameSharkAddCode((addr + curValuePos) | 0x10000000, u8Value[curValuePos] | (u8Value[curValuePos + 1] << 8));
							GameSharkAddCode((addr + curValuePos + 2), u8Value[curValuePos + 2]);
						}
						else if (remaining >= 4)
							GameSharkAddCode((addr + curValuePos) | 0x20000000, *((UINT32*) &u8Value[curValuePos]));

						curValuePos += 4;
					}
				}

				Free(value);

				// Update possible references
				for (UINT32 address = (addr / 4) * 4; address < addr + valueLen; address += 4)
					UpdateReference(address);

				// If this was a string, update any auto-labels associated with it
				for (int i = 0; i < numlabels && 0 ; i ++) // Tofix...
				{
					if (labels[i].address == addr && labels[i].autoGenerated)
					{
						// Replace the label
						char* addrChar = &mem8[addr];
						for (int j = 0; j + 2 < 512 && (UINT32) addrChar < valueLen; )
						{
							if (*addrChar == '\n')
							{
								labels[i].string[j ++] = '\\';
								labels[i].string[j ++];
							}
							else
								labels[i].string[j ++] = *addrChar;

							if (! *addrChar)
								break; // Done!
						}
					}
				}
			}
			break;
		}
	}

	DestroyWindow(wndEdit.hwnd);
	wndEdit.hwnd = NULL;
}