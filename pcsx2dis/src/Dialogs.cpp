#include "Main.h"
#include "MainList.h"
#include "Windows.h"
#include "Resources.h"
#include "Analyse.h" // FindPattern
#include "Dialogs.h"
#include "Processor.h" // CodeToASM

#include <CommCtrl.h>
#include <cstdio>

dlggoto_t dlgGoto;
dlgfind_t dlgFind;
dlglabels_t dlgLabels;
dlgprogress_t dlgProgress;
dlgplacestruct_t dlgPlaceStruct;

int currentDialog;

BOOL CALLBACK ConvertDlgCoords(HWND hwnd, LPARAM lParam);

INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (currentDialog == ID_DIALOG_GOTOADDR)
	{
		switch (message)
		{
			case WM_INITDIALOG:
			{
				char addr[9];
					
				sprintf(addr, "%08X", dlgGoto.address);

				SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOADDR), WM_SETTEXT, 0, (WPARAM) addr);
				SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOADDR), EM_SETLIMITTEXT, 8, 0);
				SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOADDR), EM_SETSEL, 0, -1);

				SetFocus(GetDlgItem(hwnd, ID_EDIT_GOTOADDR));
			}
			break;
			case WM_COMMAND:
			{
				if (wParam == IDOK)
				{
					unsigned int tempAddr;
					char addressText[9];

					SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOADDR), WM_GETTEXT, 9, (LPARAM) addressText);

					if (sscanf(addressText, "%X", &tempAddr) == 1 && strlen(addressText))
					{
						dlgGoto.address = tempAddr;
						Goto(dlgGoto.address);
						
						EndDialog(hwnd, true);
					}
					else
						EndDialog(hwnd, false);
				}
			}
			break;
			case WM_CLOSE:
				EndDialog(hwnd, 0);
			break;
		}
	}
	else if (currentDialog == ID_DIALOG_GOTOLABEL)
	{
		HWND labelList = GetDlgItem(hwnd, ID_LISTBOX_GOTOLABEL);
		switch (message)
		{
			case WM_INITDIALOG:
			{
				RECT mainrect;
				TCITEM tc;

				dlgLabels.hwnd = hwnd;

				GetWindowRect(hwnd, &mainrect);

				SetWindowPos(hwnd, HWND_TOP, 
					mainrect.left, 
					mainrect.top, 
					(mainrect.right - mainrect.left - 3) * 2 / 3, 
					(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
					0);

				EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);

				tc.mask = TCIF_TEXT;
				tc.pszText = "All";
				SendMessage(GetDlgItem(hwnd, ID_TAB_GOTOLABEL), TCM_INSERTITEM, 0, (LPARAM) &tc);
				tc.pszText = "Auto";
				SendMessage(GetDlgItem(hwnd, ID_TAB_GOTOLABEL), TCM_INSERTITEM, 1, (LPARAM) &tc);
				tc.pszText = "User";
				SendMessage(GetDlgItem(hwnd, ID_TAB_GOTOLABEL), TCM_INSERTITEM, 2, (LPARAM) &tc);

				SendMessage(GetDlgItem(hwnd, ID_TAB_GOTOLABEL), TCM_SETCURSEL, (WPARAM) dlgLabels.curCategory, 0);

				SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOLABEL), WM_SETTEXT, 0, (LPARAM) dlgLabels.text);
				SendMessage(GetDlgItem(hwnd, ID_EDIT_GOTOLABEL), EM_SETSEL, 0, -1);

				if (dlgLabels.curSort == 0)
					SendMessage(GetDlgItem(hwnd, ID_RADIO1_GOTOLABEL), BM_SETCHECK, BST_CHECKED, 0);
				else
					SendMessage(GetDlgItem(hwnd, ID_RADIO2_GOTOLABEL), BM_SETCHECK, BST_CHECKED, 0);

				SetFocus(GetDlgItem(hwnd, ID_EDIT_GOTOLABEL));

				UpdateLabelDialogLabels();
			}
			break;
			case WM_COMMAND:
			{
				if ((LOWORD(wParam) == ID_LISTBOX_GOTOLABEL && HIWORD(wParam) == LBN_DBLCLK) || wParam == IDOK)
				{
					int selection = SendMessage(GetDlgItem(hwnd, ID_LISTBOX_GOTOLABEL), LB_GETCURSEL, 0, 0);

					if (selection == -1) // What?!
						break;

					Goto(labels[(int) SendMessage(GetDlgItem(hwnd, ID_LISTBOX_GOTOLABEL), LB_GETITEMDATA, selection, 0)].address);

					EndDialog(hwnd, 1);
				}
				else if (HIWORD(wParam) == EN_CHANGE)
					SearchLabelDialog(false);
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == ID_BUTTON_GOTOLABEL)
					SearchLabelDialog(true);
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == ID_RADIO1_GOTOLABEL)
				{
					dlgLabels.curSort = 0;
					UpdateLabelDialogLabels();
				}
				else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == ID_RADIO2_GOTOLABEL)
				{
					dlgLabels.curSort = 1;
					UpdateLabelDialogLabels();
				}
			}
			break;
			case WM_NOTIFY:
			{
				NMHDR* err = (NMHDR*) lParam;

				if (err->code == TCN_SELCHANGE)
				{
					dlgLabels.curCategory = SendMessage(err->hwndFrom, TCM_GETCURSEL, 0, 0);

					UpdateLabelDialogLabels();
				}
			}
			break;
			case WM_CLOSE:
				EndDialog(hwnd, 0);
			break;
		}
	}
	else if (currentDialog == ID_DIALOG_FIND)
	{
		switch (message)
		{
			case WM_INITDIALOG:
			{
				RECT mainrect;

				GetWindowRect(hwnd, &mainrect);

				SetWindowPos(hwnd, HWND_TOP, 
					mainrect.left, 
					mainrect.top, 
					(mainrect.right - mainrect.left - 3) * 2 / 3, 
					(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
					0);

				EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);
				
				SendMessage(GetDlgItem(hwnd, 10), WM_SETTEXT, 0, (LPARAM) dlgFind.text);
				SendMessage(GetDlgItem(hwnd, 10), EM_SETSEL, 0, -1);
				SendMessage(GetDlgItem(hwnd, 11 + dlgFind.type), BM_SETCHECK, BST_CHECKED, 0);

				SetFocus(GetDlgItem(hwnd, 10));

				break;
			}
			case WM_COMMAND:
			{
				if (wParam == IDOK || wParam == IDCANCEL)
					EndDialog(hwnd, 0);

				if (wParam == IDOK)
				{
					SendMessage(GetDlgItem(hwnd, 10), WM_GETTEXT, 256, (LPARAM) dlgFind.text);
					dlgFind.text[255] = '\0';

					FindPattern(dlgFind.text, dlgFind.type); 
				}

				else if (wParam >= 11 && wParam <= 17)
				{
					for (int i = 11; i <= 17; i ++)
						SendMessage(GetDlgItem(hwnd, i), BM_SETCHECK, BST_UNCHECKED, 0);

					SendMessage(GetDlgItem(hwnd, wParam), BM_SETCHECK, BST_CHECKED, 0);
					dlgFind.type = wParam - 11;
				}

				break;
			}
			case WM_CLOSE:
				EndDialog(hwnd, 0);
				break;
		}
	}
	else if (currentDialog == ID_DIALOG_EDITREGS)
	{
		switch (message)
		{
			case WM_INITDIALOG:
			{
				RECT mainrect;

				GetWindowRect(hwnd, &mainrect);

				SetWindowPos(hwnd, HWND_TOP, 
					mainrect.left, 
					mainrect.top, 
					(mainrect.right - mainrect.left - 3) * 2 / 3, 
					(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
					0);

				EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);

				// Add register names (statics)
				HWND gbRegValues = GetDlgItem(hwnd, 10), gbRegNames = GetDlgItem(hwnd, 11);

				for (int i = 0; i < 32; i ++)
				{
					HWND temp1 = CreateWindowEx(0, "STATIC", registers[i], WS_VISIBLE | SS_RIGHT | WS_CHILD, 10 + 100 * (i / 16), 22 + (i % 16) * 14, 30, 16, gbRegValues, NULL, globalInst, 0);
					HWND temp2 = CreateWindowEx(0, "STATIC", registers[i], WS_VISIBLE | SS_RIGHT | WS_CHILD, 10 + 100 * (i / 16), 22 + (i % 16) * 14, 30, 16, gbRegNames, NULL, globalInst, 0);
					
					SendMessage(temp1, WM_SETFONT, (WPARAM) global_listfont, 0);
					SendMessage(temp2, WM_SETFONT, (WPARAM) global_listfont, 0);
				}

				// If a reg override is found, fill the register boxes
				char regValues[32][9];
				char regNames[32][17];

				memset(regValues, 0, sizeof (regValues));
				memset(regNames, 0, sizeof (regNames));

				for (int i = 0; i < numRegOverrides; i ++)
				{
					if (regOverrides[i].address != GetSelAddress(list.sel))
						continue;

					RegOverride* regSet = &regOverrides[i];

					for (int j = 0; j < regSet->numRegSetters; j ++)
						sprintf(regValues[regSet->regSetters[j].reg], "%08X", regSet->regSetters[j].value);

					for (int j = 0; j < regSet->numRegNamers; j ++)
						strcpy(regNames[regSet->regNamers[j].reg], regSet->regNamers[j].name);
				}

				char totalRegString1[16 * 17 + 1] = "", totalRegString2[16 * 17 + 1] = "";
				char totalRegNameString1[16 * 17 + 1] = "", totalRegNameString2[16 * 17 + 1] = "";

				for (int i = 0; i < 16; i ++)
				{
					strcat(totalRegString1, regValues[i]);
					strcat(totalRegString2, regValues[i + 16]);
					strcat(totalRegNameString1, regNames[i]);
					strcat(totalRegNameString2, regNames[i + 16]);

					if (i < 15)
					{
						strcat(totalRegString1, "\x0D\x0A");
						strcat(totalRegString2, "\x0D\x0A");
						strcat(totalRegNameString1, "\x0D\x0A");
						strcat(totalRegNameString2, "\x0D\x0A");
					}
				}

				SendMessage(GetDlgItem(hwnd, 12), WM_SETTEXT, 0, (LPARAM) totalRegString1);
				SendMessage(GetDlgItem(hwnd, 13), WM_SETTEXT, 0, (LPARAM) totalRegString2);
				SendMessage(GetDlgItem(hwnd, 14), WM_SETTEXT, 0, (LPARAM) totalRegNameString1);
				SendMessage(GetDlgItem(hwnd, 15), WM_SETTEXT, 0, (LPARAM) totalRegNameString2);


				// Set fonts
				SendMessage(GetDlgItem(hwnd, 12), WM_SETFONT, (WPARAM) global_listfont, 0);
				SendMessage(GetDlgItem(hwnd, 13), WM_SETFONT, (WPARAM) global_listfont, 0);
				SendMessage(GetDlgItem(hwnd, 14), WM_SETFONT, (WPARAM) global_listfont, 0);
				SendMessage(GetDlgItem(hwnd, 15), WM_SETFONT, (WPARAM) global_listfont, 0);

				break;
			}
			case WM_COMMAND:
				if (wParam == IDOK)
				{
					char regValues[(8 + 2) * 32 + 1];
					char* regNames;
					int curChar = 0;
					char curLine[sizeof (regValues)];

					// Find the current reg override or make a new one
					RegOverride* curOverride = NULL;

					for (int i = 0; i < numRegOverrides; i ++)
					{
						if (regOverrides[i].address == GetSelAddress(list.sel))
						{
							curOverride = &regOverrides[i];

							// Reset this reg override
							Free(curOverride->regSetters);
							Free(curOverride->regNamers);
							curOverride->numRegNamers = curOverride->numRegSetters = 0;
							curOverride->regNamers = NULL;
							curOverride->regSetters = NULL;
						}
					}

					if (! curOverride)
						curOverride = AddRegOverride(GetSelAddress(list.sel));

					// Get the strings from the register values
					SendMessage(GetDlgItem(hwnd, 12), WM_GETTEXT, sizeof (regValues), (LPARAM) regValues);
					strcat(regValues, "\x0D\x0A");
					SendMessage(GetDlgItem(hwnd, 13), WM_GETTEXT, sizeof (regValues) - strlen(regValues) - 1, (LPARAM) &regValues[strlen(regValues)]);

					// Get register values
					for (int i = 0; i < 32; i ++)
					{
						int curLineLen = 0;

						// Get line
						while (regValues[curChar] != '\x0D' && regValues[curChar] != '\0')
							curLine[curLineLen ++] = regValues[curChar ++];

						if (curLineLen)
						{
							// Get value from line
							UINT32 value;
							curLine[curLineLen] = '\0';

							if (sscanf(curLine, "%X", &value) == 1)
								AddRegSetter(curOverride, i, value);
						}

						// Get name
						curLineLen = 0;

						if (regValues[curChar] != '\0')
							curChar += 2; // Skip past the new line
						else
							break;
					}

					// Get register names
					int part1Len = SendMessage(GetDlgItem(hwnd, 14), WM_GETTEXTLENGTH, 0, 0), part2Len = SendMessage(GetDlgItem(hwnd, 15), WM_GETTEXTLENGTH, 0, 0);
					regNames = (char*) Alloc((int) part1Len + part2Len + 3);

					SendMessage(GetDlgItem(hwnd, 14), WM_GETTEXT, part1Len + 1, (LPARAM) regNames);
					strcat(regNames, "\x0D\x0A");
					SendMessage(GetDlgItem(hwnd, 15), WM_GETTEXT, part2Len + 1, (LPARAM) &regNames[strlen(regNames)]);

					curChar = 0;
					for (int i = 0; i < 32; i ++)
					{
						int curLineLen = 0;

						// Get line
						while (regNames[curChar] != '\x0D' && regNames[curChar] != '\0')
							curLine[curLineLen ++] = regNames[curChar ++];

						if (curLineLen && curLineLen < 17)
						{
							// Get name from line
							char name[17];

							strncpy(name, curLine, curLineLen);
							name[curLineLen] = '\0';

							AddRegNamer(curOverride, i, name);
						}

						curLineLen = 0;

						if (regNames[curChar] != '\0')
							curChar += 2; // Skip past the new line
						else
							break;
					}

					// Delete the override if it's actually been cleared
					if (! curOverride->numRegSetters && ! curOverride->numRegNamers)
					{
						numRegOverrides --;

						for (int i = (int) (curOverride - regOverrides);  i < numRegOverrides; i ++)
							regOverrides[i] = regOverrides[i + 1];

						if (numRegOverrides)
							regOverrides = (RegOverride*) Realloc(regOverrides, numRegOverrides * sizeof (RegOverride));
						else
						{
							Free(regOverrides);
							regOverrides = NULL;
						}
					}
					
					Free(regNames);
					Free(regValues);

					AnalyseVisible(); // Changes may have been made!

					EndDialog(hwnd, 0);
				}
				break;
			case WM_CLOSE:
				EndDialog(hwnd, 0);
				break;
		}

		//return DefDlgProc(hwnd, message, wParam, lParam);
	}
	else if (currentDialog == ID_DIALOG_SETDATATYPES)
	{
		switch (message)
		{
			case WM_INITDIALOG:
			{
				RECT mainrect;

				GetWindowRect(hwnd, &mainrect);

				SetWindowPos(hwnd, HWND_TOP, 
					mainrect.left, 
					mainrect.top, 
					(mainrect.right - mainrect.left - 3) * 2 / 3, 
					(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
					0);

				EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);
				
				char addrString[9];

				sprintf(addrString, "%08X", GetSelAddress(-1));

				SendMessage(GetDlgItem(hwnd, 10), WM_SETTEXT, 0, (LPARAM) addrString);
				SendMessage(GetDlgItem(hwnd, 11), WM_SETTEXT, 0, (LPARAM) "4");
				SendMessage(GetDlgItem(hwnd, 12), CB_ADDSTRING, 0, (LPARAM) "Code");
				SendMessage(GetDlgItem(hwnd, 12), CB_ADDSTRING, 0, (LPARAM) "Byte");
				SendMessage(GetDlgItem(hwnd, 12), CB_ADDSTRING, 0, (LPARAM) "Half");
				SendMessage(GetDlgItem(hwnd, 12), CB_ADDSTRING, 0, (LPARAM) "Word");
				SendMessage(GetDlgItem(hwnd, 12), CB_ADDSTRING, 0, (LPARAM) "Float");
				SendMessage(GetDlgItem(hwnd, 12), CB_SETCURSEL, 3, 0);

				break;
			}
			case WM_COMMAND:
			{
				if (wParam == IDOK)
				{
					int dataType = SendMessage(GetDlgItem(hwnd, 12), CB_GETCURSEL, 0, 0);
					const char dataTypeTable[] = {DATATYPE_CODE, DATATYPE_BYTE, DATATYPE_HWORD, DATATYPE_WORD, DATATYPE_FLOAT};
					char setDataType = dataTypeTable[dataType];
					
					// Get the start address and size
					char scanStr[10] = {0,0,0,0,0,0,0,0,0,0};
					UINT32 startAddr;
					INT32 size;

					SendMessage(GetDlgItem(hwnd, 10), WM_GETTEXT, 9, (LPARAM) scanStr);
					if (sscanf(scanStr, "%08X", &startAddr) != 1)
					{
						MessageBox(main_hwnd, "Invalid start address!", "Error", MB_OK | MB_ICONERROR);
						break;
					}

					SendMessage(GetDlgItem(hwnd, 11), WM_GETTEXT, 9, (LPARAM) scanStr);
					if (sscanf(scanStr, "%08X", &size) != 1)
					{
						MessageBox(main_hwnd, "Invalid size!", "Error", MB_OK | MB_ICONERROR);
						break;
					}

					// Check for further errors
					if (startAddr < 0 || startAddr > memlen8)
					{
						MessageBox(main_hwnd, "Start address is out of range!", "Error on the Range", MB_OK | MB_ICONERROR);
						break;
					}

					if (startAddr + size > memlen8)
						size = memlen8 - startAddr;

					// Finally, set the data types
					for (int i = startAddr / 4; i < (startAddr + size) / 4; i ++)
						lines[i].datatype = setDataType;

					EndDialog(hwnd, 0);
					break;
				}

				if (wParam == IDCANCEL)
					EndDialog(hwnd, 0);

				break;
			}
			case WM_CLOSE:
				EndDialog(hwnd, 0);
				break;
		}
	}
	else if (currentDialog == ID_DIALOG_PLACESTRUCT)
	{
		switch (message)
		{
			case WM_INITDIALOG:
			{
				RECT mainrect;

				GetWindowRect(hwnd, &mainrect);

				SetWindowPos(hwnd, HWND_TOP, 
					mainrect.left, 
					mainrect.top, 
					(mainrect.right - mainrect.left - 3) * 2 / 3, 
					(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
					0);

				EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);
				
				if (dlgPlaceStruct.structDefId < 0 || dlgPlaceStruct.structDefId >= numStructDefs)
					dlgPlaceStruct.structDefId = 0;

				char number[15];
				sprintf(number, "%i", dlgPlaceStruct.numItems);
				SendMessage(GetDlgItem(hwnd, 11), WM_SETTEXT, 0, (LPARAM) number);

				sprintf(number, "%08X", GetSelAddress(-1) + dlgPlaceStruct.numItems * structDefs[dlgPlaceStruct.structDefId].size);
				SendMessage(GetDlgItem(hwnd, 12), WM_SETTEXT, 0, (LPARAM) number);

				for (int i = 0; i < numStructDefs; i ++)
					SendMessage(GetDlgItem(hwnd, 10), CB_ADDSTRING, 0, (LPARAM) structDefs[i].name);

				SendMessage(GetDlgItem(hwnd, 10), CB_SETCURSEL, dlgPlaceStruct.structDefId, 0);

				break;
			}
			case WM_COMMAND:
			{
				if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == 10)
					dlgPlaceStruct.structDefId = SendMessage(GetDlgItem(hwnd, 10), CB_GETCURSEL, 0, 0);
				if (HIWORD(wParam) == EN_CHANGE)
				{
					if (LOWORD(wParam) == 11 && GetFocus() == (HWND) lParam)
					{
						char text[15];

						SendMessage(GetDlgItem(hwnd, 11), WM_GETTEXT, 15, (LPARAM) text);
						text[14] = '\0';

						if (sscanf(text, "%i", &dlgPlaceStruct.numItems) == 1)
						{
							sprintf(text, "%08X", GetSelAddress(-1) + dlgPlaceStruct.numItems * structDefs[dlgPlaceStruct.structDefId].size);
							SendMessage(GetDlgItem(hwnd, 12), WM_SETTEXT, 0, (LPARAM) text);
						}
					}
					else if (LOWORD(wParam) == 12 && GetFocus() == (HWND) lParam)
					{
						char text[15];
						UINT32 endAddress;

						SendMessage(GetDlgItem(hwnd, 12), WM_GETTEXT, 15, (LPARAM) text);
						text[14] = '\0';


						if (sscanf(text, "%08X", &endAddress) == 1)
						{
							if (endAddress > GetSelAddress(-1))
							{
								dlgPlaceStruct.numItems = (endAddress - GetSelAddress(-1) + 4) / structDefs[dlgPlaceStruct.structDefId].size;

								sprintf(text, "%i", dlgPlaceStruct.numItems);
								SendMessage(GetDlgItem(hwnd, 11), WM_SETTEXT, 0, (LPARAM) text);
							}
						}
					}
				}
				if (LOWORD(wParam) == IDOK)
				{
					AddStructInst(GetSelAddress(-1), dlgPlaceStruct.structDefId, dlgPlaceStruct.numItems);
					EndDialog(hwnd, 1);
				}
				if (LOWORD(wParam) == IDCANCEL)
					EndDialog(hwnd, 0);
				break;
			}
			case WM_CLOSE:
				EndDialog(hwnd, 0);
				break;
		}
	}

	return 0;
}

