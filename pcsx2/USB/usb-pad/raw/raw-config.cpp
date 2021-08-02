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

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0502
#endif
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include "USB/configuration.h"
#include "usb-pad-raw.h"
#include "raw-config-res.h"
#include <strsafe.h>

extern HINSTANCE hInst;
#define MSG_PRESS_ESC(wnd) SendDlgItemMessageW(wnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Capturing, press ESC to cancel")

namespace usb_pad
{
	namespace raw
	{

		inline bool MapExists(const MapVector& maps, const std::wstring& hid)
		{
			for (auto& it : maps)
				if (!it.hidPath.compare(hid))
					return true;
			return false;
		}

		void LoadMappings(const char* dev_type, MapVector& maps)
		{
			maps.clear();

			WCHAR sec[1024] = {0}, bind[32] = {0};
			int v = 0;
			for (int j = 0; j < 25; j++)
			{
				std::wstring hid, tmp;
				//swprintf_s(sec, TEXT("%S RAW DEVICE %d"), dev_type, j++);

				if (LoadSetting(dev_type, j, "RAW DEVICE", TEXT("HID"), hid) && !hid.empty() && !MapExists(maps, hid))
				{
					Mappings m;
					ZeroMemory(&m, sizeof(Mappings));
					maps.push_back(m);
					Mappings& ptr = maps.back();

					ptr.hidPath = hid;
					ptr.devName = hid;
					//pad_reset_data(&ptr.data[0]);
					//pad_reset_data(&ptr.data[1]);
					memset(&ptr.data[0], 0xFF, sizeof(wheel_data_t));
					memset(&ptr.data[1], 0xFF, sizeof(wheel_data_t));
					ptr.data[0].buttons = 0;
					ptr.data[1].buttons = 0;
					ptr.data[0].hatswitch = 0x8; //memset to 0xFF already or set to -1
					ptr.data[1].hatswitch = 0x8;

					for (int i = 0; i < MAX_BUTTONS; i++)
					{
						swprintf_s(bind, L"Button %d", i);
						if (LoadSetting(dev_type, j, "RAW DEVICE", bind, tmp))
							swscanf_s(tmp.c_str(), L"%08X", &(ptr.btnMap[i]));
					}

					for (int i = 0; i < MAX_AXES; i++)
					{
						swprintf_s(bind, L"Axis %d", i);
						if (LoadSetting(dev_type, j, "RAW DEVICE", bind, tmp))
							swscanf_s(tmp.c_str(), L"%08X", &(ptr.axisMap[i]));
					}

					for (int i = 0; i < 4 /*PAD_HAT_COUNT*/; i++)
					{
						swprintf_s(bind, L"Hat %d", i);
						if (LoadSetting(dev_type, j, "RAW DEVICE", bind, tmp))
							swscanf_s(tmp.c_str(), L"%08X", &(ptr.hatMap[i]));
					}
				}
			}
			return;
		}

		void SaveMappings(const char* dev_type, MapVector& maps)
		{
			uint32_t numDevice = 0;
			for (auto& it : maps)
			{
				WCHAR dev[1024] = {0}, tmp[16] = {0}, bind[32] = {0};

				SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", TEXT("HID"), it.hidPath);

				//writing everything separately, then string lengths are more predictable
				for (int i = 0; i < MAX_BUTTONS; i++)
				{
					swprintf_s(bind, L"Button %d", i);
					swprintf_s(tmp, L"%08X", it.btnMap[i]);
					SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", bind, tmp);
				}

				for (int i = 0; i < MAX_AXES; i++)
				{
					swprintf_s(bind, L"Axis %d", i);
					swprintf_s(tmp, L"%08X", it.axisMap[i]);
					SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", bind, tmp);
				}

				for (int i = 0; i < 4 /*PAD_HAT_COUNT*/; i++)
				{
					swprintf_s(bind, L"Hat %d", i);
					swprintf_s(tmp, L"%08X", it.hatMap[i]);
					SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", bind, tmp);
				}
				numDevice++;
			}

			return;
		}

/// Dialogs
#define TXT(x) (#x)
		const char* BTN2TXT[] = {
			"Cross",
			"Square",
			"Circle",
			"Triangle",
			"R1",
			"L1",
			"R2",
			"L2",
			"Select",
			"Start",
			"R3",
			"L3"};

