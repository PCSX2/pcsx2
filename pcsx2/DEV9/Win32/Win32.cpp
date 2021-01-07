/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include <stdio.h>
#include <commdlg.h>

#include <string>
#include "ghc/filesystem.h"

#include "..\Config.h"
#include "resource.h"
#include "..\DEV9.h"
#include "..\pcap_io.h"
#include "..\net.h"
#include "tap.h"
#include "AppCoreThread.h"

#include "../ATA/HddCreate.h"

extern HINSTANCE hInst;
//HANDLE handleDEV9Thread = NULL;
//DWORD dwThreadId, dwThrdParam;

void SysMessage(char* fmt, ...)
{
	va_list list;
	char tmp[512];

	va_start(list, fmt);
	vsprintf(tmp, fmt, list);
	va_end(list);
	MessageBoxA(0, tmp, "Dev9 Msg", 0);
}

void OnInitDialog(HWND hW)
{
	char* dev;
	//int i;

	LoadConf();

	ComboBox_AddString(GetDlgItem(hW, IDC_BAYTYPE), "Expansion");
	ComboBox_AddString(GetDlgItem(hW, IDC_BAYTYPE), "PC Card");
	for (int i = 0; i < pcap_io_get_dev_num(); i++)
	{
		dev = pcap_io_get_dev_desc(i);
		int itm = ComboBox_AddString(GetDlgItem(hW, IDC_ETHDEV), dev);
		ComboBox_SetItemData(GetDlgItem(hW, IDC_ETHDEV), itm, _strdup(pcap_io_get_dev_name(i)));
		if (strcmp(pcap_io_get_dev_name(i), config.Eth) == 0)
		{
			ComboBox_SetCurSel(GetDlgItem(hW, IDC_ETHDEV), itm);
		}
	}
	vector<tap_adapter>* al = GetTapAdapters();
	for (size_t i = 0; i < al->size(); i++)
	{
		int itm = ComboBox_AddString(GetDlgItem(hW, IDC_ETHDEV), al[0][i].name);
		char guid_char[256];
		wcstombs(guid_char, al[0][i].guid, wcslen(al[0][i].guid) + 1);
		ComboBox_SetItemData(GetDlgItem(hW, IDC_ETHDEV), itm, _strdup(guid_char));
		if (strcmp(guid_char, config.Eth) == 0)
		{
			ComboBox_SetCurSel(GetDlgItem(hW, IDC_ETHDEV), itm);
		}
	}

	SetWindowText(GetDlgItem(hW, IDC_HDDFILE), config.Hdd);

	//HDDText
	Edit_SetText(GetDlgItem(hW, IDC_HDDSIZE_TEXT), std::to_wstring(config.HddSize / 1024).c_str());
	Edit_LimitText(GetDlgItem(hW, IDC_HDDSIZE_TEXT), 3); //Excluding null char
	//HDDSpin
	SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SPIN), UDM_SETRANGE,
				(WPARAM)0,
				(LPARAM)MAKELPARAM(HDD_MAX_GB, HDD_MIN_GB));
	SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SPIN), UDM_SETPOS,
				(WPARAM)0,
				(LPARAM)(config.HddSize / 1024));

	//HDDSlider
	SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETRANGE,
				(WPARAM)FALSE,
				(LPARAM)MAKELPARAM(HDD_MIN_GB, HDD_MAX_GB));
	SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETPAGESIZE,
				(WPARAM)0,
				(LPARAM)10);

	for (int i = 15; i < HDD_MAX_GB; i += 5)
	{
		SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETTIC,
					(WPARAM)0,
					(LPARAM)i);
	}

	SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETPOS,
				(WPARAM)TRUE,
				(LPARAM)(config.HddSize / 1024));

	//Checkboxes
	Button_SetCheck(GetDlgItem(hW, IDC_ETHENABLED), config.ethEnable);
	Button_SetCheck(GetDlgItem(hW, IDC_HDDENABLED), config.hddEnable);
}