void CreateEditWindow(int section, int selection)
{
	HWND wnd = NULL;
	int boxx, boxy = selection * 14 + listbox_y;
	int boxwidth, boxheight = 14;
	char settext[512] = "";
	int readonly = 0;
	unsigned int addr = GetSelAddress(selection);
	bool labelExists = false;

	switch (section)
	{
		case 0: // Address section
			section = 0; boxx = sectionx[0] + 6; boxwidth = 58;
			sprintf(settext, "%08X", addr);
		
			readonly = ES_READONLY;
			break;
		case 1: // Value
			section = 1; boxx = sectionx[1]-2; boxwidth = 58;

			if (lines[addr / 4].datatype == DATATYPE_BYTE)
				wsprintf(settext, "%02X", *(unsigned char*) &mem8[addr]);
			else if (lines[addr / 4].datatype == DATATYPE_HWORD)
				sprintf(settext, "%04X", *(unsigned short*) &mem16[addr / 2]);
			else
				sprintf(settext, "%08X", mem[addr / 4]);
			break;
		case 2: // Label
			section = 2; boxx = sectionx[2]-1; boxwidth = 180;

			for (int i = 0; i < numlabels; i ++)
			{
				if (labels[i].address == addr)
					labelExists = true;
			}

			if (labelExists) // strlen(list.labels[selection]) - replaced because it denoted references as labels
				strcpy(settext, list.labels[selection]);
			else if (lines[addr / 4].datatype == DATATYPE_BYTE)
			{
				// Try and make a label from memory characters
				bool terminated = 0;
			
				for (UINT32 i = addr; i < addr + 256; i ++)
				{
					if (i >= memlen8) break;

					if (mem8[i] == '\0')
					{
						terminated = true;
						break;
					}
				}

				if (terminated)
					sprintf(settext, "\"%s\"", &mem8[addr]);
			}
			
			break;
		case 3: // Code
			section = 3; boxx = sectionx[3]-2; boxwidth = 250;

			switch (lines[addr / 4].datatype)
			{
				case DATATYPE_CODE:
				{
					char useThisConversionInstead[100];

					if (CodeToASM(useThisConversionInstead, addr, mem[addr / 4]))
						strcpy(settext, useThisConversionInstead);
					else
						strcpy(settext, "[Oh &%*$ it's unknown]");
					break;
				}
				case DATATYPE_BYTE:
					sprintf(settext, "%i", mem8[addr]);
					break;
				case DATATYPE_HWORD:
					sprintf(settext, "%i", mem16[addr / 2]);
					break;
				case DATATYPE_WORD:
					sprintf(settext, "%i", mem[addr / 4]);
					break;
				case DATATYPE_FLOAT:
				{
					float totallyafloat;
					*(UINT32*) &totallyafloat = mem[addr / 4];
					sprintf(settext, "%f", totallyafloat);
					break;
				}
			}
			break;
	}

	wnd = CreateWindowEx(0, "EDIT", "derp", WS_VISIBLE | WS_CHILD | readonly, 
		boxx, boxy, boxwidth, boxheight, main_hwnd, NULL, globalInst, 0);

	SendMessage(wnd, WM_SETFONT, (WPARAM) global_listfont, 0);
	SendMessage(wnd, WM_SETTEXT, 0, (LPARAM) settext);
	SendMessage(wnd, EM_SETSEL, 0, -1);

	if (wndEdit.hwnd)
		DestroyWindow(wndEdit.hwnd);

	wndEdit.hwnd = wnd;
	wndEdit.address = GetSelAddress(selection);
	wndEdit.section = section;

	EnableWindow(listbox_hwnd, 0);
	SetActiveWindow(wndEdit.hwnd);
	SetFocus(wndEdit.hwnd);
}