		const char* AXIS2TXT[] = {
			"Axis X",
			"Axis Y",
			"Axis Z",
			//"Axis RX",
			//"Axis RY",
			"Axis RZ",
			"Hat Switch"};

		void resetState(HWND hW);
		HWND dgHwnd = NULL;
		//std::vector<std::wstring> joysName;
		static std::vector<std::wstring> joysDev;
		static DWORD selectedJoy[2];
		//std::vector<std::string>::iterator* tmpIter;

		typedef struct _DevInfo
		{
			int ply;
			RID_DEVICE_INFO_HID hid;

			bool operator==(const _DevInfo& t) const
			{
				if (ply == t.ply && hid.dwProductId == t.hid.dwProductId &&
					hid.dwVendorId == t.hid.dwVendorId &&
					hid.dwVersionNumber == t.hid.dwVersionNumber &&
					hid.usUsage == t.hid.usUsage &&
					hid.usUsagePage == t.hid.usUsagePage)
					return true;
				return false;
			}

			bool operator<(const _DevInfo& t) const
			{
				if (ply < t.ply)
					return true;
				if (hid.dwProductId < t.hid.dwProductId)
					return true;
				if (hid.dwVendorId < t.hid.dwVendorId)
					return true;
				if (hid.dwVersionNumber < t.hid.dwVersionNumber)
					return true;
				return false;
			}

		} DevInfo_t;

		//typedef std::map<const DevInfo_t, Mappings_t> MappingsMap;
		//MappingsMap mappings;

		uint32_t axisDiff[MAX_AXES]; //previous axes values
		bool axisPass2 = false;

		//eh, global var for currently selected player
		static int plyCapturing = 0;
		PS2Buttons btnCapturing = PAD_BUTTON_COUNT;
		PS2Axis axisCapturing = PAD_AXIS_COUNT;
		PS2HatSwitch hatCapturing = PAD_HAT_COUNT;

		void populate(HWND hW, RawDlgConfig* cfg)
		{
			//mappings.clear();
			//joysName.clear();
			//joysName.push_back("None");
			joysDev.clear();
			joysDev.push_back(L"");

			int i = 0, sel_idx = 1;
			HANDLE usbHandle = INVALID_HANDLE_VALUE;
			DWORD needed = 0;
			HDEVINFO devInfo;
			GUID guid;
			SP_DEVICE_INTERFACE_DATA diData;
			PSP_DEVICE_INTERFACE_DETAIL_DATA didData = NULL;
			HIDD_ATTRIBUTES attr;
			PHIDP_PREPARSED_DATA pPreparsedData = NULL;
			HIDP_CAPS caps;
			OVERLAPPED ovl;

			memset(&ovl, 0, sizeof(OVERLAPPED));
			ovl.hEvent = CreateEvent(0, 0, 0, 0);
			ovl.Offset = 0;
			ovl.OffsetHigh = 0;

			HidD_GetHidGuid(&guid);

			devInfo = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_DEVICEINTERFACE);
			if (!devInfo)
				return;

			diData.cbSize = sizeof(diData);

			//Mappings listview
			LVCOLUMN LvCol;
			memset(&LvCol, 0, sizeof(LvCol));
			LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
			LvCol.pszText = TEXT("Device");
			LvCol.cx = 0x4F;
			ListView_InsertColumn(GetDlgItem(hW, IDC_LIST1), 0, &LvCol);

			LvCol.pszText = TEXT("PC");
			ListView_InsertColumn(GetDlgItem(hW, IDC_LIST1), 1, &LvCol);

			LvCol.pszText = TEXT("PS2");
			ListView_InsertColumn(GetDlgItem(hW, IDC_LIST1), 2, &LvCol);

			//Tab control
			TCITEM tie;
			tie.pszText = TEXT("Player 1");
			tie.cchTextMax = 32;
			tie.mask = TCIF_TEXT;
			SendDlgItemMessage(hW, IDC_TAB1, TCM_INSERTITEM, 0, (LPARAM)&tie);

			tie.pszText = TEXT("Player 2");
			SendDlgItemMessage(hW, IDC_TAB1, TCM_INSERTITEM, 1, (LPARAM)&tie);