void OnBrowse(HWND hW)
{
	wchar_t wbuff[4096] = {0};
	memcpy(wbuff, HDD_DEF, sizeof(HDD_DEF));

	//GHC uses UTF8 on all platforms
	ghc::filesystem::path inis = GetSettingsFolder().ToUTF8().data();
	wstring w_inis = inis.wstring();

	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hW;
	ofn.lpstrTitle = L"HDD image file";
	ofn.lpstrFile = wbuff;
	ofn.nMaxFile = ArraySize(wbuff);
	ofn.lpstrFilter = L"HDD\0*.raw\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = w_inis.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileName(&ofn))
	{
		ghc::filesystem::path hddFile(std::wstring(ofn.lpstrFile));

		if (ghc::filesystem::exists(hddFile))
		{
			//Get file size
			int filesizeGb = ghc::filesystem::file_size(hddFile) / (1024 * 1024 * 1024);
			//Set slider
			SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SPIN), UDM_SETPOS,
						(WPARAM)0,
						(LPARAM)filesizeGb);
			SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETPOS,
						(WPARAM)TRUE,
						(LPARAM)filesizeGb);
		}

		if (hddFile.parent_path() == inis)
			hddFile = hddFile.filename();
		Edit_SetText(GetDlgItem(hW, IDC_HDDFILE), hddFile.wstring().c_str());
	}
}

void OnOk(HWND hW)
{
	int i = ComboBox_GetCurSel(GetDlgItem(hW, IDC_ETHDEV));
	if (i == -1)
	{
		//adapter not selected
		if (Button_GetCheck(GetDlgItem(hW, IDC_ETHENABLED)))
		{
			//Trying to use an ethernet without
			//selected adapter, we can't have that
			SysMessage("Please select an ethernet adapter");
			return;
		}
		else
		{
			//user not planning on using
			//ethernet anyway
			strcpy(config.Eth, ETH_DEF);
		}
	}
	else
	{
		//adapter is selected
		char* ptr = (char*)ComboBox_GetItemData(GetDlgItem(hW, IDC_ETHDEV), i);
		strcpy(config.Eth, ptr);
	}

	GetWindowText(GetDlgItem(hW, IDC_HDDFILE), config.Hdd, 256);

	if (Edit_GetTextLength(GetDlgItem(hW, IDC_HDDSIZE_TEXT)) == 0)
		config.HddSize = HDD_MIN_GB * 1024;
	else
	{
		wchar_t text[4];
		GetWindowText(GetDlgItem(hW, IDC_HDDSIZE_TEXT), text, 4);
		config.HddSize = stoi(text) * 1024;
	}

	config.ethEnable = Button_GetCheck(GetDlgItem(hW, IDC_ETHENABLED));
	config.hddEnable = Button_GetCheck(GetDlgItem(hW, IDC_HDDENABLED));

	ghc::filesystem::path hddPath(std::wstring(config.Hdd));

	if (config.hddEnable && hddPath.empty())
	{
		SysMessage("Please specify a HDD file");
		return;
	}

	if (hddPath.is_relative())
	{
		//GHC uses UTF8 on all platforms
		ghc::filesystem::path path(GetSettingsFolder().ToUTF8().data());
		hddPath = path / hddPath;
	}

	if (config.hddEnable && !ghc::filesystem::exists(hddPath))
	{
		HddCreate hddCreator;
		hddCreator.filePath = hddPath;
		hddCreator.neededSize = config.HddSize;
		hddCreator.Start();

		if (hddCreator.errored)
			return;
	}

	SaveConf();

	EndDialog(hW, TRUE);
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	switch (uMsg)
	{
		case WM_INITDIALOG:
			OnInitDialog(hW);
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
				case IDOK:
					if (GetFocus() != GetDlgItem(hW, IDOK))
					{
						SetFocus(GetDlgItem(hW, IDOK));
						return TRUE;
					}

					OnOk(hW);
					return TRUE;
				case IDC_BROWSE:
					OnBrowse(hW);
					return TRUE;
				case IDC_HDDSIZE_TEXT:
				{
					if (GetFocus() != GetDlgItem(hW, IDC_HDDSIZE_TEXT))
						return TRUE;

					if (Edit_GetTextLength(GetDlgItem(hW, IDC_HDDSIZE_TEXT)) == 0)
						return TRUE;

					wchar_t text[4];
					Edit_GetText(GetDlgItem(hW, IDC_HDDSIZE_TEXT), text, 4); //Including null char

					switch (HIWORD(wParam))
					{
						case EN_CHANGE:
						{
							int curpos = stoi(text);

							if (HDD_MIN_GB > curpos)
								//user may still be typing
								return TRUE;

							SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SPIN), UDM_SETPOS,
										(WPARAM)0,
										(LPARAM)curpos);
							SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETPOS,
										(WPARAM)TRUE,
										(LPARAM)curpos);
							return TRUE;
						}
					}
					return FALSE;
				}
				default:
					return FALSE;
			}
		case WM_HSCROLL:
		{
			HWND hwndDlg = (HWND)lParam;
			int curpos = HIWORD(wParam);

			switch (LOWORD(wParam))
			{
				case TB_LINEUP:
				case TB_LINEDOWN:
				case TB_PAGEUP:
				case TB_PAGEDOWN:
				case TB_TOP:
				case TB_BOTTOM:
					curpos = (int)SendMessage(hwndDlg, TBM_GETPOS, 0, 0);
					[[fallthrough]];

				case TB_THUMBPOSITION:
				case TB_THUMBTRACK:
					//Update Textbox
					SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SPIN), UDM_SETPOS,
								(WPARAM)0,
								(LPARAM)curpos);
					return TRUE;

				default:
					return FALSE;
			}
		}
		case WM_VSCROLL:
		{
			HWND hwndDlg = (HWND)lParam;
			int curpos = HIWORD(wParam);

			switch (LOWORD(wParam))
			{
				case SB_LINEUP:
				case SB_LINEDOWN:
				case SB_PAGEUP:
				case SB_PAGEDOWN:
				case SB_TOP:
				case SB_BOTTOM:
					curpos = (int)SendMessage(hwndDlg, UDM_GETPOS, 0, 0);
					[[fallthrough]];

				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					//Update Textbox
					//Edit_SetText(GetDlgItem(hW, IDC_HDDSIZE_TEXT), to_wstring(curpos).c_str());
					SendMessage(GetDlgItem(hW, IDC_HDDSIZE_SLIDER), TBM_SETPOS,
								(WPARAM)TRUE,
								(LPARAM)curpos);
					return TRUE;

				default:
					return FALSE;
			}
		}
	}
	return FALSE;
}