void RepositionEditWindow()
{
	if (! wndEdit.hwnd)
		return;

	if (wndEdit.address < list.address || wndEdit.address > GetSelAddress(list.maxitems))
		ShowWindow(wndEdit.hwnd, 0);
	else
	{
		RECT mainrect, editrect;

		GetWindowRect(listbox_hwnd, &mainrect);
		GetWindowRect(wndEdit.hwnd, &editrect);

		ShowWindow(wndEdit.hwnd, 1);
		SetWindowPos(wndEdit.hwnd, HWND_TOP, editrect.left - mainrect.left - 0, 14 + GetAddressSel(wndEdit.address) * 14, 0, 0, 
			SWP_NOACTIVATE | SWP_NOSIZE);
	}
}

void SetEditWindowText(const char* text)
{
	DWORD s1, s2;

	SendMessage(wndEdit.hwnd, EM_GETSEL, (WPARAM) &s1, (LPARAM) &s2);
	SendMessage(wndEdit.hwnd, WM_SETTEXT, 0, (WPARAM) text);
	SendMessage(wndEdit.hwnd, EM_SETSEL, (WPARAM) s1, (LPARAM) s2);
}

INT_PTR CALLBACK ProgressDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
			RECT mainrect;
			GetWindowRect(hwnd, &mainrect);

			SetWindowPos(hwnd, HWND_TOP, 
			mainrect.left, 
			mainrect.top, 
			(mainrect.right - mainrect.left - 3) * 2 / 3, 
			(mainrect.bottom - mainrect.top - 25) * 3 / 5, 
			0);

			EnumChildWindows(hwnd, ConvertDlgCoords, (LPARAM) hwnd);

			SendMessage(GetDlgItem(hwnd, ID_STATIC_PROGRESS), WM_SETTEXT, 0, (LPARAM) dlgProgress.text);

			break;
	}

	return 0;
}

