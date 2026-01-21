#include "Main.h"
#include "MainList.h"
#include "Hacking.h" // UpdateMemoryRegion
#include "Analyse.h" // FindFunctionStart/End
#include "Windows.h"
#include "Processor.h" // GET_OPERATION, etc for Right key press
#include "Dialogs.h" // For Find key shorcut
#include "GameShark.h"
#include <cmath>

mainlist_s list;

void EvalInstruction(UINT32 code, UINT32 address, bool setcomments);
void ClearRegisters();

void ConfirmEditWindow(UINT32 addr, char* string, int section, bool freeze = false);
void FindPattern(const char* string, int type, bool casesens, bool hexvalue, bool reverse = 0);

#include <cstdio>
bool ProcessKey(int key)
{
	bool shift = GetKeyState(VK_SHIFT) >> 16;
	bool ctrl = GetKeyState(VK_CONTROL) >> 16;
	UINT32 selectedaddress = list.address;
	UINT32 oldAddress = list.address;
	int oldSel = list.sel;

	for (int i = 0; i < list.sel; i ++)
		selectedaddress += datasizes[lines[selectedaddress / 4].datatype];

	// Edit Box controls
	if (key == VK_ESCAPE && wndEdit.hwnd)
	{
		DestroyWindow(wndEdit.hwnd);
		wndEdit.hwnd = NULL;
		return 0;
	}

	if (GetFocus() == wndEdit.hwnd && wndEdit.hwnd)
	{
		if (key == VK_RETURN)
		{
			char string[512];
			SendMessage(wndEdit.hwnd, WM_GETTEXT, (WPARAM) 512, (LPARAM) string);
			ConfirmEditWindow(wndEdit.address, string, wndEdit.section, HIWORD(GetKeyState(VK_CONTROL)));

			if (wndEdit.address == selectedaddress)
				return 0;
		}

		if (key == VK_ESCAPE)
		{
			DestroyWindow(wndEdit.hwnd);
			wndEdit.hwnd = NULL;
		}

		if (key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT && key != VK_RETURN && key != VK_TAB)
			return 1;
	}

	// Main menu commands
	if (key == 'N' && ctrl)
		UserNewProject();

	if (key == 'S' && ctrl)
		UserSaveProject();

	if (key == 'O' && ctrl)
		UserOpenProject();

	// DOWN
	if (key == VK_DOWN && shift && list.sel < list.maxitems - 1)
	{
		ScrollUp(1);
		list.sel ++;
	}
	else if (key == VK_DOWN && ! shift)
	{
		if (list.sel == list.maxitems - 1)
			ScrollDown(1);
		else
			list.sel ++;
	}

	// UP
	if (key == VK_UP && shift && list.sel > 0)
	{
		ScrollDown(1);
		list.sel --;
	}
	else if (key == VK_UP && ! shift)
	{
		if (list.sel == 0 && list.address > 0)
			ScrollUp(1);
		else if (list.sel > 0)
			list.sel --;
	}

	// PAGE DOWN
	if (key == VK_NEXT && ! shift)
		ScrollDown(list.maxitems);
	else if (key == VK_NEXT && shift)
	{
		ScrollUp(list.maxitems - 1 - list.sel);
		list.sel = list.maxitems - 1;
	}

	// PAGE UP
	if (key == VK_PRIOR && ! shift)
		ScrollUp(list.maxitems);
	else if (key == VK_PRIOR && shift)
	{
		ScrollDown(list.sel);
		list.sel = 0;
	}

	if (list.address < 0) list.address = 0;

	// Datatype changing
	if ((key == 'W' || key == 'H' || key == 'B'/* || key == 'D'*/ || key == 'F' || key == 'U') && ! ctrl)
	{
		int datatype, lastdatatype = lines[selectedaddress / 4].datatype;

		//if (key == 'D') datatype = DATATYPE_DWORD;
		if (key == 'W') datatype = DATATYPE_WORD;
		if (key == 'H') datatype = DATATYPE_HWORD;
		if (key == 'B') datatype = DATATYPE_BYTE;
		if (key == 'F') datatype = DATATYPE_FLOAT;
		if (key == 'U') datatype = DATATYPE_CODE;

		lines[selectedaddress / 4].datatype = datatype;

		int add = 4 / (datasizes[datatype] ? datasizes[datatype] : 1); // Todo
		if (list.sel + add < list.maxitems - 1)
			list.sel += add;
		else
			ScrollDown(add);
	}

	// Dialog box shortcut keys
	if (key == 'G' && ! ctrl)
		Do_GoToAddress();

	if (key == 'G' && ctrl)
		Do_GoToLabel();

	if (key == 'F' && ctrl)
		Do_Find();

	// Mark line
	if (key == VK_SPACE)
	{
		if (list.markeraddress != GetSelAddress(list.sel))
		{
			list.markeraddress = GetSelAddress(list.sel);
			list.markervisible = 1;
		}
		else
			list.markervisible = ! list.markervisible;
	}	

	// Toggle line breakpoint
	if (key == 'B' && ctrl)
		ToggleBreakpoint(GetSelAddress(list.sel));

	// Find reference
	if (key == VK_F3 && list.markervisible)
	{
		UINT32 startaddr = GetSelAddress(list.sel) / 4;
		UINT32 goingto = startaddr*4;
		UINT32 goingtobackup = startaddr*4; // goingtobackup: Go to a pointer of the address if nothing else is found
		UINT32* startmem = &mem[startaddr];
		UINT32* endmem = &mem[memlen32];
		unsigned int* curmem = &mem[shift ? startaddr - 1 : startaddr + 1];

		for (curmem; curmem != startmem;)
		{
			if (lines[curmem-mem].reference == list.markeraddress)
			{
				goingto = (curmem - mem) * 4;
				break;
			}
			else if (*curmem == list.markeraddress && goingtobackup == startaddr*4)
				goingtobackup = (curmem - mem) * 4;

			if (! shift)
			{
				curmem ++;

				if (curmem >= endmem)
					curmem = mem;
			}
			else
			{
				curmem --;

				if (curmem < mem)
					curmem = endmem - 1;
			}
		}

		if (goingto != startaddr*4)
			Goto(goingto, HIST_NOCHANGE);
		else if (goingtobackup != startaddr*4)
			Goto(goingtobackup, HIST_NOCHANGE);
		else
			MessageBox(main_hwnd, "No references found", "Hacker's Life", MB_OK);
	}

	// Continue search/reverse search
	if (key == VK_F5)
		FindPattern(dlgFind.text, dlgFind.type, shift);

	// Follow line to address
	if (key == VK_RIGHT)
	{
		UINT32 value = mem[GetSelAddress(list.sel) / 4];

		// Updates the possible code reference, but only if it's completely obvious
		UpdateReference(GetSelAddress(list.sel));

		if (GET_OPERATION(value) == J || GET_OPERATION(value) == JAL)
			Goto(GET_ADDRESS(value));
		else if (lines[GetSelAddress(list.sel) / 4].datatype == DATATYPE_CODE && lines[GetSelAddress(list.sel) / 4].reference != 0xFFFFFFFF)
			Goto(lines[GetSelAddress(list.sel) / 4].reference);
		else if ((mem[GetSelAddress(list.sel) / 4] & 0x03FFFFFF) < memlen8)
			Goto(mem[GetSelAddress(list.sel) / 4]);
	}

	// Go to offset
	if (key == 'O' && ! ctrl)
		Goto(mem[GetSelAddress(list.sel) / 4] + GetSelAddress(list.sel), (HistFlags) (HIST_ADDNEW | HIST_UPDATEOLD));

	// History
	if (key == VK_LEFT && ! shift)
		AddrHistBack();

	if (key == VK_LEFT && shift)
		AddrHistForward();

	// Edit line
	if (key == VK_RETURN)
		CreateEditWindow(wndEdit.section, list.sel);

	// Change edit box section
	if (key == VK_TAB && wndEdit.hwnd)
	{
		DestroyWindow(wndEdit.hwnd);
		wndEdit.hwnd = NULL;

		if (! shift && wndEdit.address == GetSelAddress(-1))
		{
			if (++ wndEdit.section > 3)
				wndEdit.section = 0;
		}
		else if (wndEdit.address == GetSelAddress(-1))
		{
			if (-- wndEdit.section < 0)
				wndEdit.section = 3;
		}

		CreateEditWindow(wndEdit.section, list.sel);
	}

	// Move edit box if selection changed (new behaviour)
	if ((oldSel != list.sel || oldAddress != list.address) && wndEdit.hwnd)
	{
		DestroyWindow(wndEdit.hwnd);
		wndEdit.hwnd = NULL;

		if (key != VK_NEXT && key != VK_PRIOR) // If we're jumping a page we might as well just destroy the box
			CreateEditWindow(wndEdit.section, list.sel);
	}

	// Analyse the current function if possible
	//DWORD start = GetTickCount();
	UINT32 curFunction = FindFunctionStart(list.address);
	UINT32 curFunctionEnd = FindFunctionEnd(GetSelAddress(list.maxitems - 1));
	static UINT32 lastFunction = 0x00000000, lastFunctionEnd = 0x00000000;

/*	char temp[200];
	sprintf(temp, "Took %i", GetTickCount() - start);
	MessageBox(main_hwnd, temp, "", MB_OK);*/

	if (curFunctionEnd - curFunction >= 0x00008000) // Function size limit; there's gotta be one!
	{
		/*if (curFunction < GetSelAddress(list.sel) - 0x00004000)
			curFunction = GetSelAddress(list.sel) - 0x00004000;
		
		if (curFunctionEnd > GetSelAddress(list.sel) + 0x00004000)
			curFunctionEnd = GetSelAddress(list.sel) + 0x00004000;*/
		curFunction = list.address;
		curFunctionEnd = GetSelAddress(list.maxitems - 1);
	}
	else
	{
		curFunction = lastFunction;
		curFunctionEnd = lastFunctionEnd;
	}
	
	if (curFunction != lastFunction || curFunctionEnd != lastFunctionEnd)
	{
		UpdateMemoryRegion(curFunction, curFunctionEnd - curFunction);

		ClearAutoGeneratedComments();
		AnalyseRegion(curFunction, curFunctionEnd, 1);

		// Update lastFunction
		lastFunction = curFunction;
		lastFunctionEnd = curFunctionEnd;
	}

	RepositionEditWindow();
	UpdateList();
	UpdateTextbox();

	if (wndEdit.hwnd && GetAddressSel(wndEdit.address) == list.sel)
		SetFocus(wndEdit.hwnd);
	else
		SetFocus(main_hwnd);
	
	// SET THE ACTUAL SEL HERE
	SendMessage(listbox_hwnd, LB_SETCURSEL, list.sel, 0);

	if (key == VK_RETURN && GetFocus() == wndEdit.hwnd) return 1; // Hack (well dur): No annoying beep sound from Windows

	return 0;
}