			//Selected FFB target device
			SendDlgItemMessageA(hW, IDC_COMBO_FFB, CB_ADDSTRING, 0, (LPARAM) "None");
			SendDlgItemMessage(hW, IDC_COMBO_FFB, CB_SETCURSEL, 0, 0);

			while (SetupDiEnumDeviceInterfaces(devInfo, 0, &guid, i, &diData))
			{
				if (usbHandle != INVALID_HANDLE_VALUE)
					CloseHandle(usbHandle);

				SetupDiGetDeviceInterfaceDetail(devInfo, &diData, 0, 0, &needed, 0);

				didData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(needed);
				if (!didData)
					break;
				didData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				if (!SetupDiGetDeviceInterfaceDetail(devInfo, &diData, didData, needed, 0, 0))
				{
					free(didData);
					break;
				}

				usbHandle = CreateFile(didData->DevicePath, GENERIC_READ | GENERIC_WRITE,
									   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

				if (usbHandle == INVALID_HANDLE_VALUE)
				{
					Console.Warning("Could not open device %i", i);
					free(didData);
					i++;
					continue;
				}

				HidD_GetAttributes(usbHandle, &attr);
				if (!HidD_GetPreparsedData(usbHandle, &pPreparsedData))
				{
					Console.Warning("Could not get preparsed data from %04x:%04x", attr.VendorID, attr.ProductID);
					free(didData);
					i++;
					continue;
				}

				HidP_GetCaps(pPreparsedData, &caps);

				if (caps.UsagePage == HID_USAGE_PAGE_GENERIC &&
					(caps.Usage == HID_USAGE_GENERIC_JOYSTICK || caps.Usage == HID_USAGE_GENERIC_GAMEPAD))
				{
					std::wstring strPath(didData->DevicePath);
					std::transform(strPath.begin(), strPath.end(), strPath.begin(), ::toupper);
					joysDev.push_back(strPath);

					wchar_t str[MAX_PATH + 1];
					if (HidD_GetProductString(usbHandle, str, sizeof(str)))
						SendDlgItemMessageW(hW, IDC_COMBO_FFB, CB_ADDSTRING, 0, (LPARAM)str);
					else
					{
						swprintf_s(str, L"%04X:%04X", attr.VendorID, attr.ProductID);
						SendDlgItemMessageW(hW, IDC_COMBO_FFB, CB_ADDSTRING, 0, (LPARAM)str);
					}

					if (cfg->player_joys[0] == strPath)
					{
						SendDlgItemMessage(hW, IDC_COMBO_FFB, CB_SETCURSEL, sel_idx, 0);
						selectedJoy[0] = sel_idx;
					}
					else if (cfg->player_joys[1] == strPath)
					{
						selectedJoy[1] = sel_idx;
					}
					sel_idx++;
				}
				free(didData);
				HidD_FreePreparsedData(pPreparsedData);
				i++;
			}
			if (usbHandle != INVALID_HANDLE_VALUE)
				CloseHandle(usbHandle);
		}