void CreateProgressDialog(const char* infoText)
{
	if (dlgProgress.hwnd)
		return;

	strncpy(dlgProgress.text, infoText, 256); // Do this first (text is set in WM_INITDIALOG)
	dlgProgress.text[255] = '\0';

	dlgProgress.hwnd = CreateDialog(globalInst, MAKEINTRESOURCE(MAKEWORD(ID_DIALOG_PROGRESS, 0)), main_hwnd, ProgressDialogProc);
	ShowWindow(dlgProgress.hwnd, true);

	SendMessage(GetDlgItem(dlgProgress.hwnd, ID_PROGRESSBAR_PROGRESS), PBM_SETRANGE32, 0, 1000);
	SendMessage(GetDlgItem(dlgProgress.hwnd, ID_PROGRESSBAR_PROGRESS), PBM_SETPOS, 0, 0);
}

void UpdateProgressDialog(int progress)
{
	if (! dlgProgress.hwnd)
		return;

	MSG temp;

	SendMessage(GetDlgItem(dlgProgress.hwnd, ID_PROGRESSBAR_PROGRESS), PBM_SETPOS, progress, 0);

	while (PeekMessage(&temp, dlgProgress.hwnd, 0, 0, PM_REMOVE) != 0)
		IsDialogMessage(dlgProgress.hwnd, &temp);
}

