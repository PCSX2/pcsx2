#include "Main.h"
#include "Windows.h"
#include "GameShark.h"

#include <cstdio>

char* gameSharkCodeString = NULL;

GameSharkCode* gameSharkCodes;
int numGameSharkCodes;

INT8*  scanMem8; // Memory snapshot from last scan
INT16* scanMem16; // Ditto
INT32* scanMem32; // Ditto
UINT8* scanMask; // Mask representing data eliminated from the scan and otherwise

void GameSharkScan(const char* value, DataType dataType, ScanType scanType)
{
	bool isFirstScan = false;
	
	UpdateMemoryRegion(0, memlen8);

	if (! scanMem8)
	{
		// Create the memory snapshot and mask
		scanMem8 = (INT8*) Alloc(memlen8);
		scanMem16 = (INT16*) scanMem8;
		scanMem32 = (INT32*) scanMem8;
		scanMask = (UINT8*) Alloc(memlen8 / 8 + 1); // + 1 in case there's somehowa an incomplete byte (e.g. a total of 7 bits would equate to 0 bytes allocated; not good!)

		memcpy(scanMem8, mem8, memlen8);
		memset(scanMask, 0xFF, memlen8 / 8 + 1);

		isFirstScan = true;
	}

	// Scan the world to death
	UINT32 iScanValue = 0;
	bool hexValue = (SendMessage(wndGameShark.btHex, BM_GETCHECK, 0, 0) == BST_CHECKED);

	if (! hexValue)
		sscanf(value, "%i", &iScanValue);
	else
		sscanf(value, "%X", &iScanValue);
	
	// Note: The scan code is optimised for speed, not readability!
	if (scanType == ST_UNKNOWN)
		goto SkipSearch;

	if (dataType == DT_BYTE)
	{
		iScanValue = (int) ((char) iScanValue);
			
		for (int i = 0; i < memlen8; i ++)
		{
			if (! (scanMask[i >> 3] & (1 << (i & 7)))) continue;

			// Test this value and just continue if it's okay
			switch (scanType)
			{
				case ST_EQUAL: if (mem8[i] == iScanValue) continue; break;
				case ST_MORETHAN: if (mem8[i] > iScanValue) continue; break;
				case ST_LESSTHAN: if (mem8[i] < iScanValue) continue; break;
				case ST_CHANGED: if (mem8[i] != scanMem8[i]) continue; break;
				case ST_UNCHANGED: if (mem8[i] == scanMem8[i]) continue; break;
				case ST_INCREASED: if (mem8[i] > scanMem8[i]) continue; break;
				case ST_DECREASED: if (mem8[i] < scanMem8[i]) continue; break;
			}

			// Failed the tests: remove the mask
			scanMask[i >> 3] &= ~(1 << (i & 7));
		}
	}
	else if (dataType == DT_HALF)
	{
		iScanValue = (int) ((short) iScanValue);
	
		for (int i = 0; i < memlen8; i += 2)
		{
			if (! (scanMask[i >> 3] & (1 << (i & 7)))) continue;

			switch (scanType)
			{
				case ST_EQUAL: if (*((INT16*) &mem8[i]) == iScanValue) continue; break;
				case ST_MORETHAN: if (*((INT16*) &mem8[i]) > iScanValue) continue; break;
				case ST_LESSTHAN: if (*((INT16*) &mem8[i]) < iScanValue) continue; break;
				case ST_CHANGED: if (*((INT16*) &mem8[i]) != *((INT16*) &scanMem8[i])) continue; break;
				case ST_UNCHANGED: if (*((INT16*) &mem8[i]) == *((INT16*) &scanMem8[i])) continue; break;
				case ST_INCREASED: if (*((INT16*) &mem8[i]) > *((INT16*) &scanMem8[i])) continue; break;
				case ST_DECREASED: if (*((INT16*) &mem8[i]) < *((INT16*) &scanMem8[i])) continue; break;
			}

			scanMask[i >> 3] &= ~(1 << (i & 7));
		}
	}
	else if (dataType == DT_WORD)
	{
		for (int i = 0; i < memlen8; i += 4)
		{
			if (! (scanMask[i >> 3] & (1 << (i & 7)))) continue;

			switch (scanType)
			{
				case ST_EQUAL: if (*((INT32*) &mem8[i]) == iScanValue) continue; break;
				case ST_MORETHAN: if (*((INT32*) &mem8[i]) > iScanValue) continue; break;
				case ST_LESSTHAN: if (*((INT32*) &mem8[i]) < iScanValue) continue; break;
				case ST_CHANGED: if (*((INT32*) &mem8[i]) != *((INT32*) &scanMem8[i])) continue; break;
				case ST_UNCHANGED: if (*((INT32*) &mem8[i]) == *((INT32*) &scanMem8[i])) continue; break;
				case ST_INCREASED: if (*((INT32*) &mem8[i]) > *((INT32*) &scanMem8[i])) continue; break;
				case ST_DECREASED: if (*((INT32*) &mem8[i]) < *((INT32*) &scanMem8[i])) continue; break;
			}

			scanMask[i >> 3] &= ~(1 << (i & 7));
		}
	}
	else if (dataType == DT_FLOAT)
	{
		float fScanValue = 0.0f;

		sscanf(value, "%f", &fScanValue);

		iScanValue = *((int*) &fScanValue); // Used for faster Equal To searches
		
		for (int i = 0; i < memlen8; i += 4)
		{
			if (! (scanMask[i >> 3] & (1 << (i & 7)))) continue;
			/*if (! (scanMask[(i + 1) >> 3] & (1 << ((i + 1) & 7)))) {i += 1; continue;}
			if (! (scanMask[(i + 2) >> 3] & (1 << ((i + 2) & 7)))) {i += 2; continue;}
			if (! (scanMask[(i + 3) >> 3] & (1 << ((i + 3) & 7)))) {i += 3; continue;}*/

			switch (scanType)
			{
				case ST_EQUAL: if (*((INT32*) &mem8[i]) == iScanValue) continue; break;
				case ST_MORETHAN: if (*((float*) &mem8[i]) > fScanValue) continue; break;
				case ST_LESSTHAN: if (*((float*) &mem8[i]) < fScanValue) continue; break;
				case ST_CHANGED: if (*((INT32*) &mem8[i]) != *((INT32*) &scanMem8[i])) continue; break;
				case ST_UNCHANGED: if (*((INT32*) &mem8[i]) == *((INT32*) &scanMem8[i])) continue; break;
				case ST_INCREASED: if (*((float*) &mem8[i]) > *((float*) &scanMem8[i])) continue; break;
				case ST_DECREASED: if (*((float*) &mem8[i]) < *((float*) &scanMem8[i])) continue; break;
			}

			scanMask[i >> 3] &= ~(1 << (i & 7));
		}
	}

SkipSearch:
	// Update memory snapshot
	if (! isFirstScan)
		memcpy(scanMem8, mem8, memlen8);

	int numActualResults = 0;
	UINT32 actualResults[500];

	// Get the addresses of every result
	switch (dataType)
	{
		case DT_BYTE:
		{
			for (int i = 0; i < memlen8; i ++)
			{	
				if (scanMask[i >> 3] & (1 << (i & 7)))
				{
					if (numActualResults < 500)
						actualResults[numActualResults] = i;

					numActualResults ++;
				}
			}
			break;
		}
		case DT_HALF:
		{
			for (int i = 0; i < memlen8; i += 2) 
			{	
				if (scanMask[i >> 3] & (1 << (i & 7)))
				{
					if (numActualResults < 500)
						actualResults[numActualResults] = i;

					numActualResults ++;
				}
			}
			break;
		}
		case DT_WORD:
		case DT_FLOAT:
		{
			for (int i = 0; i < memlen8; i += 4) 
			{
				if (scanMask[i >> 3] & (1 << (i & 7)))
				{
					if (numActualResults < 500)
						actualResults[numActualResults] = i;

					numActualResults ++;
				}
			}
			break;
		}
	}

	if (numActualResults < 500)
	{
		// Add the results to the list
		SendMessage(wndGameShark.lbResults, LB_RESETCONTENT, 0, 0);

		for (int i = 0; i < numActualResults; i ++)
		{
			char outString[80];

			switch (dataType)
			{
				case DT_BYTE: sprintf(outString, "%08X %08X (%i)", actualResults[i], (UINT8) mem8[actualResults[i]], mem8[actualResults[i]]); break;
				case DT_HALF: sprintf(outString, "%08X %08X (%i)", actualResults[i], *((UINT16*) &mem8[actualResults[i]]), *((INT16*) &mem8[actualResults[i]])); break;
				case DT_WORD: sprintf(outString, "%08X %08X (%i)", actualResults[i], *((UINT32*) &mem8[actualResults[i]]), *((INT32*) &mem8[actualResults[i]])); break;
				case DT_FLOAT:
				{
					UINT32 temp = *(((UINT32*) &mem8[actualResults[i]]));
					sprintf(outString, "%08X %08X (%.6f)", actualResults[i], temp, *((float*) &temp));
					break;
				}
			}

			SendMessage(wndGameShark.lbResults, LB_ADDSTRING, 0, (LPARAM) outString);
			SendMessage(wndGameShark.lbResults, LB_SETITEMDATA, i, actualResults[i]);
		}
	}
	else
	{
		char resString[100];

		if (numActualResults > 0)
			sprintf(resString, "%i results (too many to show)", numActualResults);
		else
			sprintf(resString, "0 results =(", numActualResults);

		SendMessage(wndGameShark.lbResults, LB_RESETCONTENT, 0, 0);
		SendMessage(wndGameShark.lbResults, LB_ADDSTRING, 0, (LPARAM) resString);
	}
}