		void populateMappings(HWND hW)
		{
			LVITEM lvItem;
			HWND lv = GetDlgItem(hW, IDC_LIST1);

			//LoadMappings(mapVector);

			memset(&lvItem, 0, sizeof(lvItem));

			lvItem.mask = LVIF_TEXT | LVIF_PARAM;
			lvItem.cchTextMax = 256;
			lvItem.iItem = 0;
			lvItem.iSubItem = 0;

			TCHAR tmp[256];
			int m[3];

			for (auto& it : mapVector)
			{
				//TODO feels a bit hacky
				bool isKB = (it.devName == TEXT("Keyboard"));
				if (isKB)
				{
					m[0] = PAD_BUTTON_COUNT;
					m[1] = PAD_AXIS_COUNT;
					m[2] = 4;
				}
				else
				{
					m[0] = MAX_BUTTONS;
					m[1] = MAX_AXES;
					m[2] = 4;
				}

				for (int i = 0; i < m[0]; i++)
				{
					//if((*it).btnMap[i] >= PAD_BUTTON_COUNT)
					//	continue;
					int btn = it.btnMap[i];
					int val = PLY_GET_VALUE(plyCapturing, btn);

					lvItem.iItem = ListView_GetItemCount(lv);
					if (PLY_IS_MAPPED(plyCapturing, btn) /*&& val < PAD_BUTTON_COUNT*/)
					{
						swprintf_s(tmp, 255, L"%s", it.devName.c_str()); //TODO
						lvItem.pszText = tmp;
						lvItem.lParam = (LPARAM) & (it.btnMap[i]);
						ListView_InsertItem(lv, &lvItem);
						swprintf_s(tmp, 255, L"P%d: Button %d", plyCapturing + 1, isKB ? val : i);
						ListView_SetItemText(lv, lvItem.iItem, 1, tmp);

						swprintf_s(tmp, 255, L"%S", isKB ? BTN2TXT[i] : BTN2TXT[val]);
						ListView_SetItemText(lv, lvItem.iItem, 2, tmp);
					}
				}

				for (int i = 0; i < m[1]; i++)
				{
					//if(it.axisMap[i] >= PAD_AXIS_COUNT)
					//	continue;
					int axis = it.axisMap[i];
					int val = PLY_GET_VALUE(plyCapturing, axis);

					if (PLY_IS_MAPPED(plyCapturing, axis) /* && val < PAD_AXIS_COUNT*/)
					{
						lvItem.iItem = ListView_GetItemCount(lv);
						//swprintf_s(tmp, 255, L"%s", it.devName.c_str()); //TODO
						lvItem.pszText = (LPWSTR)it.devName.c_str();
						lvItem.lParam = (LPARAM) & (it.axisMap[i]);
						ListView_InsertItem(lv, &lvItem);

						swprintf_s(tmp, 255, L"P%d: Axis %d", plyCapturing + 1, isKB ? val : i);
						ListView_SetItemText(lv, lvItem.iItem, 1, tmp);

						swprintf_s(tmp, 255, L"%S", isKB ? AXIS2TXT[i] : AXIS2TXT[val]);
						ListView_SetItemText(lv, lvItem.iItem, 2, tmp);
					}
				}

				for (int i = 0; i < m[2]; i++)
				{
					int hat = it.hatMap[i];
					int val = PLY_GET_VALUE(plyCapturing, hat);

					if (PLY_IS_MAPPED(plyCapturing, hat) /*&& val < PAD_HAT_COUNT*/)
					{
						lvItem.iItem = ListView_GetItemCount(lv);
						lvItem.pszText = (LPWSTR)it.devName.c_str();
						lvItem.lParam = (LPARAM) & (it.hatMap[i]);
						ListView_InsertItem(lv, &lvItem);

						swprintf_s(tmp, 255, L"P%d: Hat %d", plyCapturing + 1, isKB ? val : i);
						ListView_SetItemText(lv, lvItem.iItem, 1, tmp);

						swprintf_s(tmp, 255, L"Hat %d", isKB ? i : val);
						ListView_SetItemText(lv, lvItem.iItem, 2, tmp);
					}
				}
			}
		}

		static void ParseRawInput(PRAWINPUT pRawInput, HWND hW)
		{
			PHIDP_PREPARSED_DATA pPreparsedData = NULL;
			HIDP_CAPS Caps;
			PHIDP_BUTTON_CAPS pButtonCaps = NULL;
			PHIDP_VALUE_CAPS pValueCaps = NULL;
			USHORT capsLength;
			UINT bufferSize;
			USAGE usage[MAX_BUTTONS];
			ULONG i, usageLength, value;
			TCHAR name[1024] = {0};
			UINT nameSize = 1024;
			UINT pSize;
			RID_DEVICE_INFO devInfo;
			std::wstring devName;
			//DevInfo_t            mapDevInfo;
			Mappings* mapping = NULL;
			int axis;
			TCHAR buf[256];
			//std::pair<MappingsMap::iterator, bool> iter;

			//
			// Get the preparsed data block
			//
			GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);
			pSize = sizeof(devInfo);
			GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICEINFO, &devInfo, &pSize);

