#include <windows.h>
#include <WindowsX.h> // GET_X_LPARAM etc
#include <iostream>
#include <Psapi.h>
#include <CommCtrl.h>

#include "Main.h"
#include "MainList.h"
#include "Logical.h"
#include "Windows.h"
#include "Resources.h"
#include "Analyse.h" // For skipping empty lines when shifting list view.
#include "Hacking.h"
#include "Processor.h" // ASMToCode
#include "Dialogs.h"
#include "GameShark.h"

HWND main_hwnd;
HWND listbox_hwnd;
HWND textbox_hwnd;

wndedit_t wndEdit;
wndbreak_t wndBreak;
wndgameshark_t wndGameShark;
wndstruct_t wndStruct;
wndstructmanager_t wndStructManager;

HFONT global_mainfont     = NULL;
HFONT global_listfont     = NULL;
HFONT global_boldlistfont = NULL;
HFONT global_gotoaddrfont = NULL;
HFONT global_notepadfont  = NULL;

int sectionx[6] = {1, 72, 140, 330, 620};

int listbox_y = 14;

HMENU dropMenu;

WNDPROC DefTextBoxProc;

HINSTANCE globalInst;

LRESULT CALLBACK WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextBoxProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK BreakWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK GameSharkWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StructWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK StructManagerWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DialogProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void DrawListItem(DRAWITEMSTRUCT* item, const char* text, int x, int y, int maxChars);