void DestroyProgressDialog()
{
	DestroyWindow(dlgProgress.hwnd);
	dlgProgress.hwnd = NULL;
}

void UpdateLabelDialogLabels()
{
	HWND labelList = GetDlgItem(dlgLabels.hwnd, ID_LISTBOX_GOTOLABEL);
	int oldLabelId = -1, oldCurSel = SendMessage(labelList, LB_GETCURSEL, 0, 0), oldTopIndex = SendMessage(labelList, LB_GETTOPINDEX, 0, 0);

	if (oldCurSel >= 0)
		oldLabelId = (int) SendMessage(labelList, LB_GETITEMDATA, oldCurSel, 0);

	SendMessage(labelList, LB_RESETCONTENT, 0, 0);

	int numListItems = 0;
	for (int i = 0; i < numlabels; i ++)
	{
		UINT temp;
		if (labels[i].autoGenerated && ! strncmp(labels[i].string, "FUNC_", 5))
			continue;

		// Check label meets conditions
		switch (dlgLabels.curCategory)
		{
			case 1:
				if (! labels[i].autoGenerated)
					continue;
				break;
			case 2: // User-defined
				if (labels[i].autoGenerated)
					continue;
				break;
		}

		// Add the label and set the list item data to the label's ID
		int index;
		if (dlgLabels.curSort == 0)
			index = SendMessage(labelList, LB_ADDSTRING, 0, (LPARAM) labels[i].string);
		else
			index = SendMessage(labelList, LB_INSERTSTRING, numListItems, (LPARAM) labels[i].string);
		SendMessage(labelList, LB_SETITEMDATA, index, i);

		// Increment numListItems
		numListItems ++;
	}
	
	// Try to preserve the cursor position if possible
	if (oldLabelId != -1)
	{
		for (int i = 0; i < numListItems; i ++)
		{
			if (SendMessage(labelList, LB_GETITEMDATA, i, 0) == oldLabelId)
			{
				int newTopIndex = i - (oldCurSel - oldTopIndex);

				if (newTopIndex < 0)
					newTopIndex = 0;

				SendMessage(labelList, LB_SETCURSEL, i, 0);
				SendMessage(labelList, LB_SETTOPINDEX, newTopIndex, 0);
			}
		}
	}
}