			if (devInfo.dwType == RIM_TYPEKEYBOARD)
			{
				devName = TEXT("Keyboard");
			}
			else
			{
				CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0);
				CHECK(pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize));
				CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);
				CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS);

				devName = name;
				std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);
			}

			for (auto& it : mapVector)
			{
				if (it.hidPath == devName)
				{
					mapping = &(it);
					break;
				}
			}

			if (mapping == NULL)
			{
				Mappings m; // = new Mappings;
				ZeroMemory(&m, sizeof(Mappings));
				mapVector.push_back(m);
				mapping = &mapVector.back();
				mapping->devName = devName;
				mapping->hidPath = devName;
			}
			//TODO get real dev name, probably from registry (see PAD Windows)
			if (!mapping->devName.length())
				mapping->devName = devName;

			if (devInfo.dwType == RIM_TYPEKEYBOARD &&
				(pRawInput->data.keyboard.Flags & RI_KEY_BREAK) != RI_KEY_BREAK)
			{
				if (pRawInput->data.keyboard.VKey == 0xff)
					return; //TODO
				if (pRawInput->data.keyboard.VKey == VK_ESCAPE)
				{
					resetState(hW);
					return;
				}
				if (btnCapturing < PAD_BUTTON_COUNT)
				{
					mapping->btnMap[btnCapturing] = PLY_SET_MAPPED(plyCapturing, pRawInput->data.keyboard.VKey);
					swprintf_s(buf, TEXT("Captured KB button %d"), pRawInput->data.keyboard.VKey);
					SendDlgItemMessage(dgHwnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
					btnCapturing = PAD_BUTTON_COUNT;
				}
				else if (hatCapturing < PAD_HAT_COUNT)
				{
					for (int h = 0; h < 4; h++)
					{
						if (HATS_8TO4[h] == hatCapturing)
							mapping->hatMap[h] = PLY_SET_MAPPED(plyCapturing, pRawInput->data.keyboard.VKey);
					}
					swprintf_s(buf, TEXT("Captured KB button %d"), pRawInput->data.keyboard.VKey);
					SendDlgItemMessage(dgHwnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
					hatCapturing = PAD_HAT_COUNT;
				}
				else if (axisCapturing < PAD_AXIS_COUNT)
				{
					mapping->axisMap[axisCapturing] = PLY_SET_MAPPED(plyCapturing, pRawInput->data.keyboard.VKey);
					swprintf_s(buf, TEXT("Captured KB button %d"), pRawInput->data.keyboard.VKey);
					SendDlgItemMessage(dgHwnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
					axisCapturing = PAD_AXIS_COUNT;
				}
			}
			else //if(devInfo.dwType == RIM_TYPEHID)
			{
				//Capture buttons
				if (btnCapturing < PAD_BUTTON_COUNT || hatCapturing < PAD_HAT_COUNT)
				{
					// Button caps
					CHECK(pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps));

					capsLength = Caps.NumberInputButtonCaps;
					CHECK(HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS);
					int numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;

					usageLength = numberOfButtons;
					CHECK(
						HidP_GetUsages(
							HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
							(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS);

					if (usageLength > 0) //Using first button only though
					{
						if (btnCapturing < PAD_BUTTON_COUNT)
						{
							mapping->btnMap[usage[0] - pButtonCaps->Range.UsageMin] = PLY_SET_MAPPED(plyCapturing, btnCapturing);
							btnCapturing = PAD_BUTTON_COUNT;
						}
						else if (hatCapturing < PAD_HAT_COUNT)
						{
							mapping->hatMap[usage[0] - pButtonCaps->Range.UsageMin] = PLY_SET_MAPPED(plyCapturing, hatCapturing);
							hatCapturing = PAD_HAT_COUNT;
						}
					}
				}
				else if (axisCapturing < PAD_AXIS_COUNT)
				{
					// Value caps
					CHECK(pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps));
					capsLength = Caps.NumberInputValueCaps;
					CHECK(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS);

					for (i = 0; i < Caps.NumberInputValueCaps && axisCapturing < PAD_AXIS_COUNT; i++)
					{
						CHECK(
							HidP_GetUsageValue(
								HidP_Input, pValueCaps[i].UsagePage, 0, pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
								(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS);

						uint32_t logical = pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin;

						switch (pValueCaps[i].Range.UsageMin)
						{
							case HID_USAGE_GENERIC_X:
							case HID_USAGE_GENERIC_Y:
							case HID_USAGE_GENERIC_Z:
							case HID_USAGE_GENERIC_RX:
							case HID_USAGE_GENERIC_RY:
							case HID_USAGE_GENERIC_RZ:
								axis = pValueCaps[i].Range.UsageMin - HID_USAGE_GENERIC_X;
								if (axisPass2)
								{
									if ((uint32_t)abs((int)(axisDiff[axis] - value)) > (logical >> 2))
									{
										mapping->axisMap[axis] = PLY_SET_MAPPED(plyCapturing, axisCapturing);
										axisPass2 = false;
										swprintf_s(buf, TEXT("Captured wheel axis %d"), axis);
										SendDlgItemMessage(dgHwnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
										axisCapturing = PAD_AXIS_COUNT;
										goto Error;
									}
									break;
								}
								axisDiff[axis] = value;
								break;

							case 0x39: // Hat Switch
								if (value < 0x8)
								{
									mapping->axisMap[6] = PLY_SET_MAPPED(plyCapturing, axisCapturing);
									axisPass2 = false;
									swprintf_s(buf, TEXT("Captured wheel hat switch"));
									SendDlgItemMessage(dgHwnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
									axisCapturing = PAD_AXIS_COUNT;
									goto Error;
								}
								break;
						}
					}

					axisPass2 = true;
				}
			}
		Error:
			SAFE_FREE(pPreparsedData);
			SAFE_FREE(pButtonCaps);
			SAFE_FREE(pValueCaps);
		}

		//Also when switching player
		void resetState(HWND hW)
		{
			SendDlgItemMessage(hW, IDC_COMBO_FFB, CB_SETCURSEL, selectedJoy[plyCapturing], 0);
			SendDlgItemMessage(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)TEXT(""));

			btnCapturing = PAD_BUTTON_COUNT;
			axisCapturing = PAD_AXIS_COUNT;
			hatCapturing = PAD_HAT_COUNT;
			ListView_DeleteAllItems(GetDlgItem(hW, IDC_LIST1));
			populateMappings(hW);
		}

		static void Register(HWND hW)
		{
			RAWINPUTDEVICE rid[3];
			rid[0].usUsagePage = 0x01;
			rid[0].usUsage = 0x05;
			rid[0].dwFlags = hW ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds game pad
			rid[0].hwndTarget = hW;

			rid[1].usUsagePage = 0x01;
			rid[1].usUsage = 0x04;
			rid[1].dwFlags = hW ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds joystick
			rid[1].hwndTarget = hW;

			rid[2].usUsagePage = 0x01;
			rid[2].usUsage = 0x06;
			rid[2].dwFlags = hW ? RIDEV_INPUTSINK /*| RIDEV_NOLEGACY*/ : RIDEV_REMOVE; // adds HID keyboard and also ignores legacy keyboard messages
			rid[2].hwndTarget = hW;

			if (!RegisterRawInputDevices(rid, 3, sizeof(rid[0])))
			{
				SendDlgItemMessage(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)TEXT("Could not register raw input devices."));
			}
		}

		INT_PTR CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{

			TCHAR buf[256];
			LVITEM lv;
			RawDlgConfig* cfg = (RawDlgConfig*)GetWindowLongPtr(hW, GWLP_USERDATA);
			int ret = 0;
			switch (uMsg)
			{
				case WM_INITDIALOG:
					if (!InitHid())
						return FALSE;
					dgHwnd = hW;
					SetWindowLongPtr(hW, GWLP_USERDATA, lParam);
					//SendDlgItemMessage(hW, IDC_BUILD_DATE, WM_SETTEXT, 0, (LPARAM)__DATE__ " " __TIME__);
					ListView_SetExtendedListViewStyle(GetDlgItem(hW, IDC_LIST1), LVS_EX_FULLROWSELECT);

					//LoadConfig();
					cfg = (RawDlgConfig*)lParam;

					if (!LoadSetting(cfg->dev_type, PLAYER_ONE_PORT, APINAME, N_JOYSTICK, cfg->player_joys[0]))
						cfg->player_joys[0].clear();

					if (!LoadSetting(cfg->dev_type, PLAYER_TWO_PORT, APINAME, N_JOYSTICK, cfg->player_joys[1]))
						cfg->player_joys[1].clear();

					LoadSetting(cfg->dev_type, PLAYER_ONE_PORT, APINAME, N_WHEEL_PT, cfg->pt[0]);
					LoadSetting(cfg->dev_type, PLAYER_TWO_PORT, APINAME, N_WHEEL_PT, cfg->pt[1]);

					Register(hW);
					LoadMappings(cfg->dev_type, mapVector);
					//if (conf.Log) CheckDlgButton(hW, IDC_LOGGING, TRUE);
					CheckDlgButton(hW, IDC_DFP_PASS, cfg->pt[0]);
					populate(hW, cfg);
					resetState(hW);
					return TRUE;

				case WM_INPUT:
					ret = S_FALSE;
					if (axisCapturing != PAD_AXIS_COUNT ||
						btnCapturing != PAD_BUTTON_COUNT ||
						hatCapturing != PAD_HAT_COUNT)
					{
						ret = 0;
						PRAWINPUT pRawInput;
						UINT bufferSize;
						UINT cbsize = sizeof(RAWINPUT);
						GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));
						pRawInput = (PRAWINPUT)malloc(bufferSize);
						if (!pRawInput)
							break;
						if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER)) > 0)
							ParseRawInput(pRawInput, hW);
						free(pRawInput);

						if (axisCapturing == PAD_AXIS_COUNT &&
							btnCapturing == PAD_BUTTON_COUNT &&
							hatCapturing == PAD_HAT_COUNT)
						{
							resetState(hW);
						}
					}
					DefWindowProc(hW, uMsg, wParam, lParam); //TODO should call for cleanup?
					return ret;

				case WM_KEYDOWN:
					if (LOWORD(lParam) == VK_ESCAPE)
					{
						resetState(hW);
						return TRUE;
					}
					break;
				case WM_NOTIFY:
					switch (((LPNMHDR)lParam)->code)
					{
						case TCN_SELCHANGE:
							switch (LOWORD(wParam))
							{
								case IDC_TAB1:
									plyCapturing = SendDlgItemMessage(hW, IDC_TAB1, TCM_GETCURSEL, 0, 0);
									CheckDlgButton(hW, IDC_DFP_PASS, cfg->pt[plyCapturing]);
									resetState(hW);
									break;
							}
							break;
					}
					break;
				case WM_TIMER:
					if (wParam == 1)
						resetState(hW);
					break;
				case WM_COMMAND:
					switch (HIWORD(wParam))
					{
						case LBN_SELCHANGE:
							switch (LOWORD(wParam))
							{
								case IDC_COMBO_FFB:
									selectedJoy[plyCapturing] = SendDlgItemMessage(hW, IDC_COMBO_FFB, CB_GETCURSEL, 0, 0);
									//player_joys[plyCapturing] = *(joysDev.begin() + selectedJoy[plyCapturing]);
									//resetState(hW);
									/*if(selectedJoy[plyCapturing] > 0 && selectedJoy[plyCapturing] == selectedJoy[1-plyCapturing])
					{
						selectedJoy[plyCapturing] = 0;
						resetState(hW);
						SendDlgItemMessage(hW, IDC_COMBO_FFB, CB_SETCURSEL, selectedJoy[plyCapturing], 0);
						//But would you want to?
						SendDlgItemMessageA(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)"Both players can't have the same controller."); //Actually, whatever, but config logics are limited ;P
					}*/
									break;
								case IDC_COMBO1:
									break;
								case IDC_COMBO2:
									break;
								default:
									return FALSE;
							}
							break;
						case BN_CLICKED:
							if (axisCapturing == PAD_AXIS_COUNT &&
								btnCapturing == PAD_BUTTON_COUNT &&
								LOWORD(wParam) >= IDC_BUTTON13 &&
								LOWORD(wParam) <= IDC_BUTTON16)
							{
								switch (LOWORD(wParam))
								{
									case IDC_BUTTON13:
										hatCapturing = PAD_HAT_N;
										break;
									case IDC_BUTTON14:
										hatCapturing = PAD_HAT_W;
										break;
									case IDC_BUTTON15:
										hatCapturing = PAD_HAT_E;
										break;
									case IDC_BUTTON16:
										hatCapturing = PAD_HAT_S;
										break;
								}
								MSG_PRESS_ESC(hW);
								SetTimer(hW, 1, 5000, nullptr);
							}

							switch (LOWORD(wParam))
							{
								case IDC_DFP_PASS:
									cfg->pt[plyCapturing] = IsDlgButtonChecked(hW, IDC_DFP_PASS) > 0;
									break;
								case IDC_UNBIND:
									int sel;
									HWND lhW;
									lhW = GetDlgItem(hW, IDC_LIST1);
									while (1)
									{
										ZeroMemory(&lv, sizeof(LVITEM));
										sel = ListView_GetNextItem(lhW, -1, LVNI_SELECTED);
										if (sel < 0)
											break;
										lv.iItem = sel;
										lv.mask = LVIF_PARAM;
										ListView_GetItem(lhW, &lv);
										ListView_DeleteItem(lhW, sel);
										ListView_EnsureVisible(lhW, sel, 0);

										int* map = (int*)lv.lParam;
										if (map)
											*map = PLY_UNSET_MAPPED(plyCapturing, *map);
									}
									resetState(hW);
									break;
								case IDC_BUTTON1:  //cross
								case IDC_BUTTON2:  //square
								case IDC_BUTTON3:  //circle
								case IDC_BUTTON4:  //triangle
								case IDC_BUTTON5:  //L1
								case IDC_BUTTON6:  //R1
								case IDC_BUTTON7:  //L2
								case IDC_BUTTON8:  //R2
								case IDC_BUTTON9:  //select
								case IDC_BUTTON10: //start
								case IDC_BUTTON11: //L3
								case IDC_BUTTON12: //R3
									if (axisCapturing == PAD_AXIS_COUNT &&
										hatCapturing == PAD_HAT_COUNT)
									{
										btnCapturing = (PS2Buttons)(LOWORD(wParam) - IDC_BUTTON1);
										MSG_PRESS_ESC(hW);
										SetTimer(hW, 1, 5000, nullptr);
									}
									return TRUE;
								case IDC_BUTTON17: //x
								case IDC_BUTTON18: //y
								case IDC_BUTTON19: //z
								case IDC_BUTTON20: //rz
								case IDC_BUTTON21: //hat
									if (btnCapturing == PAD_BUTTON_COUNT &&
										hatCapturing == PAD_HAT_COUNT)
									{
										axisCapturing = (PS2Axis)(LOWORD(wParam) - IDC_BUTTON17);
										swprintf_s(buf, TEXT("Capturing for axis %u, press ESC to cancel"), axisCapturing);
										SendDlgItemMessage(hW, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);
										SetTimer(hW, 1, 5000, nullptr);
									}
									return TRUE;
								case IDCANCEL:
									if (btnCapturing < PAD_BUTTON_COUNT ||
										axisCapturing < PAD_AXIS_COUNT ||
										hatCapturing < PAD_HAT_COUNT)
										return FALSE;
									Register(nullptr);
									EndDialog(hW, FALSE);
									return TRUE;
								case IDOK:
									Register(nullptr);
									cfg = (RawDlgConfig*)GetWindowLongPtr(hW, GWLP_USERDATA);
									cfg->player_joys[0] = joysDev[selectedJoy[0]];
									cfg->player_joys[1] = joysDev[selectedJoy[1]];

									INT_PTR res = RESULT_OK;

									if (!SaveSetting(cfg->dev_type, PLAYER_ONE_PORT, APINAME, N_JOYSTICK, cfg->player_joys[0]))
										res = RESULT_FAILED;
									if (!SaveSetting(cfg->dev_type, PLAYER_TWO_PORT, APINAME, N_JOYSTICK, cfg->player_joys[1]))
										res = RESULT_FAILED;

									SaveMappings(cfg->dev_type, mapVector);

									if (!SaveSetting(cfg->dev_type, PLAYER_ONE_PORT, APINAME, N_WHEEL_PT, cfg->pt[0]))
										res = RESULT_FAILED;
									if (!SaveSetting(cfg->dev_type, PLAYER_TWO_PORT, APINAME, N_WHEEL_PT, cfg->pt[1]))
										res = RESULT_FAILED;

									EndDialog(hW, res);
									return TRUE;
							}
					}
			}

			return S_OK; //DefWindowProc(hW, uMsg, wParam, lParam);
		}

	} // namespace raw
} // namespace usb_pad