void GameSharkResetScanner()
{
	Free(scanMem8);
	Free(scanMask);

	scanMem8 = NULL;
	scanMem16 = NULL;
	scanMem32 = NULL;
	scanMask = NULL;

	SendMessage(wndGameShark.lbResults, LB_RESETCONTENT, 0, 0);
}

void GameSharkAddCode(UINT32 address, UINT32 value)
{
/*	SendMessage(wndBreak.edRegs1, EM_SETSEL, SendMessage(wndBreak.edRegs1, EM_LINEINDEX, curSelLine, 0), SendMessage(wndBreak.edRegs1, EM_LINEINDEX, curSelLine, 0));*/
	bool windowOpen = IsWindowVisible(wndGameShark.hwnd);
	char* stringToChange;
	char* addrStart = NULL, *valueStart = NULL; // Pointers to the numbers to replace
	int curSelLine, curScroll;

	// Get the string we're going to use
	if (windowOpen)
	{
		curSelLine = SendMessage(wndGameShark.edCodes, EM_LINEFROMCHAR, -1, 0);
		curScroll = SendMessage(wndGameShark.edCodes, EM_GETFIRSTVISIBLELINE, 0, 0);

		stringToChange = (char*) Alloc(SendMessage(wndGameShark.edCodes, WM_GETTEXTLENGTH, 0, 0) + 20);
		SendMessage(wndGameShark.edCodes, WM_GETTEXT, SendMessage(wndGameShark.edCodes, WM_GETTEXTLENGTH, 0, 0) + 1, (WPARAM) stringToChange);
	}
	else
	{
		int codeStringLen = 0;

		if (gameSharkCodeString)
			codeStringLen = strlen(gameSharkCodeString);

		stringToChange = (char*) Realloc(gameSharkCodeString, codeStringLen + 20);
	}
	
	// Find a code to replace if possible
	char* curChar = stringToChange;
	for (int line = 0; ; line ++)
	{
		int addrLength = 0, valueLength = 0;
		char* tempAddrStart = NULL, *tempValueStart = NULL;
		char addrString[9];

		if (! *curChar)
			break;

		for (; *curChar != '\0'; *curChar ++)
		{
			if (*curChar == '\x0D') continue;
			if (*curChar == '\x0A') {curChar ++; break;}

			if (*curChar == ' ') continue;

			if (addrLength < 8)
			{
				if (tempAddrStart == NULL)
					tempAddrStart = curChar;

				addrString[addrLength ++] = *curChar;
			}
			else if (valueLength < 8)
			{
				if (tempValueStart == NULL)
					tempValueStart = curChar;

				valueLength ++;
			}
			else
				break;
		}

		if (! (addrLength == 8 && valueLength == 8))
			continue;

		UINT32 thisAddress;
			
		sscanf(addrString, "%08X", &thisAddress);

		if ((address & 0x03FFFFFF) == (thisAddress & 0x03FFFFFF))
		{
			// Found the code to replace!
			addrStart = tempAddrStart;
			valueStart = tempValueStart;
			break;
		}
	}

	// Add/replace the code
	if (! (addrStart && valueStart))
	{
		char stringToAppend[20];
		int tempLen = strlen(stringToChange);

		if ((tempLen >= 2 && stringToChange[tempLen - 1] == 0x0A && stringToChange[tempLen - 2] == 0x0D) && tempLen != 1) // !=1 hack so it won't be appended to a single char
			sprintf(stringToAppend, "%08X %08X", address, value); // No newline necessary
		else
			sprintf(stringToAppend, "\x0D\x0A%08X %08X", address, value);

		strcat(stringToChange, stringToAppend);
	}
	else
	{
		char addrString[9], valueString[9];
			
		sprintf(addrString, "%08X", address);
		sprintf(valueString, "%08X", value);

		memcpy(addrStart, addrString, 8);
		memcpy(valueStart, valueString, 8);
	}

	// Update the window string if it's open
	if (windowOpen)
	{
		SendMessage(wndGameShark.edCodes, WM_SETTEXT, 0, (WPARAM) stringToChange);
		SendMessage(wndGameShark.edCodes, EM_LINESCROLL, 0, curScroll);
	}

	// Copy to the GS code string
	if (gameSharkCodeString)
		Free(gameSharkCodeString);

	gameSharkCodeString = stringToChange;

	// Update codes
	GameSharkUpdateCodes();
}

void GameSharkUpdateCodes()
{
	if (gameSharkCodes)
	{
		Free(gameSharkCodes);
		gameSharkCodes = NULL;
		numGameSharkCodes = 0;
	}

	if (! gameSharkCodeString)
		return;

	char* curChar = &gameSharkCodeString[0];

	for (int line = 0; *curChar != '\0'; line ++)
	{
		char curLine[17];
		int curLineLength = 0;

		for (; *curChar != '\0' && curLineLength < 16; *curChar ++)
		{
			if (*curChar == '\x0D') continue;
			if (*curChar == '\x0A') {curChar ++; break;}

			if (*curChar == ' ') continue;

			curLine[curLineLength ++] = *curChar;
		}

		if (curLineLength == 16)
		{
			UINT32 address, value;
			curLine[16] = '\0';

			if (sscanf(curLine, "%08X%08X", &address, &value) == 2)
			{
				gameSharkCodes = (GameSharkCode*) Realloc(gameSharkCodes, (numGameSharkCodes + 1) * sizeof (GameSharkCode));

				// Add the GameShark code
				gameSharkCodes[numGameSharkCodes].address = address;
				gameSharkCodes[numGameSharkCodes].value = value;
				numGameSharkCodes ++;
			}
		}
	}
}