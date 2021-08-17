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
#include "gui/AppCoreThread.h"
#include "USB/USB.h"
#include "resource_usb.h"
#include "Config_usb.h"
#include "USB/deviceproxy.h"
#include "USB/usb-pad/padproxy.h"
#include "USB/usb-mic/audiodeviceproxy.h"
#include "USB/configuration.h"
#include "USB/shared/inifile_usb.h"

HINSTANCE hInstUSB;

void SysMessageA(const char* fmt, ...)
{
	va_list list;
	char tmp[512];

	va_start(list, fmt);
	vsprintf_s(tmp, 512, fmt, list);
	va_end(list);
	MessageBoxA(0, tmp, "USB Msg", 0);
}

void SysMessageW(const wchar_t* fmt, ...)
{
	va_list list;
	wchar_t tmp[512];

	va_start(list, fmt);
	vswprintf_s(tmp, 512, fmt, list);
	va_end(list);
	MessageBoxW(0, tmp, L"USB Msg", 0);
}

void SelChangedAPI(HWND hW, int port)
{
	int sel = SendDlgItemMessage(hW, port ? IDC_COMBO_API1_USB : IDC_COMBO_API2_USB, CB_GETCURSEL, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1_USB : IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto devName = rd.Name(devtype);
	auto apis = rd.Device(devtype)->ListAPIs();
	auto it = apis.begin();
	std::advance(it, sel);
	changedAPIs[std::make_pair(port, devName)] = *it;
}

void SelChangedSubtype(HWND hW, int port)
{
	int sel = SendDlgItemMessage(hW, port ? IDC_COMBO_WHEEL_TYPE1_USB : IDC_COMBO_WHEEL_TYPE2_USB, CB_GETCURSEL, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1_USB : IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto devName = rd.Name(devtype);
	changedSubtype[std::make_pair(port, devName)] = sel;
}

void PopulateAPIs(HWND hW, int port)
{
	SendDlgItemMessage(hW, port ? IDC_COMBO_API1_USB : IDC_COMBO_API2_USB, CB_RESETCONTENT, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1_USB : IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto dev = rd.Device(devtype);
	auto devName = rd.Name(devtype);
	auto apis = dev->ListAPIs();

	std::string selApi = GetSelectedAPI(std::make_pair(port, devName));

	std::string var;
	std::wstring tmp;
	if (!LoadSetting(nullptr, port, rd.Name(devtype), N_DEVICE_API, tmp))
	{
		if (apis.begin() != apis.end())
		{
			selApi = *apis.begin();
			changedAPIs[std::make_pair(port, devName)] = selApi;
		}
	}

	var = wstr_to_str(tmp);
	int i = 0, sel = 0;
	for (auto& api : apis)
	{
		auto name = dev->LongAPIName(api);
		if (!name)
			continue;
		SendDlgItemMessageW(hW, port ? IDC_COMBO_API1_USB : IDC_COMBO_API2_USB, CB_ADDSTRING, 0, (LPARAM)name);
		if (api == var)
			sel = i;
		i++;
	}
	SendDlgItemMessage(hW, port ? IDC_COMBO_API1_USB : IDC_COMBO_API2_USB, CB_SETCURSEL, sel, 0);
}

void PopulateSubType(HWND hW, int port)
{
	SendDlgItemMessage(hW, port ? IDC_COMBO_WHEEL_TYPE1_USB : IDC_COMBO_WHEEL_TYPE2_USB, CB_RESETCONTENT, 0, 0);
	int devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1_USB : IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
	if (devtype == 0)
		return;
	devtype--;
	auto& rd = RegisterDevice::instance();
	auto dev = rd.Device(devtype);
	auto devName = rd.Name(devtype);

	int sel = 0;
	if (!LoadSetting(nullptr, port, dev->TypeName(), N_DEV_SUBTYPE, sel))
	{
		changedSubtype[std::make_pair(port, devName)] = sel;
	}

	for (auto subtype : dev->SubTypes())
	{
		SendDlgItemMessageA(hW, port ? IDC_COMBO_WHEEL_TYPE1_USB : IDC_COMBO_WHEEL_TYPE2_USB, CB_ADDSTRING, 0, (LPARAM)subtype.c_str());
	}
	SendDlgItemMessage(hW, port ? IDC_COMBO_WHEEL_TYPE1_USB : IDC_COMBO_WHEEL_TYPE2_USB, CB_SETCURSEL, sel, 0);
}

BOOL CALLBACK ConfigureDlgProcUSB(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	int port;
	switch (uMsg)
	{
		case WM_INITDIALOG:
			LoadConfig();
			CheckDlgButton(hW, IDC_LOGGING_USB, conf.Log);
			//Selected emulated devices.
			SendDlgItemMessageA(hW, IDC_COMBO1_USB, CB_ADDSTRING, 0, (LPARAM) "None");
			SendDlgItemMessageA(hW, IDC_COMBO2_USB, CB_ADDSTRING, 0, (LPARAM) "None");

			{
				auto& rd = RegisterDevice::instance();
				int i = 0, p1 = 0, p2 = 0;
				for (auto& name : rd.Names())
				{
					i++; //jump over "None"
					auto dev = rd.Device(name);
					SendDlgItemMessageW(hW, IDC_COMBO1_USB, CB_ADDSTRING, 0, (LPARAM)dev->Name());
					SendDlgItemMessageW(hW, IDC_COMBO2_USB, CB_ADDSTRING, 0, (LPARAM)dev->Name());

					//Port 1 aka device/player 1
					if (conf.Port[1] == name)
						p1 = i;
					//Port 0 aka device/player 2
					if (conf.Port[0] == name)
						p2 = i;
				}
				SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_SETCURSEL, p1, 0);
				SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_SETCURSEL, p2, 0);
				PopulateAPIs(hW, 0);
				PopulateAPIs(hW, 1);
				PopulateSubType(hW, 0);
				PopulateSubType(hW, 1);
			}

			return TRUE;
			break;
		case WM_COMMAND:
			switch (HIWORD(wParam))
			{
				case CBN_SELCHANGE:
					switch (LOWORD(wParam))
					{
						case IDC_COMBO_API1_USB:
						case IDC_COMBO_API2_USB:
							port = (LOWORD(wParam) == IDC_COMBO_API1_USB) ? 1 : 0;
							SelChangedAPI(hW, port);
							break;
						case IDC_COMBO1_USB:
						case IDC_COMBO2_USB:
							port = (LOWORD(wParam) == IDC_COMBO1_USB) ? 1 : 0;
							PopulateAPIs(hW, port);
							PopulateSubType(hW, port);
							break;
						case IDC_COMBO_WHEEL_TYPE1_USB:
						case IDC_COMBO_WHEEL_TYPE2_USB:
							port = (LOWORD(wParam) == IDC_COMBO_WHEEL_TYPE1_USB) ? 1 : 0;
							SelChangedSubtype(hW, port);
							break;
					}
					break;
				case BN_CLICKED:
					switch (LOWORD(wParam))
					{
						case IDC_CONFIGURE1_USB:
						case IDC_CONFIGURE2_USB:
						{
							LRESULT devtype, apitype;
							port = (LOWORD(wParam) == IDC_CONFIGURE1_USB) ? 1 : 0;
							devtype = SendDlgItemMessage(hW, port ? IDC_COMBO1_USB : IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
							apitype = SendDlgItemMessage(hW, port ? IDC_COMBO_API1_USB : IDC_COMBO_API2_USB, CB_GETCURSEL, 0, 0);

							if (devtype > 0)
							{
								devtype--;
								auto device = RegisterDevice::instance().Device(devtype);
								if (device)
								{
									auto list = device->ListAPIs();
									auto it = list.begin();
									std::advance(it, apitype);
									if (it == list.end())
										break;
									std::string api = *it;
									Win32Handles handles(hInstUSB, hW);
									if (device->Configure(port, api, &handles) == RESULT_FAILED)
										SysMessage(TEXT("Some settings may not have been saved!\n"));
								}
							}
						}
						break;
						case IDCANCEL:
							EndDialog(hW, TRUE);
							return TRUE;
						case IDOK:
							conf.Log = IsDlgButtonChecked(hW, IDC_LOGGING_USB);
							{
								auto& regInst = RegisterDevice::instance();
								int i;
								//device type
								i = SendDlgItemMessage(hW, IDC_COMBO1_USB, CB_GETCURSEL, 0, 0);
								conf.Port[1] = regInst.Name(i - 1);
								i = SendDlgItemMessage(hW, IDC_COMBO2_USB, CB_GETCURSEL, 0, 0);
								conf.Port[0] = regInst.Name(i - 1);
							}

							SaveConfig();
							CreateDevices();
							EndDialog(hW, RESULT_OK);
							return TRUE;
					}
			}
	}

	return FALSE;
}

void USBconfigure()
{
	ScopedCoreThreadPause paused_core;
    USBsetSettingsDir();
	RegisterDevice::Register();
	DialogBox(hInstUSB,
			  MAKEINTRESOURCE(IDD_CONFIG_USB),
			  GetActiveWindow(),
			  (DLGPROC)ConfigureDlgProcUSB);
	paused_core.AllowResume();
}