void SearchLabelDialog(bool fromCurrent)
{
	HWND labelList = GetDlgItem(dlgLabels.hwnd, ID_LISTBOX_GOTOLABEL);
	int id = 0, startId;
	int numListItems = SendMessage(labelList, LB_GETCOUNT, 0, 0);

	if (! numListItems)
		return;

	// Get the search string
	SendMessage(GetDlgItem(dlgLabels.hwnd, ID_EDIT_GOTOLABEL), WM_GETTEXT, 256, (LPARAM) dlgLabels.text);
	dlgLabels.text[255] = '\0';

	// Make a lowercase version of the search string
	char lowercase[256];
	int strl = strlen(dlgLabels.text);

	for (int i = 0; i < strl; i ++)
	{
		lowercase[i] = dlgLabels.text[i];

		if (dlgLabels.text[i] >= 'A' && dlgLabels.text[i] <= 'Z')
			lowercase[i] -= ('A' - 'a');
	}

	// Decide the search start
	if (! fromCurrent)
		startId = 0;
	else
		startId = (int) SendMessage(labelList, LB_GETCURSEL, 0, 0) + 1;

	// Search!
	bool firstRun = false;
	bool found = false;

	for (id = startId; ; id ++)
	{
		if (id >= numListItems)
			id = 0;
		if (id == startId)
		{
			if (! firstRun)
				firstRun = true;
			else
				break;
		}

		//if (strstr(labels[SendMessage(labelList, LB_GETITEMDATA, id, 0)].string, dlgLabels.text))
		//	found = true;

		char* labelstr = labels[SendMessage(labelList, LB_GETITEMDATA, id, 0)].string;
		int lstrl = strlen(labelstr);

		for (int i = 0; i <= lstrl - strl; i ++)
		{
			int j;
			for (j = 0; j < strl; j ++)
			{
				char c = labelstr[i + j];

				if (c != lowercase[j] && ! (c >= 'A' && c <= 'Z' && c - ('A' - 'a') == lowercase[j]))
					goto CONT;
			}

			if (j == strl)
			{
				found = true;
				break;
			}

			CONT: continue;
		}

		if (found)
			break;
	}

	if (found)
		SendMessage(GetDlgItem(dlgLabels.hwnd, ID_LISTBOX_GOTOLABEL), LB_SETCURSEL, id, 0);
}

BOOL CALLBACK ConvertDlgCoords(HWND hwnd, LPARAM lParam)
{
	RECT mainrect, childrect;
	GetWindowRect((HWND) lParam, &mainrect);
	GetWindowRect(hwnd, &childrect);

	SetWindowPos(hwnd, HWND_TOP, 
		(childrect.left - mainrect.left - 3) * 2 / 3,
		(childrect.top - mainrect.top - 25) * 3 / 5, 
		(childrect.right - childrect.left) * 2 / 3, 
		(childrect.bottom - childrect.top) * 3 / 5,  
		0);

	return 1;
}