void ClearItems()
{
	list.numitems = 0;
}

void Convert(unsigned int start, unsigned int end);
void UpdateList()
{
	UpdateMemoryRegion(list.address, list.maxitems * 4);

	ClearItems();
	Convert(list.address, list.address + (list.maxitems * 4));

	RECT ca;
	int yOffset = (int) (list.addressFraction * 14.0f);

	GetClientRect(main_hwnd, &ca);
	ca.bottom = (ca.bottom + 13) / 14 * 14; // Go just beyond the window boundary to avoid any annoying slowscrolls
	
	MoveWindow(listbox_hwnd, 0, listbox_y - yOffset, ca.right, ca.bottom - listbox_y + yOffset, 0);
	UpdateWindow(listbox_hwnd);
	RedrawWindow(listbox_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_NOCHILDREN);
	UpdateWindow(textbox_hwnd);
	RedrawWindow(textbox_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_NOCHILDREN);
}

int mouseYHistory[30];
DWORD mouseTimeHistory[30];

void UpdateListScroll()
{
	UINT32 deltaTime = GetTickCount() - list.lastScrollUpdateTime;

	if (deltaTime > 1000)
		deltaTime = 1000; // Too long

	// Do mouse input check
	POINT mouse;

	GetCursorPos(&mouse);

	// Add mouse position to history
	for (int i = 30-1; i >= 1; i --)
	{
		mouseYHistory[i] = mouseYHistory[i - 1];
		mouseTimeHistory[i] = mouseTimeHistory[i - 1];
	}

	mouseYHistory[0] = mouse.y;
	mouseTimeHistory[0] = GetTickCount();

	static bool mouseLastDown;
	float scrollAmt = ((int) ((list.scrollSpeed * 100.0f) * deltaTime)) / 100000.0f;

	if (list.scrollSpeed != list.scrollSpeed)
		list.scrollSpeed = 0.0f; // Floating-point bugfix?

	if (HIWORD(GetKeyState(VK_MBUTTON)))
	{
		scrollAmt = (mouseYHistory[1] - mouse.y) / 14.0f;
		
		DWORD totalTime = 0;
		float totalSpeed = 0;
		int i;
		for (i = 1; i < 30 && totalTime < 100; i ++)
		{
			totalSpeed += mouseYHistory[i - 1] - mouseYHistory[i];
			totalTime += mouseTimeHistory[i - 1] - mouseTimeHistory[i];
		}

		totalSpeed /= (float) i; // Now the mean speed

		list.scrollSpeed = -totalSpeed / totalTime * 1000.0f / 14.0f;
	}
	else
	{
		list.scrollSpeed *= pow(0.8f, ((float) deltaTime) / 100.0f);

		if ((list.scrollSpeed >= -0.5f && list.scrollSpeed <= 0.5f && (list.addressFraction <= 0.1f || list.addressFraction >= 0.9f)) || 
			(list.scrollSpeed >= -0.05f && list.scrollSpeed <= 0.05f))
		{
			list.scrollSpeed = 0.0f;

			if (list.addressFraction >= 0.5f)
				ScrollDown(1);

			list.addressFraction = 0;
			scrollAmt = 0.0f;
		}
	}

	mouseLastDown = HIWORD(GetKeyState(VK_MBUTTON));
	
	// Scroll the list
	if (list.address <= 0 && scrollAmt < 0.0f)
	{
		scrollAmt = 0.0f;
		list.addressFraction = 0.0f;
	}

	if (scrollAmt <= -1.0)
		ScrollUp((int) -scrollAmt);
	if (scrollAmt >= 1.0)
		ScrollDown((int) scrollAmt);

	list.addressFraction += scrollAmt - (int) scrollAmt;

	if (list.addressFraction >= 1.0f)
	{
		list.addressFraction -= 1.0f;
		ScrollDown(1);
	}
	else if (list.addressFraction <= -1.0f)
	{
		list.addressFraction += 1.0f;
		ScrollUp(1);
	}

	if (scrollAmt != 0.0f)
		UpdateList();

	list.lastScrollUpdateTime = GetTickCount();
}