void Convert(unsigned int, unsigned int);
bool StringToValue(UINT32 addr, char* string, void** valueOut, int* valueLen);
void SaveMemoryDump();
void CreateBreakWindow();
void CreateGameSharkWindow();
BOOL CALLBACK SetChildFonts(HWND hwnd, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hNull, LPSTR lpszCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;

	InitCommonControls();

	globalInst = hInstance;

	/*AllocConsole();

	freopen("CONIN$",  "rb", stdin);
	freopen("CONOUT$", "wb", stdout);
	freopen("CONOUT$", "wb", stderr);*/

    wc.cbSize        = sizeof (WNDCLASSEX);
    wc.hInstance     = hInstance;
    wc.lpszClassName = "PS2DEClass";
    wc.lpfnWndProc   = WindowProc;
    wc.style         = CS_DBLCLKS;
    wc.hIcon         = NULL;
    wc.hIconSm       = NULL;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName  = NULL;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hbrBackground = (HBRUSH) (COLOR_APPWORKSPACE) + 1;

    RegisterClassEx(&wc);

	wc.lpszClassName = "BreakerClass";
	wc.lpfnWndProc = BreakWindowProc;

	RegisterClassEx(&wc);

	wc.lpszClassName = "GameSharkClass";
	wc.lpfnWndProc = GameSharkWindowProc;

	RegisterClassEx(&wc);

	wc.lpszClassName = "StructClass";
	wc.lpfnWndProc = StructWindowProc;

	RegisterClassEx(&wc);

	wc.lpszClassName = "StructManagerClass";
	wc.lpfnWndProc = StructManagerWindowProc;

	RegisterClassEx(&wc);

    main_hwnd = CreateWindowEx(
        0, 
        "PS2DEClass", 
        DEFAULTWINDOWTITLE, 
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
        CW_USEDEFAULT, CW_USEDEFAULT, 
        700, 500, 
        NULL, 
        NULL, 
        hInstance, 
        NULL);

	listbox_hwnd = CreateWindowEx(
		0, 
		"LISTBOX", 
		"", 
		WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 
		0, listbox_y, 
		598, 498, 
		main_hwnd, 
		NULL, 
		hInstance, 
		NULL);

	textbox_hwnd = CreateWindowEx(
		0, 
		"EDIT", 
		"", 
		WS_VISIBLE | WS_CHILD | ES_READONLY, 
		0, 0, 0, 14, 
		main_hwnd, 
		NULL, 
		hInstance, 
		NULL);

	DefTextBoxProc = (WNDPROC) SetWindowLongPtr(textbox_hwnd, GWLP_WNDPROC, (LONG_PTR) TextBoxProc);

	// Create the main fonts.
	global_mainfont
		= CreateFont(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Microsoft Sans Serif");
	global_listfont
		= CreateFont(14, 7, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Courier New");
	global_boldlistfont
		= CreateFont(14, 7, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Courier New");
	global_gotoaddrfont
		= CreateFont(14, 5, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Microsoft Sans Serif");
	global_notepadfont
		= CreateFont(13, 8, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");

	// Set textbox font
	SendMessage(textbox_hwnd, WM_SETFONT, (WPARAM) global_listfont, 0);

	// Create the break window (after the fonts are created; it uses them)
	CreateBreakWindow();

	// Create the GameShark window (ditto for fonts)
	CreateGameSharkWindow();

	// Create the Struct Manager window
	CreateStructManagerWindow();

	// Create the menus.
	HMENU menu_main = CreateMenu(), menu_file = CreateMenu(), menu_edit = CreateMenu(), 
	      menu_analyse = CreateMenu(), menu_breakpoints = CreateMenu(), menu_gameshark = CreateMenu();

	AppendMenu(menu_main, MF_ENABLED | MF_POPUP, (UINT_PTR) menu_file, "File");
	AppendMenu(menu_main, MF_ENABLED | MF_POPUP, (UINT_PTR) menu_edit, "Edit");
	AppendMenu(menu_main, MF_ENABLED | MF_POPUP, (UINT_PTR) menu_analyse, "Analyse");
	AppendMenu(menu_main, MF_ENABLED | MF_POPUP, (UINT_PTR) menu_gameshark, "GameShark");
#ifdef _DLL
	AppendMenu(menu_main, MF_ENABLED | MF_POPUP, (UINT_PTR) menu_breakpoints, "Breakpoints");
#endif
	
	AppendMenu(menu_file, MF_ENABLED | MF_STRING, 10, "New...\tCtrl+N");
	AppendMenu(menu_file, MF_ENABLED | MF_STRING, 11, "Open Project...\tCtrl+O");
	AppendMenu(menu_file, MF_ENABLED | MF_STRING, 12, "Save Project\tCtrl+S");
	AppendMenu(menu_file, MF_ENABLED | MF_STRING, 13, "Save Project As...");
	AppendMenu(menu_file, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(menu_file, MF_ENABLED | MF_STRING, 14, "Save Memory Dump...");

	AppendMenu(menu_edit, MF_ENABLED | MF_STRING, 20, "Find...\tCtrl+F");
	AppendMenu(menu_edit, MF_ENABLED | MF_STRING, 21, "Go to Label\tCtrl+G");
	AppendMenu(menu_edit, MF_ENABLED | MF_STRING, 22, "Go to Address\tG");
	AppendMenu(menu_edit, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(menu_edit, MF_ENABLED | MF_STRING, 22, "Set Data Types...");
	AppendMenu(menu_edit, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(menu_edit, MF_ENABLED | MF_STRING, 23, "Struct Definitions...");

	AppendMenu(menu_analyse, MF_ENABLED | MF_STRING, 30, "Analyse All");
	AppendMenu(menu_analyse, MF_ENABLED | MF_STRING, 31, "Analyse Labels");
	
#ifdef _DLL
	AppendMenu(menu_breakpoints, MF_ENABLED | MF_STRING, 40, "Breakpoints...");
	AppendMenu(menu_breakpoints, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(menu_breakpoints, MF_ENABLED | MF_STRING, 41, "Toggle PC breakpoint\tCtrl+B");
	AppendMenu(menu_breakpoints, MF_ENABLED | MF_STRING, 42, "Clear all PC breakpoints");
#endif

	AppendMenu(menu_gameshark, MF_ENABLED | MF_STRING, 50, "Gameshark Tools...");

	SetMenu(main_hwnd, menu_main);

	// Create the right-click drop-down menu
	dropMenu = CreatePopupMenu();
	
#ifdef _DLL
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 105, "Freeze");
	AppendMenu(dropMenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 100, "Toggle PC Breakpoint\tCtrl+B");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 101, "Clear all PC breakpoints");
	AppendMenu(dropMenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 102, "Set as Read Breakpoint");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 103, "Set as Write Breakpoint");
	AppendMenu(dropMenu, MF_DISABLED | MF_SEPARATOR, 0, "");
#endif
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 106, "Set Data Types...");
	AppendMenu(dropMenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 107, "Place Struct...");
	AppendMenu(dropMenu, MF_DISABLED | MF_SEPARATOR, 0, "");
	AppendMenu(dropMenu, MF_ENABLED | MF_STRING, 104, "Override Registers...");

	// Send final messages...
	SendMessage(listbox_hwnd, LB_SETITEMHEIGHT, 0, 14);

	SendMessage(main_hwnd, WM_SIZE, SIZE_RESTORED, MAKEWORD(600, 500));
	UpdateWindow(main_hwnd);

	MainStartup();

#ifdef _DLL
	return 1;
#endif

	while (MainUpdate());
	/*bool quit = 0;
	static int lastupdate = 0;
	while (! quit)
	{
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) > 0)
		{
			// HACK! We're sending a copy of the keystroke receipt to main_hwnd as well. In fact we might not even let it through to listbox_hwnd.
			if ((message.hwnd == listbox_hwnd && (message.message == WM_KEYDOWN)) || 
				(message.hwnd == wndEdit.hwnd && (message.message == WM_KEYDOWN)))
			{
				MSG message_copy;
				bool relay_message; // Sometimes we might not want to pass on the message...

				memcpy(&message_copy, &message, sizeof (MSG));

				message_copy.hwnd = main_hwnd;

								TranslateMessage (&message_copy);
				relay_message = DispatchMessage  (&message_copy);

				if (! relay_message)
					continue;
			}

			if (message.message == WM_QUIT)
				quit = 1;

			TranslateMessage (&message);
			DispatchMessage  (&message);
		}

		if (GetTickCount() - lastupdate >= 500)
		{
			int sel = SendMessage(listbox_hwnd, LB_GETCURSEL, 0, 0);
			UpdateList();
			UpdateTextbox();
			SendMessage(listbox_hwnd, LB_SETCURSEL, sel, 0);
			lastupdate = GetTickCount();
		}

		Sleep(33);

		HandleBreakpoints();
	}*/

	GameDataShutdown();

    return 0;
}

int lastclickx, lastclicky;
int mousex, mousey;
DWORD lastclicktime;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int sel = 0;

	switch (message)
	{
		case WM_CLOSE:
#ifndef _DLL
			PostQuitMessage(0);
			break;
#else
			return 0;
#endif
		case WM_PAINT:
		{
			HDC hdc;
			PAINTSTRUCT temp;
			RECT clientr;
			HBRUSH red = CreateSolidBrush(RGB(255, 127, 127));

			GetClientRect(listbox_hwnd, &clientr);

            hdc = 
			BeginPaint(hwnd, &temp);

			SelectObject(hdc, red);
			//Rectangle(hdc, temp.rcPaint.left, temp.rcPaint.top, temp.rcPaint.right, temp.rcPaint.bottom);

            EndPaint(hwnd, &temp);

			DeleteObject(red);

			return 0;
		}
		break;
		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*) lParam;

			if (item->hwndItem == listbox_hwnd)
			{
				if (item->itemID >= list.maxitems || item->itemID < 0)
					return 0;
				
				if (! lines)
					return 0;

				HBRUSH backBrush[4];
				
				for (int i = 0; i < list.numBackColours[item->itemID]; i ++)
					backBrush[i] = CreateSolidBrush(list.backColours[item->itemID][i]);

				SelectObject(item->hDC, global_listfont);
				
				for (int i = 0; i < list.numBackColours[item->itemID]; i ++)
				{
					RECT temp = item->rcItem;
					temp.top = temp.top + i * (temp.bottom - temp.top) / list.numBackColours[item->itemID];
					temp.bottom = temp.top + (i + 1) * (temp.bottom - temp.top) / list.numBackColours[item->itemID];
					FillRect(item->hDC, &temp, backBrush[i]);
				}

				SetBkMode(item->hDC, TRANSPARENT);//SetBkColor(item->hDC, list.backColours[item->itemID][0]);
				SetTextColor(item->hDC, list.frontColours[item->itemID]);

				char data_type = list.datatypes[item->itemID];

				if (item->itemID != list.sel && data_type != DATATYPE_CODE)
					SetTextColor(item->hDC, RGB(0, 0, 0));

				char* addr = list.addresses[item->itemID], *label = list.labels[item->itemID], 
					 *code = list.code[item->itemID], *cmnt = list.comments[item->itemID];

				DrawListItem(item, addr, sectionx[0], item->rcItem.top, strlen(addr) - 1);
				DrawListItem(item, label, sectionx[2], item->rcItem.top, strlen(label) - 1);
				DrawListItem(item, code, sectionx[3], item->rcItem.top, strlen(code) - 1);
				DrawListItem(item, cmnt, sectionx[4], item->rcItem.top, strlen(cmnt) - 1);

				for (int i = 0; i < list.numBackColours[item->itemID]; i ++)
					DeleteObject(backBrush[i]);

				if (item->itemID == GetAddressSel(wndEdit.address) && wndEdit.hwnd)
					RedrawWindow(wndEdit.hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

				return 1;
			}
		}
		break;
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			if ((HWND) lParam != wndEdit.hwnd)
				break;

			COLORREF clr;
			char input[512];
			int cursel = GetAddressSel(wndEdit.address);

			SendMessage(wndEdit.hwnd, WM_GETTEXT, (WPARAM) 512, (LPARAM) input);

			switch (wndEdit.section)
			{
				case 0: clr = RGB(127, 127, 127); break;
				case 1:
				{
					UINT32 val1, val2;

					if (cursel == -1) break;

					if (lines[wndEdit.address / 4].datatype == DATATYPE_BYTE)
						val1 = *(unsigned char*) &mem8[wndEdit.address];
					else if (lines[wndEdit.address / 4].datatype == DATATYPE_HWORD)
						val1 = *(unsigned short*) &mem16[wndEdit.address / 2];
					else
						val1 = mem[wndEdit.address / 4];

					sscanf(input, "%X", &val2);

					if (val1 == val2) clr = RGB(127, 255, 127);
					else              clr = RGB(255, 127, 127);
					break;
				}
				case 2:
					clr = RGB(255, 255, 255);
					break;
				case 3:
				{
					void* memValue;
					int memSize;

					if (StringToValue(wndEdit.address, input, &memValue, &memSize))
					{	
						if (! memcmp(&mem8[wndEdit.address], memValue, memSize))
							clr = RGB(127, 255, 127);
						else
							clr = RGB(255, 255, 127);
					}
					else
						clr = RGB(255, 127, 127);

					break;
				}
			}

			SetBkColor((HDC) wParam, clr);
			return (HRESULT) GetStockObject(GRAY_BRUSH);
		}
		break;
		case WM_SIZE:
		case WM_SIZING:
		{
			if (hwnd == main_hwnd && listbox_hwnd != NULL && wndBreak.hwnd != NULL)
			{
				RECT ca, breakRect, mainRect;

				GetWindowRect(main_hwnd, &mainRect);
				GetWindowRect(wndBreak.hwnd, &breakRect);
				GetClientRect(main_hwnd, &ca);
				ca.bottom = (ca.bottom + 13) / 14 * 14; // Go just beyond the window boundary to avoid any annoying slowscrolls

				MoveWindow(listbox_hwnd, 0, listbox_y, ca.right, ca.bottom - listbox_y, 1);
				MoveWindow(textbox_hwnd, 0, 0, ca.right, 14, 1);

				//SetWindowPos(wndBreak.hwnd, NULL, 0, mainRect.bottom - (breakRect.bottom - breakRect.top) - mainRect.top, ca.right, (breakRect.bottom - breakRect.top), 0);
				SetWindowPos(wndBreak.hwnd, NULL, ca.right - (breakRect.right - breakRect.left), listbox_y, (breakRect.right - breakRect.left), ca.bottom, 0);

				SetWindowPos(wndBreak.edRegs1, NULL, 0, 0, 80, ca.bottom - 296 - 56, SWP_NOMOVE);
				SetWindowPos(wndBreak.gbRegs, NULL, 0, 0, 138, ca.bottom - 277 - 40, SWP_NOMOVE);

				UpdateBreakWindowRegs();

				list.maxitems = (ca.bottom - listbox_y) / SendMessage(listbox_hwnd, LB_GETITEMHEIGHT, 0, 0) - 1;

				if (list.maxitems < 0)
					list.maxitems = 0;

				SendMessage(listbox_hwnd, LB_RESETCONTENT, 0, 0);
				for (int i = 0; i < list.maxitems; i ++)
					SendMessage(listbox_hwnd, LB_ADDSTRING, 0, (LPARAM) "NULL");
			}
		}
		break;
		case WM_KEYDOWN:
			return ProcessKey(wParam);
		break;
		case WM_MOUSEWHEEL:
			if (GET_WHEEL_DELTA_WPARAM(wParam) < 0)
				ScrollDown(-GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 4);
			else if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
				ScrollUp(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 4);
			UpdateList();
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			lastclickx = GET_X_LPARAM(lParam); lastclicky = GET_Y_LPARAM(lParam) - listbox_y;
			list.sel = (WPARAM) (lastclicky) / 14;

			if (wndEdit.hwnd && GetAddressSel(wndEdit.address) == lastclicky / 14)
				SetFocus(wndEdit.hwnd);
			else
				SetFocus(main_hwnd);
			
			//RedrawWindow(listbox_hwnd, NULL, NULL, RDW_INVALIDATE);
			UpdateList();

			if (message == WM_RBUTTONDOWN)
			{
				POINT cursorPos;
				GetCursorPos(&cursorPos);
				BOOL derp = TrackPopupMenu(dropMenu, TPM_RETURNCMD, cursorPos.x, cursorPos.y, 0, main_hwnd, NULL);

				SendMessage(main_hwnd, WM_COMMAND, MAKELONG(derp, 0), 0);
			}

			return 1;
		case WM_LBUTTONDBLCLK:
		{
			int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam) - listbox_y;
			int section = -1;

			for (int i = 0; i < 5; i ++)
			{
				if (mx > sectionx[i])
					section = i;
			}
			
			if (section != -1)
				CreateEditWindow(section, list.sel);

			lastclickx = mx; lastclicky = my;
			return 0; // ? When this returned 1, problems arose re: the editbox's window focus
		}
		case WM_COMMAND:
		{
			if (HIWORD(wParam) != 0)
				break;

			switch (LOWORD(wParam))
			{
				case 10:
					UserNewProject();
					break;
				case 11:
					UserOpenProject();
					break;
				case 12:
					UserSaveProject();
					break;
				case 13:
					UserSaveProjectAs();
					break;
				case 14:
					SaveMemoryDump();
					break;
				case 20:
					Do_Find();
					break;
				case 21:
					Do_GoToLabel();
					break;
				case 22:
					Do_GoToAddress();
					break;
				case 23:
					ShowStructManagerWindow(true);
					break;
				case 30:
				{
					CreateProgressDialog("Analysing memory...");
					UpdateMemoryRegion(0x00000000, memlen8);
					FindLabels();
					FindReferences();
					FindGp();
					UpdateProgressDialog(50);
					AnalyseRegion(0x00000000, memlen8, 0);
					DestroyProgressDialog();
					break;
				}
				case 31:
				{
					CreateProgressDialog("Analysing labels...");
					UpdateMemoryRegion(0x00000000, memlen8);
					FindLabels();
					DestroyProgressDialog();
					break;
				}
				case 40:
					ShowBreakWindow(! IsWindowVisible(wndBreak.hwnd));
					break;
				case 100:
				case 41:
					ToggleBreakpoint(GetSelAddress(list.sel));
					break;
				case 101:
				case 42:
					ClearBreakpoints();
					break;
				case 102:
					SetBreakWindowReadBreakpoint(GetSelAddress(list.sel));
					break;
				case 103:
					SetBreakWindowWriteBreakpoint(GetSelAddress(list.sel));
					break;
				case 104:
					Do_EditRegs();
					break;
				case 105:
				{
					UINT32 address = GetSelAddress(list.sel);

					UpdateMemoryRegion(address, 4);

					UINT32 value = mem[address / 4];
					switch (lines[GetSelAddress(list.sel) / 4].datatype)
					{
						case DATATYPE_WORD:
						case DATATYPE_FLOAT:
						case DATATYPE_CODE:
							address |= 0x20000000;
							break;
						case DATATYPE_HWORD:
							value = mem16[address / 2];
							address |= 0x10000000;
							break;
						case DATATYPE_BYTE:
							value = mem8[address];
							break;
					}

					GameSharkAddCode(address, value);
					break;
				}
				case 106:
				{
					Do_SetDataTypes();
					break;
				}
				case 107:
				{
					Do_PlaceStruct();
					break;
				}
				case 50:
					ShowWindow(wndGameShark.hwnd, true);
					break;
			}

			UpdateList();
		}
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TextBoxProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_KEYDOWN)
	{
		BYTE keyMap[256];
		char c[2];
		DWORD sel1, sel2;

		SendMessage(hwnd, EM_GETSEL, (WPARAM) &sel1, (LPARAM) &sel2);

		GetKeyboardState(keyMap);

		if (ToAscii(wParam, lParam >> 16 & 0xFF, keyMap, (WORD*) c, 0) == 1 && wParam != VK_BACK)
		{
			SetMemory(GetSelAddress(list.sel) + sel1, &c[0], 1);
			sel1 ++; sel2 ++;
			UpdateTextbox();
		}
		else if (wParam == VK_DELETE)
		{
			char itsGreatToFinallyBeBornIntoThisWorld_IAmButAHumbleVariable_AndIHopeToGetAlongWellWithYou = 0;
			short yeahHesNotGonnaLastTwoSecondsHere = 1337;

			SetMemory(GetSelAddress(list.sel) + sel1, &itsGreatToFinallyBeBornIntoThisWorld_IAmButAHumbleVariable_AndIHopeToGetAlongWellWithYou, 1);
		}
		else if (wParam == VK_BACK && sel1 > 0)
		{
			UINT32 addr = GetSelAddress(list.sel);

			for (int i = sel1 - 1; i < 256; i ++)
			{
				SetMemory(addr + i, &mem8[addr + i + 1], 1);

				if (mem8[addr + i + 1] == '\0')
					break;
			}
			
			sel1 --; sel2 --;
		}
		
		SendMessage(hwnd, EM_SETSEL, (WPARAM) sel1, (LPARAM) sel2);
	}

	return CallWindowProc(DefTextBoxProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK BreakWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int sel = 0;

	if (hwnd == wndBreak.gbCallStack)
	{
		if (message == WM_COMMAND)
			return SendMessage(GetParent(hwnd), message, wParam, lParam);
		else
			return CallWindowProc((WNDPROC) wndBreak.procGbCallStack, hwnd, message, wParam, lParam);
	}
	if (hwnd == wndBreak.edRegs1)
	{
		LRESULT derp = CallWindowProc((WNDPROC) wndBreak.procGbRegs, hwnd, message, wParam, lParam);
		if (message == WM_VSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN)
			SendMessage(wndBreak.hwnd, message, wParam, lParam);

		return derp;
	}

	switch (message)
	{
		case WM_CLOSE:
			ShowWindow(hwnd, 0);
			return 0;
		case WM_VSCROLL:
		case WM_MOUSEWHEEL:
		case WM_KEYDOWN:
		{
			UpdateBreakWindowRegs();
			return 0;
		}
		case WM_COMMAND:
		{
			if ((HWND) lParam == wndBreak.btContinue)
				wndBreak.continuePressed = true;
			else if ((HWND) lParam == wndBreak.btStep)
				wndBreak.stepPressed = true;
			else if ((HWND) lParam == wndBreak.btAuto)
				wndBreak.autoContinue = (SendMessage(wndBreak.btAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);
			else if ((HIWORD(wParam) == LBN_DBLCLK || wParam == IDOK) && GetFocus() == wndBreak.liCallStack)
			{
				char addrStr[16];
				UINT addrValue = 0;

				SendMessage(wndBreak.liCallStack, LB_GETTEXT, SendMessage(wndBreak.liCallStack, LB_GETCURSEL, 0, 0), (LPARAM) addrStr);

				if (sscanf(addrStr, "%08X", &addrValue))
					Goto(addrValue);
				else if (sscanf(addrStr, "--> %08X", &addrValue))
					Goto(addrValue);
			}

			break;
		}
		case WM_SIZE:
		case WM_SIZING:
		{
			break;
			RECT ca, breakRect, mainRect;

			GetWindowRect(main_hwnd, &mainRect);
			GetWindowRect(wndBreak.hwnd, &breakRect);
			GetClientRect(main_hwnd, &ca);
				
			if (breakRect.right - breakRect.left <= 5)
				breakRect.right += (5 - (breakRect.right - breakRect.left)); // Hack for a decent minimum size
			
			SetWindowPos(wndBreak.hwnd, NULL, ca.right - (breakRect.right - breakRect.left), listbox_y, (breakRect.right - breakRect.left), ca.bottom, 0);
			break;
		}
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK GameSharkWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (hwnd == wndGameShark.gbActiveCodes)
	{
		// Annoying redirects required for groupbox children
		if (message == WM_COMMAND || message == WM_NOTIFY || message == WM_RBUTTONDOWN)
			return SendMessage(wndGameShark.hwnd, message, wParam, lParam);
		else
			return CallWindowProc((WNDPROC) wndGameShark.procGbActiveCodes, hwnd, message, wParam, lParam);
	}

	if (hwnd == wndGameShark.gbCodeScanner)
	{
		if (message == WM_COMMAND || message == WM_RBUTTONDOWN)
			return SendMessage(wndGameShark.hwnd, message, wParam, lParam);
		else
			return CallWindowProc((WNDPROC) wndGameShark.procGbCodeScanner, hwnd, message, wParam, lParam);
	}

	switch (message)
	{
		case WM_SHOWWINDOW:
		{
			// Do this every time the window is opened
			if (hwnd != wndGameShark.hwnd || wParam != TRUE) break;

			if (! gameSharkCodeString)
				SendMessage(wndGameShark.edCodes, WM_SETTEXT, 0, 
					(LPARAM) "GameShark codes go here!\x0D\nComments may also be added anywhere:\x0D\nAs long as all codes are at the\x0D\nbeginning of their line =)"
					"\x0D\n\x0D\nThat way you can temporarily disable\x0D\nany code, just by sneaking in a\x0D\nrandom character before it"
					"\x0D\n\x0D\nRAW 0, 1, 2 and D-codes are \x0D\ncurrently supported.");
			else
				SendMessage(wndGameShark.edCodes, WM_SETTEXT, 0, (LPARAM) gameSharkCodeString);

			break;
		}
		case WM_SIZE:
		case WM_SIZING:
		{
			if (hwnd != wndGameShark.hwnd)
				break;

			RECT sizeRect;
			GetClientRect(hwnd, &sizeRect); // 556 width, 336 height default
			
			MoveWindow(wndGameShark.gbActiveCodes, 12, 12, sizeRect.right - 229, sizeRect.bottom - 14, true);
			MoveWindow(wndGameShark.gbCodeScanner, sizeRect.right - 211, 12, 207, sizeRect.bottom - 14, true);

			MoveWindow(wndGameShark.edCodes, 7, 22, sizeRect.right - 242, sizeRect.bottom - 69, true);
			MoveWindow(wndGameShark.btApply, 7, sizeRect.bottom - 43, sizeRect.right - 242, 23, true);

			MoveWindow(wndGameShark.lbResults, 6, 152, 195, sizeRect.bottom - 176, true);

			break;
		}
		case WM_COMMAND:
		{
			if ((HWND) lParam == (HWND) wndGameShark.btScan)
			{
				int valueLen = SendMessage(wndGameShark.edSearch, WM_GETTEXTLENGTH, 0, 0);
				char* valueString = (char*) Alloc(valueLen + 1);

				SendMessage(wndGameShark.edSearch, WM_GETTEXT, valueLen + 1, (LPARAM) valueString);

				GameSharkScan(valueString, (DataType) SendMessage(wndGameShark.cbDataType, CB_GETCURSEL, 0, 0), (ScanType) SendMessage(wndGameShark.cbScanMode, CB_GETCURSEL, 0, 0));
				Free(valueString);

				EnableWindow(wndGameShark.btReset, true);
			}

			if ((HWND) lParam == wndGameShark.btReset)
			{
				GameSharkResetScanner();
			
				EnableWindow(wndGameShark.btReset, false);
			}

			if ((HWND) lParam == wndGameShark.lbResults && HIWORD(wParam) == LBN_DBLCLK)
			{
				int curSel = SendMessage(wndGameShark.lbResults, LB_GETCURSEL, 0, 0);

				if (curSel != -1)
				{
					Goto(SendMessage(wndGameShark.lbResults, LB_GETITEMDATA, curSel, 0));
					
					UpdateList();
				}
			}

			if ((HWND) lParam == wndGameShark.btApply || (HWND) lParam == wndGameShark.btOk)
			{
				int stringLength = SendMessage(wndGameShark.edCodes, WM_GETTEXTLENGTH, 0, 0);
				
				if (gameSharkCodeString)
					Free(gameSharkCodeString);

				gameSharkCodeString = (char*) Alloc(stringLength + 1);

				SendMessage(wndGameShark.edCodes, WM_GETTEXT, stringLength + 1, (LPARAM) gameSharkCodeString);

				GameSharkUpdateCodes();

				if ((HWND) lParam == wndGameShark.btOk)
					ShowWindow(hwnd, 0);
			}

			if ((HWND) lParam == wndGameShark.cbDataType && HIWORD(wParam) == CBN_SELCHANGE)
			{
				switch (SendMessage(wndGameShark.cbDataType, CB_GETCURSEL, 0, 0))
				{
					case DT_FLOAT:
					case DT_STRING:
						EnableWindow(wndGameShark.btHex, false);
						break;
					default:
						EnableWindow(wndGameShark.btHex, true);
						break;
				}
			}

			if ((HWND) lParam == wndGameShark.cbScanMode && HIWORD(wParam) == CBN_SELCHANGE)
			{
				switch (SendMessage(wndGameShark.cbScanMode, CB_GETCURSEL, 0, 0))
				{
					case ST_CHANGED:
					case ST_UNCHANGED:
					case ST_INCREASED:
					case ST_DECREASED:
					case ST_UNKNOWN:
						EnableWindow(wndGameShark.edSearch, false);
						break;
					default:
						EnableWindow(wndGameShark.edSearch, true);
						break;
				}
			}

			if ((HWND) lParam == wndGameShark.btCancel)
				ShowWindow(hwnd, 0);

			break;
		}
		case WM_CONTEXTMENU:
		{
			POINT cursorPos, clientCursorPos;

			GetCursorPos(&cursorPos);

			clientCursorPos = cursorPos;
			ScreenToClient(wndGameShark.lbResults, &clientCursorPos);

			SetFocus(wndGameShark.lbResults);

			if (cursorPos.y > 0)
				SendMessage(wndGameShark.lbResults, LB_SETCURSEL, clientCursorPos.y / SendMessage(wndGameShark.lbResults, LB_GETITEMHEIGHT, 0, 0) + 
				SendMessage(wndGameShark.lbResults, LB_GETTOPINDEX, 0, 0), 0);
			
			BOOL derp = TrackPopupMenu(wndGameShark.dropMenu, TPM_RETURNCMD, cursorPos.x, cursorPos.y, 0, wndGameShark.hwnd, NULL);

			if (derp == 100)
			{
				UINT32 address = SendMessage(wndGameShark.lbResults, LB_GETITEMDATA, SendMessage(wndGameShark.lbResults, LB_GETCURSEL, 0, 0), 0);

				if (address < memlen8)
				{
					UpdateMemoryRegion(address, 4);

					DataType dataType = (DataType) SendMessage(wndGameShark.cbDataType, CB_GETCURSEL, 0, 0);
					UINT32 value = *((UINT32*) &mem8[address]);

					switch (dataType)
					{
						case DT_BYTE:
							value &= 0xFF;
							break;
						case DT_HALF:
							address |= 0x10000000;
							value &= 0xFFFF;
							break;
						case DT_WORD:
						case DT_FLOAT:
							address |= 0x20000000;
							break;
					}

					GameSharkAddCode(address, value);
				}
			}
			return 0;
		}
		case WM_CLOSE:
			ShowWindow(hwnd, 0);
			return 0;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK StructWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
			return 0;
		case WM_COMMAND:
		{
			if ((HWND) lParam == wndStruct.btCancel)
			{
				DestroyWindow(hwnd);
				wndStruct.hwnd = NULL;
			}
			else if ((HWND) lParam == wndStruct.btOk)
			{
				int scriptLength = SendMessage(wndStruct.edScript, WM_GETTEXTLENGTH, 0, 0);
				char* fullScript = (char*) Alloc(scriptLength + 1);
				char nameString[50], sizeString[9];
				char curLine[256];
				int curTextPos = 0;
				int error = 0;
				StructDef* newStruct;

				// Get main script text
				SendMessage(wndStruct.edScript, WM_GETTEXT, scriptLength + 1, (LPARAM) fullScript);
				fullScript[scriptLength] = '\0';

				// Get struct name and info
				SendMessage(wndStruct.edName, WM_GETTEXT, 50, (LPARAM) nameString);
				SendMessage(wndStruct.edSize, WM_GETTEXT, 8, (LPARAM) sizeString);
				nameString[49] = '\0'; sizeString[8] = '\0';

				// Preliminari..ly create the new struct
				structDefs = (StructDef*) Realloc(structDefs, sizeof (StructDef) * (numStructDefs + 1));
				newStruct = &structDefs[numStructDefs];
				memset(newStruct, 0, sizeof (StructDef));

				while (curTextPos < scriptLength)
				{
					// Extract the current line from the script
					int sub = curTextPos;
					for (curTextPos; curTextPos <= scriptLength; curTextPos ++)
					{
						if (curTextPos - sub >= 256)
						{
							// Line is a little too long, huh? Probably invalid
							curLine[255] = '\0';
							break;
						}

						curLine[curTextPos - sub] = fullScript[curTextPos];

						if (fullScript[curTextPos] == 0x0D || fullScript[curTextPos] == '\0') // Line end!
						{
							curLine[curTextPos - sub] = '\0';
							curTextPos += 2;
							break;
						}
					}

					if (! strlen(curLine))
						continue; // Blank lines allowed =)

					// Find the colon
					int i;
					char varName[VARNAMEMAX];

					for (i = 0; i < VARNAMEMAX; i ++)
					{
						varName[i] = curLine[i];
						if (curLine[i] == ':')
						{
							varName[i] = '\0';
							break;
						}
					}

					if (i >= VARNAMEMAX)
					{
						error = 1;
						goto ErrorCheck;
					}

					if (! i)
					{
						error = 2;
						goto ErrorCheck;
					}

					// Remove all spaces after the colon
					int shiftBack = 0;
					for (i; curLine[i]; i ++)
					{
						while (curLine[i + shiftBack] == ' ')
							shiftBack ++;

						curLine[i] = curLine[i + shiftBack];
					}

					// Do a struct member scan
					INT32 offset;
					char dataType;
					int numItems = 1;
					int curLinePos = strlen(varName) + 1;

					for (int stage = 0; stage < 3; stage ++)
					{
						char getStr[256] = "";
						bool gotComma = false;
						for (i = 0; ; i ++)
						{
							if (curLine[curLinePos] == ',' || curLine[curLinePos] == '\0')
							{
								if (curLine[curLinePos] == ',')
								{
									gotComma = true;
									curLinePos ++; // Advance
								}
								getStr[i] = '\0';
								break;
							}

							getStr[i] = curLine[curLinePos ++];
						}

						if (! gotComma && stage < 1)
						{
							error = 4;
							goto ErrorCheck;
						}

						switch (stage)
						{
							case 0:
								// Get offset
								if (sscanf(getStr, "%X", &offset) != 1)
								{
									error = 6;
									goto ErrorCheck;
								}
								break;
							case 1:
								// Get type
								if (! strcmp(getStr, "byte"))
									dataType = DATATYPE_BYTE;
								else if (! strcmp(getStr, "half"))
									dataType = DATATYPE_HWORD;
								else if (! strcmp(getStr, "word"))
									dataType = DATATYPE_WORD;
								else if (! strcmp(getStr, "float"))
									dataType = DATATYPE_WORD;
								else
								{
									error = 7;
									goto ErrorCheck;
								}
								break;
							case 2:
								// Get num array items
								if (sscanf(getStr, "%X", &numItems) != 1)
								{
									error = 9;
									goto ErrorCheck;
								}
								break;
						}

						if (! gotComma)
							break;
					}

					// Scan was successful! Add this member to the struct
					newStruct->vars = (StructVar*) Realloc(newStruct->vars, sizeof (StructVar) * (newStruct->numVars + 1));
					strcpy(newStruct->vars[newStruct->numVars].name, varName);
					newStruct->vars[newStruct->numVars].dataType = dataType;
					newStruct->vars[newStruct->numVars].numItems = numItems;
					newStruct->vars[newStruct->numVars].offset = offset;

					newStruct->numVars ++;
					continue;
FullBreak:
					break;
				}

				// Do some last-minute errorchecking for the struct
				if (sscanf(sizeString, "%X", &newStruct->size) != 1)
					error = 3;

				if (newStruct->size <= 0)
				{
					error = 10;
					goto ErrorCheck;
				}

				for (int i = 0; i < newStruct->numVars; i ++)
				{
					StructVar* curVar = &newStruct->vars[i];

					if (curVar->offset < 0)
						error = 5;

					if (curVar->numItems < 0)
						error = 8;

					if (curVar->offset + datasizes[curVar->dataType] > newStruct->size)
						error = 11;

					if (error)
						goto ErrorCheck;

					for (int l = 0; l < newStruct->numVars; l ++)
					{
						if (l == i)
							continue;

						if (curVar->offset < newStruct->vars[l].offset && curVar->offset + datasizes[curVar->dataType] > newStruct->vars[l].offset)
						{
							error = 12;
							goto ErrorCheck;
						}
					}
				}

				// Finally, display errors if there are any
				ErrorCheck:
				if (error)
				{
					char errorMessage[256] = "";
					const char* standardErrors[] = {"", "", 
						"A blank variable was detected", "Struct size hasn't been specified!",  // 2, 3
						"An invalid definition was detected (format is: [name]:[offset],[type],[numitems (optional)]", // 4
						"A negative offset value was detected. Offsets must be positive and smaller than 80000000.", // 5
						"An invalid offset was detected.", // 6
						"An invalid type was detected (valid types are: byte, half, word, float)", // 7
						"A negative array size was used. Array sizes must be positive and smaller than 80000000.", // 8
						"An invalid array size was detected.", // 9
						"Struct's size is invalid! Must be more than 0", // 10
						"A struct variable is overlapping the struct's total size!", // 11
						"Two or more struct variables are overlapping!", // 12
					};
					switch (error)
					{
						case 1:
						{
							sprintf(errorMessage, "A variable name is too long (%i characters is the maximum), or a colon (:) was not found", VARNAMEMAX - 1);
							break;
						}
						default:
							strcpy(errorMessage, standardErrors[error]);
							break;
					}

					MessageBox(main_hwnd, errorMessage, "Error creating struct!", MB_ICONERROR);
					Free(newStruct->vars);
					Free(fullScript);
					break;
				}

				// No errors to display!
				// We've got the a-OK! Finalise the struct's creation!
				strcpy(newStruct->name, nameString);
				newStruct->script = fullScript;

				if (wndStruct.editingId == -1)
					numStructDefs ++;
				else
				{
					// Replace the old struct with this new one instead
					Free(structDefs[wndStruct.editingId].script);
					Free(structDefs[wndStruct.editingId].vars);

					memcpy(&structDefs[wndStruct.editingId], newStruct, sizeof (StructDef));
				}

				UpdateStructManagerWindow();

				DestroyWindow(hwnd);
				wndStruct.hwnd = NULL;
			}
			break;
		}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK StructManagerWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CLOSE:
			ShowStructManagerWindow(false);
			return 0;
		case WM_COMMAND:
		{
			if ((HWND) lParam == wndStructManager.btAdd)
				Do_CreateStruct(-1);
			else if ((HWND) lParam == wndStructManager.btEdit)
			{
				int structId = SendMessage(wndStructManager.lbStructs, LB_GETCURSEL, 0, 0);

				if (structId == -1)
					break;

				Do_CreateStruct(structId);
			}
			else if ((HWND) lParam == wndStructManager.btDelete)
			{
				int structId = SendMessage(wndStructManager.lbStructs, LB_GETCURSEL, 0, 0);
				
				if (structId < 0)
					break;

				if (MessageBox(main_hwnd, "Are you sure you want to delete this struct definition?\nAll instances of this struct will be removed from the list.", 
					"Ya sure dude?", MB_YESNO) == IDYES)
					RemoveStructDef(structId);

				UpdateStructManagerWindow();
			}
			else if ((HWND) lParam == wndStructManager.btOk)
				ShowStructManagerWindow(false);
			break;
		}
	}
	
	return DefWindowProc(hwnd, message, wParam, lParam);
}

void CreateBreakWindow()
{
	HINSTANCE inst = globalInst;
	wndbreak_t* b = &wndBreak;

	wndBreak.hwnd = CreateWindowEx(0, "BreakerClass", "Breakpoints", WS_THICKFRAME | WS_CHILD, CW_USEDEFAULT, CW_USEDEFAULT, 162, 318, main_hwnd, NULL, globalInst, 0);

	b->gbRegs = CreateWindowEx(0, "BUTTON", "Registers", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 3, 277, 138, 516, wndBreak.hwnd, NULL, inst, 0);
	b->gbCallStack = CreateWindowEx(0, "BUTTON", "Call Stack", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 3, 114, 141, 157, wndBreak.hwnd, NULL, inst, 0);

	b->edRegs1 = CreateWindowEx(0, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER, 54, 19, 80, 452, b->gbRegs, NULL, inst, 0);

	b->liCallStack = CreateWindowEx(0, "LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL, 3, 19, 135, 134, b->gbCallStack, NULL, inst, 0);

	b->btContinue = CreateWindowEx(0, "BUTTON", "Continue", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 4, 128, 23, wndBreak.hwnd, NULL, inst, 0);
	b->btStep = CreateWindowEx(0, "BUTTON", "Step", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 6, 42, 67, 23, wndBreak.hwnd, NULL, inst, 0);
	b->btStepOver = CreateWindowEx(0, "BUTTON", "Step Over", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 79, 42, 65, 23, wndBreak.hwnd, NULL, inst, 0);
	b->btAuto = CreateWindowEx(0, "BUTTON", "Auto-Continue", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 489, 222, 93, 17, wndBreak.hwnd, NULL, inst, 0);
	
	b->btReadBreak = CreateWindowEx(0, "BUTTON", "OnRead:", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 3, 70, 75, 17, wndBreak.hwnd, NULL, inst, 0);
	b->btWriteBreak = CreateWindowEx(0, "BUTTON", "OnWrite:", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 3, 91, 75, 17, wndBreak.hwnd, NULL, inst, 0);
	
	b->edReadBreak  = CreateWindowEx(0, "EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 83, 68, 61, 20, wndBreak.hwnd, NULL, inst, 0);
	b->edWriteBreak = CreateWindowEx(0, "EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 83, 89, 61, 20, wndBreak.hwnd, NULL, inst, 0);

	b->stStatus = CreateWindowEx(0, "STATIC", "Running", WS_VISIBLE | WS_CHILD | SS_CENTER, 11, 28, 128, 13, wndBreak.hwnd, NULL, inst, 0);
	
	SendMessage(b->gbCallStack, WM_SETFONT, (WPARAM) global_listfont, 0);
	wndBreak.procGbCallStack = GetWindowLongPtr(b->gbCallStack, GWLP_WNDPROC);
	SetWindowLongPtr(b->gbCallStack, GWLP_WNDPROC, (LONG_PTR) BreakWindowProc);
	wndBreak.procGbRegs = SetWindowLongPtr(b->edRegs1, GWLP_WNDPROC, (LONG_PTR) BreakWindowProc);

	for (int i = 0; i < 32; i ++)
	{
		b->stRegs[i] = CreateWindowEx(0, "STATIC", registers[i], WS_VISIBLE | WS_CHILD | SS_SIMPLE, 10, 18 + i * 14, 56, 16, b->gbRegs, NULL, inst, 0);

		SendMessage(b->stRegs[i], WM_SETFONT, (WPARAM) global_listfont, 0);
	}
	
	SendMessage(b->gbRegs, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(b->edRegs1, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(b->liCallStack, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(b->btContinue, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->btStep, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->btStepOver, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->btAuto, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->btReadBreak, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->btWriteBreak, WM_SETFONT, (WPARAM) global_mainfont, 0);
	SendMessage(b->edReadBreak, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(b->edWriteBreak, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(b->stStatus, WM_SETFONT, (WPARAM) global_mainfont, 0);
}

void GetBreakWindowText(char* out, int maxChars)
{
	SendMessage(wndBreak.edRegs1, WM_GETTEXT, maxChars, (LPARAM) out);
}

void SetBreakWindowText(const char* in)
{
	DWORD selStart, selEnd;
	int curSelLine = SendMessage(wndBreak.edRegs1, EM_LINEFROMCHAR, -1, 0);
	int curScroll = SendMessage(wndBreak.edRegs1, EM_GETFIRSTVISIBLELINE, 0, 0);

	SendMessage(wndBreak.edRegs1, WM_SETTEXT, 0, (LPARAM) in);

	SendMessage(wndBreak.edRegs1, EM_LINESCROLL, 0, curScroll);
	SendMessage(wndBreak.edRegs1, EM_SETSEL, SendMessage(wndBreak.edRegs1, EM_LINEINDEX, curSelLine, 0), SendMessage(wndBreak.edRegs1, EM_LINEINDEX, curSelLine, 0));
}

void AddBreakWindowCall(const char* in)
{
	SendMessage(wndBreak.liCallStack, LB_ADDSTRING, 0, (LPARAM) in);
}

void ClearBreakWindowCalls()
{
	SendMessage(wndBreak.liCallStack, LB_RESETCONTENT, 0, 0);
}

void SetBreakWindowStatus(const char* status)
{
	SendMessage(wndBreak.stStatus, WM_SETTEXT, 0, (LPARAM) status);
}

void ShowBreakWindow(bool dur)
{
	ShowWindow(wndBreak.hwnd, (wndBreak.isOpen = dur));
}

UINT32 _GetBreakWindowBreakpoint(HWND bt, HWND ed)
{
	char test[9];

	if (SendMessage(bt, BM_GETCHECK, 0, 0) != BST_CHECKED)
		return 0xFFFFFFFF;

	SendMessage(ed, WM_GETTEXT, 9, (LPARAM) test);
	test[8] = '\0'; // Safety first

	UINT32 breakAddr = 0;

	if (sscanf(test, "%X", &breakAddr) != 1 || breakAddr > 0x02000000)
		return 0xFFFFFFFF;

	return breakAddr & 0x03FFFFFF;
}

UINT32 GetBreakWindowReadBreakpoint()
{
	return _GetBreakWindowBreakpoint(wndBreak.btReadBreak, wndBreak.edReadBreak);
}

UINT32 GetBreakWindowWriteBreakpoint()
{
	return _GetBreakWindowBreakpoint(wndBreak.btWriteBreak, wndBreak.edWriteBreak);
}

void _SetBreakWindowBreakpoint(HWND bt, HWND ed, UINT32 address)
{
	char valString[9];

	if (address == 0xFFFFFFFF)
	{
		SendMessage(bt, BM_SETCHECK, BST_UNCHECKED, 0);
		return;
	}

	SendMessage(bt, BM_SETCHECK, BST_CHECKED, 0);

	sprintf(valString, "%08X", address & 0x0FFFFFFF);
	SendMessage(ed, WM_SETTEXT, 0, (LPARAM) valString);
}

void SetBreakWindowReadBreakpoint(UINT32 address)
{
	_SetBreakWindowBreakpoint(wndBreak.btReadBreak, wndBreak.edReadBreak, address);
}

void SetBreakWindowWriteBreakpoint(UINT32 address)
{
	_SetBreakWindowBreakpoint(wndBreak.btWriteBreak, wndBreak.edWriteBreak, address);
}

void UpdateBreakWindowRegs()
{
	RECT regsBox;
	GetClientRect(wndBreak.edRegs1, &regsBox);

	int startLine = SendMessage(wndBreak.edRegs1, EM_GETFIRSTVISIBLELINE, 0, 0);
	int endLine = startLine + regsBox.bottom / 14 + 1;
	if (startLine > 32) startLine = 32;
	if (endLine > 32) endLine = 32;

	for (int i = 0; i < 32; i ++)
	{
		if (i < startLine || i >= endLine)
			ShowWindow(wndBreak.stRegs[i], false);
		else
		{
			ShowWindow(wndBreak.stRegs[i], true);
			SetWindowPos(wndBreak.stRegs[i], NULL, 10, 18 + (i - startLine) * 14, 0, 0, SWP_NOSIZE);
		}
	}

	if (IsWindowVisible(wndBreak.hwnd))
		RedrawWindow(wndBreak.hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

void CreateGameSharkWindow()
{
	#define CREATE(cls, text, style, x, y, w, h, parent) CreateWindowEx(0, cls, text, (style) | WS_VISIBLE | WS_CHILD, x, y, w, h, parent, NULL, globalInst, 0)
	wndgameshark_t* gs = &wndGameShark;
	
	gs->hwnd = CreateWindowEx(0, "GameSharkClass", "GameShark Tools", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 572, 374, main_hwnd, NULL, globalInst, 0);

	gs->gbActiveCodes = CREATE("BUTTON", "Active Codes", WS_CHILD | BS_GROUPBOX, 12, 12, 327, 322, gs->hwnd);
	gs->gbCodeScanner = CREATE("BUTTON", "Code Scanner", WS_CHILD | BS_GROUPBOX, 345, 12, 207 , 322, gs->hwnd);
	
	gs->procGbActiveCodes = SetWindowLongPtr(gs->gbActiveCodes, GWLP_WNDPROC, (LONG) GameSharkWindowProc);
	gs->procGbCodeScanner = SetWindowLongPtr(gs->gbCodeScanner, GWLP_WNDPROC, (LONG) GameSharkWindowProc);

	gs->edCodes = CREATE("EDIT", "", WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL, 7, 22, 314, 267, gs->gbActiveCodes);

	//gs->btOk = CREATE("BUTTON", "OK", WS_CHILD | BS_PUSHBUTTON, 163, 293, 75, 23, gs->gbActiveCodes);
	//gs->btCancel = CREATE("BUTTON", "Cancel", WS_CHILD | BS_PUSHBUTTON, 244, 293, 75, 23, gs->gbActiveCodes);
	gs->btApply = CREATE("BUTTON", "Update", WS_CHILD | BS_PUSHBUTTON, 7, 293, 314, 23, gs->gbActiveCodes);

	gs->edSearch = CREATE("EDIT", "", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 42, 19, 108, 20, gs->gbCodeScanner);
	gs->btHex = CREATE("BUTTON", "Hex", WS_CHILD | BS_AUTOCHECKBOX, 156, 21, 45, 17, gs->gbCodeScanner);

	gs->cbScanMode = CREATE(WC_COMBOBOX, "", CBS_DROPDOWNLIST, 93, 71, 105, 21, gs->gbCodeScanner);
	gs->cbDataType = CREATE(WC_COMBOBOX, "", CBS_DROPDOWNLIST, 93, 95, 105, 21, gs->gbCodeScanner);

	gs->btScan = CREATE("BUTTON", "Scan", WS_CHILD | BS_PUSHBUTTON, 25, 123, 75, 23, gs->gbCodeScanner);
	gs->btReset = CREATE("BUTTON", "Reset", WS_CHILD | WS_DISABLED | BS_PUSHBUTTON, 106, 123, 75, 23, gs->gbCodeScanner);

	gs->lbResults = CREATE(WC_LISTBOX, "No results", WS_CHILD | WS_BORDER | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL, 6, 152, 195, 160, gs->gbCodeScanner);

	CREATE("STATIC", "Find:", WS_CHILD | SS_SIMPLE, 6, 22, 30, 13, gs->gbCodeScanner);
	CREATE("STATIC", "Scan Mode:", WS_CHILD | SS_SIMPLE, 3, 74, 65, 13, gs->gbCodeScanner);
	CREATE("STATIC", "Scan Type:", WS_CHILD | SS_SIMPLE, 3, 98, 60, 13, gs->gbCodeScanner);

	// Add combobox items
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Equal To");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Greater Than");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Less Than");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Changed");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Unchanged");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Increased");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Decreased");
	SendMessage(gs->cbScanMode, CB_ADDSTRING, 0, (LPARAM) "Unknown");

	SendMessage(gs->cbDataType, CB_ADDSTRING, 0, (LPARAM) "1 byte");
	SendMessage(gs->cbDataType, CB_ADDSTRING, 0, (LPARAM) "2 bytes");
	SendMessage(gs->cbDataType, CB_ADDSTRING, 0, (LPARAM) "4 bytes");
	SendMessage(gs->cbDataType, CB_ADDSTRING, 0, (LPARAM) "Float");
	SendMessage(gs->cbDataType, CB_ADDSTRING, 0, (LPARAM) "String");
	
	SendMessage(gs->cbScanMode, CB_SETCURSEL, 0, 0);
	SendMessage(gs->cbDataType, CB_SETCURSEL, 2, 0);

	// Create dropdown menu for listbox
	gs->dropMenu = CreatePopupMenu();
	
	AppendMenu(gs->dropMenu, MF_ENABLED | MF_STRING, 100, "Freeze");
	//AppendMenu(gs->dropMenu, MF_ENABLED | MF_STRING, 101, "Discard");

	// Set fonts
	EnumChildWindows(gs->hwnd, SetChildFonts, (LPARAM) global_mainfont);

	SendMessage(gs->lbResults, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(gs->edCodes, WM_SETFONT, (WPARAM) global_notepadfont, 0);
}

//#define CREATE(cls, text, style, x, y, w, h, parent) CreateWindowEx(0, cls, text, (style) | WS_VISIBLE | WS_CHILD, x, y, w, h, parent, NULL, globalInst, 0)
void CreateStructManagerWindow()
{
	wndstructmanager_t* sm = &wndStructManager;

	sm->hwnd = CreateWindowEx(0, "StructManagerClass", "Structs...", WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_SYSMENU), 
		CW_USEDEFAULT, CW_USEDEFAULT, 232, 238, main_hwnd, NULL, globalInst, 0);
	
	sm->lbStructs = CREATE("LISTBOX", "", LBS_HASSTRINGS | WS_BORDER | WS_VSCROLL, 13, 13, 111, 173, sm->hwnd);

	sm->btAdd = CREATE("BUTTON", "Add...", BS_PUSHBUTTON, 130, 13, 75, 23, sm->hwnd);
	sm->btEdit = CREATE("BUTTON", "Edit...", BS_PUSHBUTTON, 130, 42, 75, 23, sm->hwnd);
	sm->btDelete = CREATE("BUTTON", "Delete", BS_PUSHBUTTON, 130, 71, 75, 23, sm->hwnd);

	sm->btOk = CREATE("BUTTON", "OK", BS_PUSHBUTTON, 130, 162, 75, 23, sm->hwnd);

	EnumChildWindows(sm->hwnd, SetChildFonts, (LPARAM) global_mainfont);
}

void ShowStructManagerWindow(bool show)
{
	ShowWindow(wndStructManager.hwnd, show);
}

void UpdateStructManagerWindow()
{
	SendMessage(wndStructManager.lbStructs, LB_RESETCONTENT, 0, 0);

	for (int i = 0; i < numStructDefs; i ++)
		SendMessage(wndStructManager.lbStructs, LB_ADDSTRING, 0, (LPARAM) structDefs[i].name);
}

void UpdateTextbox()
{
	DWORD sel1, sel2;
	char string[512];
	UINT32 addr = GetSelAddress(list.sel);
	int updateAmount = 512;

	if (addr + 512 >= memlen8)
		updateAmount = memlen8 - addr;

	UpdateMemoryRegion(addr, updateAmount);

	for (int i = 0; i < updateAmount; i ++)
	{
		char memchar = mem8[addr + i];

		if (memchar == '\0')
			string[i] = '.';
		else
		{
			string[i] = memchar;

			// TEMP - Okami translation
			/*if (memchar >= 0x31 && memchar < 0x31 + 26)
				string[i] = 'a' + memchar - 0x31;
			if (memchar >= 0x17 && memchar < 0x31)
				string[i] = 'A' + memchar - 0x17;*/
		}

		if (addr + i + 1 >= memlen8)
		{
			string[i] = '\0';
			break;
		}
	}

	if (updateAmount > 0)
		string[updateAmount - 1] = '\0';

	SendMessage(textbox_hwnd, EM_GETSEL, (WPARAM) &sel1, (LPARAM) &sel2);
	SendMessage(textbox_hwnd, WM_SETTEXT, 0, (LPARAM) string);
	SendMessage(textbox_hwnd, EM_SETSEL, sel1, sel2);
}

void DrawListItem(DRAWITEMSTRUCT* item, const char* text, int x, int y, int maxChars)
{
	wchar_t str[256];

	mbstowcs(str, text, strlen(text));
	str[strlen(text)] = '\0';

	int lastMark = 0;
	int curX = x;
	COLORREF preserveColor = GetTextColor(item->hDC);
	SelectObject(item->hDC, global_listfont);

	while (str[lastMark] != '\0')
	{
		for (int i = lastMark; i < 256; i ++)
		{
			wchar_t* chr = &str[i];
			SIZE rect;
			COLORREF changeColor = 0xFFFFFFFF;
			int changeBold = -1;

			switch (*chr)
			{
				case 0x01: changeColor = preserveColor; break;
				case 0x02: changeColor = RGB(255, 0, 0); break;
				case 0x03: changeColor = RGB(0, 176, 0); break;
				case 0x04: changeColor = RGB(0, 0, 176); break;
				case 0x05: changeColor = RGB(128+30, 90, 90); break;
				case 0xB1: changeBold = 1; break;
				case 0xB0: changeBold = 0; break;
				case 0xFF: *chr = L'▼'; break;
				case 0xFE: *chr = L'▲'; break;
				case 0xFD: *chr = L'◄'; break;
				case 0xFC: *chr = L'↓'; break;
				case 0xFB: *chr = L'↑'; break;
				case 0xFA: *chr = L'→'; break;
				case 0xF9: *chr = L'┴'; break;
				default:
					if (*chr != '\0')
						continue; // Continue until a special character or ending is reached
			}

			// Draw the actual text!
			TextOutW(item->hDC, curX, y, &str[lastMark], i - lastMark);

			GetTextExtentPointW(item->hDC, str, i - lastMark, &rect);
			
			curX += rect.cx;
			lastMark = i;

			if (changeColor != 0xFFFFFFFF)
			{
				if (item->itemID != list.sel)
					SetTextColor(item->hDC, changeColor);

				lastMark ++;
			}

			if (changeBold == 1)
			{
				SelectObject(item->hDC, global_boldlistfont);
				lastMark ++;
			}
			else if (changeBold == 0)
			{
				SelectObject(item->hDC, global_listfont);
				lastMark ++;
			}

			if (*chr == '\0')
				break;
		}
	}
	
	SetTextColor(item->hDC, preserveColor);
}

bool Do_GoToAddress()
{
	currentDialog = ID_DIALOG_GOTOADDR;

	bool success = DialogBox(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_GOTOADDR, 0)), main_hwnd, DialogProc);

	return success;
}

bool Do_GoToLabel()
{
	currentDialog = ID_DIALOG_GOTOLABEL;

	bool success = DialogBox(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_GOTOLABEL, 0)), main_hwnd, DialogProc);

	return success;
}

bool Do_Find()
{
	currentDialog = ID_DIALOG_FIND;

	bool success = DialogBox(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_FIND, 0)), main_hwnd, DialogProc);

	return success;
}

bool Do_EditRegs()
{
	currentDialog = ID_DIALOG_EDITREGS;

	bool success = DialogBox(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_EDITREGS, 0)), main_hwnd, DialogProc);

	return success;
}

bool Do_SetDataTypes()
{
	currentDialog = ID_DIALOG_SETDATATYPES;

	bool success = DialogBox(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_SETDATATYPES, 0)), main_hwnd, DialogProc);

	return success;
}

bool Do_PlaceStruct()
{
	if (! numStructDefs)
	{
		MessageBox(main_hwnd, "You haven't defined any structs yet!", "They need to exist first", MB_ICONERROR);
		return false;
	}

	bool foundOverlap = false;
	UINT32 selAddr = GetSelAddress(-1);
	for (int i = 0; i < numStructInsts; i ++)
	{
		if (selAddr >= structInsts[i].address && selAddr < structInsts[i].address + structDefs[structInsts[i].structDefId].size)
			foundOverlap = true;
	}

	/*if (foundOverlap)
	{
		MessageBox(main_hwnd, "There is already a struct occupying this address!", "Not sharing is not caring!", MB_ICONERROR);
		return false;
	}*/ // Ah, what the heck, let them get away with it.

	currentDialog = ID_DIALOG_PLACESTRUCT;
	
	bool success = DialogBox(globalInst, MAKEINTRESOURCE(ID_DIALOG_PLACESTRUCT), main_hwnd, DialogProc);

	return success;
}

void Do_CreateStruct(int editingId)
{
	if (wndStruct.hwnd)
		return; // Window is already open
	const char info[] = "Use this form to create data structures with labelled variables that can be marked anywhere on the main list!\x0D\x0A"
		"Each command in this script must be on its own line.\x0D\x0A"
		"The following format is used to create each struct member:\x0D\x0A"
		"[name/label]: [byte offset], [type], [number of items (optional) (for arrays)]\x0D\x0AAll numbers are hexadecimal.\x0D\x0A"
		"Valid types are: byte, half, word, float\x0D\x0A\x0D\x0A"
		"Example:\x0D\x0A"
		"ASingleVariable: 0, word\x0D\x0A"
		"AnotherVariable: 4, word\x0D\x0A"
		"AnArrayOfHalfwords: 8, half, 5\x0D\x0A";

	wndStruct.hwnd = CreateWindowEx(0, "StructClass", "Create New Struct", WS_VISIBLE | WS_OVERLAPPEDWINDOW & ~(WS_SYSMENU), 
		CW_USEDEFAULT, CW_USEDEFAULT, 389, 362, main_hwnd, NULL, globalInst, 0);

	wndStruct.edScript = CreateWindowEx(0, "EDIT", info, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE, 13, 35, 348, 248, wndStruct.hwnd, NULL, globalInst, 0);

	wndStruct.edName = CreateWindowEx(0, "EDIT", "New struct", WS_VISIBLE | WS_CHILD | WS_BORDER, 89, 9, 100, 20, wndStruct.hwnd, NULL, globalInst, 0);
	wndStruct.edSize = CreateWindowEx(0, "EDIT", "0", WS_VISIBLE | WS_CHILD | WS_BORDER, 262, 9, 32, 20, wndStruct.hwnd, NULL, globalInst, 0);

	wndStruct.btOk = CreateWindowEx(0, "BUTTON", "OK", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 13, 290, 75, 23, wndStruct.hwnd, NULL, globalInst, 0);
	wndStruct.btCancel = CreateWindowEx(0, "BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 94, 290, 75, 23, wndStruct.hwnd, NULL, globalInst, 0);
	
	CreateWindowEx(0, "STATIC", "Struct Name:", WS_VISIBLE | WS_CHILD, 14, 12, 69, 13, wndStruct.hwnd, NULL, globalInst, 0);
	CreateWindowEx(0, "STATIC", "Struct Size:", WS_VISIBLE | WS_CHILD, 195, 12, 69, 13, wndStruct.hwnd, NULL, globalInst, 0);

	EnumChildWindows(wndStruct.hwnd, SetChildFonts, (LPARAM) global_mainfont);

	wndStruct.editingId = editingId;

	if (editingId != -1)
	{
		// Fill the window with the appropriate info
		char size[9];
		sprintf(size, "%X", structDefs[editingId].size);

		SendMessage(wndStruct.edName, WM_SETTEXT, 0, (LPARAM) structDefs[editingId].name);
		SendMessage(wndStruct.edSize, WM_SETTEXT, 0, (LPARAM) size);
		SendMessage(wndStruct.edScript, WM_SETTEXT, 0, (LPARAM) structDefs[editingId].script);
	}
}

BOOL CALLBACK SetChildFonts(HWND hwnd, LPARAM lParam)
{
	SendMessage(hwnd, WM_SETFONT, (WPARAM) lParam, 0);
	return true;
}