/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>

#include "..\Config.h"
#include "resource.h"
#include "..\DEV9.h"
#include "..\pcap_io.h"
#include "..\net.h"
#include "tap.h"

extern HINSTANCE hInst;
//HANDLE handleDEV9Thread = NULL;
//DWORD dwThreadId, dwThrdParam;

void SysMessage(char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf(tmp,fmt,list);
	va_end(list);
	MessageBox(0, tmp, "Dev9 Msg", 0);
}

void OnInitDialog(HWND hW) {
	char *dev;
	//int i;

	LoadConf();

	ComboBox_AddString(GetDlgItem(hW, IDC_BAYTYPE), "Expansion");
	ComboBox_AddString(GetDlgItem(hW, IDC_BAYTYPE), "PC Card");
	for (int i=0; i<pcap_io_get_dev_num(); i++) {
		dev = pcap_io_get_dev_desc(i);
		int itm=ComboBox_AddString(GetDlgItem(hW, IDC_ETHDEV), dev);
		ComboBox_SetItemData(GetDlgItem(hW, IDC_ETHDEV),itm,_strdup(pcap_io_get_dev_name(i)));
		if (strcmp(pcap_io_get_dev_name(i), config.Eth) == 0) {
			ComboBox_SetCurSel(GetDlgItem(hW, IDC_ETHDEV), itm);
		}
	}
	vector<tap_adapter> * al=GetTapAdapters();
	for (size_t i=0; i<al->size(); i++) {
		int itm=ComboBox_AddString(GetDlgItem(hW, IDC_ETHDEV), al[0][i].name.c_str());
		ComboBox_SetItemData(GetDlgItem(hW, IDC_ETHDEV),itm,_strdup( al[0][i].guid.c_str()));
		if (strcmp(al[0][i].guid.c_str(), config.Eth) == 0) {
			ComboBox_SetCurSel(GetDlgItem(hW, IDC_ETHDEV), itm);
		}
	}

	Edit_SetText(GetDlgItem(hW, IDC_HDDFILE), config.Hdd);

	Button_SetCheck(GetDlgItem(hW, IDC_ETHENABLED), config.ethEnable);
	Button_SetCheck(GetDlgItem(hW, IDC_HDDENABLED), config.hddEnable);
}

void OnOk(HWND hW) {
	int i = ComboBox_GetCurSel(GetDlgItem(hW, IDC_ETHDEV));
	if (i == -1)
	{
		//adapter not selected
		if ( Button_GetCheck(GetDlgItem(hW, IDC_ETHENABLED)))
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

	Edit_GetText(GetDlgItem(hW, IDC_HDDFILE), config.Hdd, 256);

	config.ethEnable = Button_GetCheck(GetDlgItem(hW, IDC_ETHENABLED));
	config.hddEnable = Button_GetCheck(GetDlgItem(hW, IDC_HDDENABLED));

	SaveConf();

	EndDialog(hW, TRUE);
}

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch(uMsg) {
		case WM_INITDIALOG:
			OnInitDialog(hW);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hW, FALSE);
					return TRUE;
				case IDOK:
					OnOk(hW);
					return TRUE;
			}
	}
	return FALSE;
}

BOOL CALLBACK AboutDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					EndDialog(hW, FALSE);
					return TRUE;
			}
	}
	return FALSE;
}

EXPORT_C_(void)
DEV9configure() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_CONFIG),
              GetActiveWindow(),
             (DLGPROC)ConfigureDlgProc);
		//SysMessage("Nothing to Configure");
}

EXPORT_C_(void)
DEV9about() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_ABOUT),
              GetActiveWindow(),
              (DLGPROC)AboutDlgProc);
}

BOOL APIENTRY DllMain(HANDLE hModule,                  // DLL INIT
                      DWORD  dwReason,
                      LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	return TRUE;                                          // very quick :)
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
s32  _DEV9open()
{
	//handleDEV9Thread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) DEV9ThreadProc, &dwThrdParam, CREATE_SUSPENDED, &dwThreadId);
	//SetThreadPriority(handleDEV9Thread,THREAD_PRIORITY_HIGHEST);
	//ResumeThread (handleDEV9Thread);
	NetAdapter* na=GetNetAdapter();
	if (!na)
	{
		emu_printf("Failed to GetNetAdapter()\n");
		config.ethEnable = false;
	}
	else
	{
		InitNet(na);
	}
	return 0;
}

void _DEV9close() {
	//TerminateThread(handleDEV9Thread,0);
	//handleDEV9Thread = NULL;
	TermNet();
}