void DEV9configure()
{
	ScopedCoreThreadPause paused_core;
	DialogBox(hInst,
			  MAKEINTRESOURCE(IDD_CONFIG),
			  GetActiveWindow(),
			  (DLGPROC)ConfigureDlgProc);
	//SysMessage("Nothing to Configure");
	paused_core.AllowResume();
}

/*
UINT DEV9ThreadProc() {
	DEV9thread();

	return 0;
}*/
NetAdapter* GetNetAdapter()
{
	NetAdapter* na = static_cast<NetAdapter*>(new TAPAdapter());

	if (!na->isInitialised())
	{
		delete na;
		return 0;
	}
	return na;
}
s32 _DEV9open()
{
	//handleDEV9Thread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) DEV9ThreadProc, &dwThrdParam, CREATE_SUSPENDED, &dwThreadId);
	//SetThreadPriority(handleDEV9Thread,THREAD_PRIORITY_HIGHEST);
	//ResumeThread (handleDEV9Thread);
	NetAdapter* na = GetNetAdapter();
	if (!na)
	{
		Console.Error("Failed to GetNetAdapter()");
		config.ethEnable = false;
	}
	else
	{
		InitNet(na);
	}
	return 0;
}

void _DEV9close()
{
	//TerminateThread(handleDEV9Thread,0);
	//handleDEV9Thread = NULL;
	TermNet();
}