void AddrHistAdd()
{
	list.addrhist = (unsigned int*) Realloc(list.addrhist, (list.addrhistpos + 2) * sizeof (unsigned int));

	list.addrhistpos ++;
	list.addrhist[list.addrhistpos] = GetSelAddress(list.sel);
	list.addrhistmax = list.addrhistpos;
}

void AddrHistBack()
{
	if (list.addrhistpos > 0)
	{
		Goto(list.addrhist[list.addrhistpos - 1], HIST_UPDATEOLD);
		list.addrhistpos --;
	}
}

void AddrHistForward()
{
	if (list.addrhistpos < list.addrhistmax)
	{
		Goto(list.addrhist[list.addrhistpos + 1], HIST_NOCHANGE);
		list.addrhistpos ++;
	}
}

void Goto(UINT32 address, HistFlags histFlags)
{
	UINT32 oldAddr = GetSelAddress(-1);

	if (memlen8 < 0x01000000)
		address &= 0x00FFFFFF; // Fix for 80****** addresses
	if (memlen8 <= 0x02000000)
		address &= 0x03FFFFFF;

	if (GetAddressSel(address) != -1) // It's in view
		list.sel = GetAddressSel(address);
	else
	{
		list.address = address;

		if (list.address + list.maxitems * 4 >= memlen8)
			list.address = memlen8 - list.maxitems * 4;

		if (list.sel != -1)
			ScrollUp(list.sel);
		else
		{
			list.sel = list.maxitems / 2;
			ScrollUp(list.sel);
		}
	}

	AnalyseVisible();
	RedrawWindow(listbox_hwnd, NULL, NULL, RDW_INVALIDATE);

	if (histFlags & HIST_UPDATEOLD)
		list.addrhist[list.addrhistpos] = oldAddr;
	if (histFlags & HIST_ADDNEW)
		AddrHistAdd();
}