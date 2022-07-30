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

// Used OBS as example

#include "PrecompiledHeader.h"
#include <windows.h>

#include <wx/fileconf.h>

#include "gui/AppConfig.h"

#include "USB/Win32/Config_usb.h"

#include <commctrl.h>
#include "usb-python2-raw.h"
#include "python2-config-raw-res.h"
#include "PAD/Windows/VKey.h"

#include <windowsx.h>

#define MSG_PRESS_ESC(wnd) SendDlgItemMessageW(wnd, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)L"Capturing, press ESC to cancel")

namespace usb_python2
{
	namespace raw
	{
		HWND dgHwnd2 = NULL;
		uint32_t axisDiff[32]; //previous axes values
		bool axisPass2 = false;
		uint32_t uniqueKeybindIdx = 0;

		std::wstring getKeyLabel(const KeyMapping key)
		{
			TCHAR tmp[256] = {0};

			if (key.bindType == 0)
				swprintf_s(tmp, 255, L"Button %d", key.value);
			else if (key.bindType == 1)
				swprintf_s(tmp, 255, L"%s Axis", axisLabelList[key.value]);
			else if (key.bindType == 2)
				swprintf_s(tmp, 255, L"Hat %d", key.value);
			else if (key.bindType == 3)
				swprintf_s(tmp, 255, L"%s", GetVKStringW(key.value));

			return std::wstring(tmp);
		}

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

			uniqueKeybindIdx = 0;

			WCHAR sec[1024] = {0}, bind[32] = {0};
			int v = 0;
			for (int j = 0; j < 25; j++)
			{
				std::wstring hid, tmp;

				if (LoadSetting(dev_type, j, "RAW DEVICE", TEXT("HID"), hid) && !hid.empty() && !MapExists(maps, hid))
				{
					Mappings m;
					ZeroMemory(&m, sizeof(Mappings));
					maps.push_back(m);
					Mappings& ptr = maps.back();

					ptr.hidPath = hid;
					ptr.devName = hid;

					for (uint32_t targetBind = 0; targetBind < buttonLabelList.size(); targetBind++)
					{
						if (LoadSetting(dev_type, j, "RAW DEVICE", buttonLabelList[targetBind], tmp))
						{
							while (tmp.size() > 0)
							{
								auto idx = tmp.find_first_of(L',');

								if (idx == std::wstring::npos)
									idx = tmp.size();

								auto substr = std::wstring(tmp.begin(), tmp.begin() + idx);

								uint32_t buttonType = 0;
								uint32_t value = 0;
								int isOneshot = 0;
								swscanf_s(substr.c_str(), L"%02X|%08X|%d", &buttonType, &value, &isOneshot);

								KeyMapping keybindMapping = {
									uniqueKeybindIdx++,
									targetBind,
									buttonType,
									value,
									isOneshot == 1};
								ptr.mappings.push_back(keybindMapping);

								if (idx + 1 < tmp.size())
									tmp.erase(tmp.begin(), tmp.begin() + idx + 1);
								else
									tmp.clear();
							}
						}
					}
				}
			}
			return;
		}

		void SaveMappings(const char* dev_type, MapVector& maps)
		{
			uint32_t numDevice = 0;

			for (int j = 0; j < 25; j++)
			{
				RemoveSection(dev_type, j, "RAW DEVICE");
			}

			for (auto& it : maps)
			{
				if (it.mappings.size() > 0)
				{
					SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", TEXT("HID"), it.hidPath);

					std::map<int, std::vector<KeyMapping>> mapByKeybind;

					for (auto& bindIt : it.mappings)
						mapByKeybind[bindIt.keybindId].push_back(bindIt);

					for (auto& bindIt : mapByKeybind)
					{
						std::wstring val;

						for (auto& keymap : bindIt.second)
						{
							WCHAR tmp[255] = {0};
							swprintf_s(tmp, 255, L"%02X|%08X|%d", keymap.bindType, keymap.value, keymap.isOneshot);

							if (val.size() > 0)
								val.append(L",");

							val.append(std::wstring(tmp));
						}

						SaveSetting<std::wstring>(dev_type, numDevice, "RAW DEVICE", buttonLabelList[bindIt.first], val.c_str());
					}
				}

				numDevice++;
			}

			return;
		}

		void populateListHeaders(HWND hWnd)
		{
			auto dlgHwd = GetDlgItem(hWnd, IDC_LIST_BOUND_KEYS);
			LVCOLUMN LvCol;
			memset(&LvCol, 0, sizeof(LvCol));
			LvCol.mask = LVCF_TEXT | LVCF_SUBITEM;
			LvCol.pszText = TEXT("Keybind Name");
			ListView_InsertColumn(dlgHwd, 0, &LvCol);
			LvCol.pszText = TEXT("Button");
			ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST_BOUND_KEYS), 1, &LvCol);
			LvCol.pszText = TEXT("Oneshot");
			ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST_BOUND_KEYS), 2, &LvCol);
			LvCol.pszText = TEXT("Device");
			ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST_BOUND_KEYS), 3, &LvCol);

			ListView_SetColumnWidth(dlgHwd, 0, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(dlgHwd, 1, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(dlgHwd, 2, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(dlgHwd, 3, LVSCW_AUTOSIZE_USEHEADER);
		}

		void populateMappingButtons(HWND hWnd)
		{
			auto listItem = GetDlgItem(hWnd, IDC_LIST_AVAILABLE_MAPPINGS);
			ListView_DeleteAllItems(listItem);

			LVCOLUMN LvCol;
			memset(&LvCol, 0, sizeof(LvCol));
			LvCol.mask = LVCF_TEXT | LVCF_SUBITEM;
			LvCol.pszText = TEXT("Keybind");
			ListView_InsertColumn(listItem, 0, &LvCol);

			for (size_t i = 0; i < buttonLabelList.size(); i++)
			{
				LVITEM lvItem;
				memset(&lvItem, 0, sizeof(lvItem));
				lvItem.mask = LVIF_TEXT | LVIF_PARAM;
				lvItem.cchTextMax = 256;
				lvItem.iItem = i;
				lvItem.iSubItem = 0;
				lvItem.pszText = buttonLabelList[i];
				lvItem.lParam = (LPARAM)i;
				ListView_InsertItem(listItem, &lvItem);
			}

			ListView_SetColumnWidth(listItem, 0, LVSCW_AUTOSIZE_USEHEADER);
		}

		void populateMappings(HWND hW)
		{
			auto lv = GetDlgItem(hW, IDC_LIST_BOUND_KEYS);
			LVITEM lvItem;
			memset(&lvItem, 0, sizeof(lvItem));

			ListView_DeleteAllItems(lv);

			lvItem.mask = LVIF_TEXT | LVIF_PARAM;
			lvItem.cchTextMax = 256;
			lvItem.iItem = 0;
			lvItem.iSubItem = 0;

			TCHAR tmp[256];
			for (size_t mapIdx = 0; mapIdx < mapVector.size(); mapIdx++)
			{
				for (auto &bindIt : mapVector[mapIdx].mappings)
				{
					lvItem.iItem = ListView_GetItemCount(lv);
					lvItem.pszText = buttonLabelList[bindIt.keybindId];
					lvItem.lParam = (mapIdx << 16) | bindIt.uniqueId;
					ListView_InsertItem(lv, &lvItem);

					swprintf_s(tmp, 255, L"%s", getKeyLabel(bindIt).c_str());
					ListView_SetItemText(lv, lvItem.iItem, 1, tmp);

					swprintf_s(tmp, 255, L"%s", bindIt.isOneshot ? L"On" : L"Off");
					ListView_SetItemText(lv, lvItem.iItem, 2, tmp);

					swprintf_s(tmp, 255, L"%s", mapVector[mapIdx].devName.c_str());
					ListView_SetItemText(lv, lvItem.iItem, 3, tmp);
				}
			}
		}

		static uint32_t getCurrentSelectedTargetBind(HWND hW)
		{
			auto lhW = GetDlgItem(hW, IDC_LIST_AVAILABLE_MAPPINGS);
			LVITEM lvItem;
			memset(&lvItem, 0, sizeof(lvItem));
			auto sel = ListView_GetNextItem(lhW, -1, LVNI_SELECTED);
			if (sel < 0)
				return -1;

			lvItem.iItem = sel;
			lvItem.mask = LVIF_PARAM;
			ListView_GetItem(lhW, &lvItem);
			return (uint32_t)lvItem.lParam;
		}

		bool captureButton = false;

		static void ParseRawInput(PRAWINPUT pRawInput, HWND hW)
		{
			PHIDP_PREPARSED_DATA pPreparsedData = NULL;
			HIDP_CAPS Caps;
			PHIDP_BUTTON_CAPS pButtonCaps = NULL;
			PHIDP_VALUE_CAPS pValueCaps = NULL;
			USHORT capsLength;
			UINT bufferSize;
			USAGE usage[32];
			ULONG usageLength;
			TCHAR name[1024] = {0};
			UINT nameSize = 1024;
			UINT pSize;
			RID_DEVICE_INFO devInfo;
			std::wstring devName;
			//DevInfo_t            mapDevInfo;
			Mappings* mapping = NULL;
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

			const auto targetBind = getCurrentSelectedTargetBind(hW);
			if (targetBind == -1)
			{
				// Nothing is actually selected, bail early
				captureButton = false;
				return;
			}

			bool isTargetKeybindOneshot = std::find(buttonDefaultOneshotList.begin(), buttonDefaultOneshotList.end(), buttonLabelList[targetBind]) != buttonDefaultOneshotList.end();

			if (devInfo.dwType == RIM_TYPEKEYBOARD &&
				(pRawInput->data.keyboard.Flags & RI_KEY_BREAK) != RI_KEY_BREAK)
			{
				if (pRawInput->data.keyboard.VKey == 0xff)
					return; //TODO
				if (pRawInput->data.keyboard.VKey == VK_ESCAPE)
				{
					//resetState(hW);
					return;
				}

				swprintf_s(buf, TEXT("Captured KB button %d"), pRawInput->data.keyboard.VKey);
				SendDlgItemMessage(dgHwnd2, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);

				KeyMapping keybindMapping = {
					uniqueKeybindIdx++,
					targetBind,
					KeybindType_Keyboard,
					pRawInput->data.keyboard.VKey,
					isTargetKeybindOneshot};
				mapping->mappings.push_back(keybindMapping);

				captureButton = false;
			}
			else if (devInfo.dwType != RIM_TYPEKEYBOARD)
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
					swprintf_s(buf, TEXT("Captured HID button %d"), usage[0]);
					SendDlgItemMessage(dgHwnd2, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);

					KeyMapping keybindMapping = {
						uniqueKeybindIdx++,
						targetBind,
						KeybindType_Button,
						usage[0],
						isTargetKeybindOneshot};
					mapping->mappings.push_back(keybindMapping);

					captureButton = false;
				}
				else
				{
					ULONG value = 0;

					// Value caps
					CHECK(pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps));
					capsLength = Caps.NumberInputValueCaps;
					CHECK(HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS);

					for (auto i = 0; i < Caps.NumberInputValueCaps; i++)
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
							{
								uint32_t axis = pValueCaps[i].Range.UsageMin - HID_USAGE_GENERIC_X;
								if (axisPass2)
								{
									if ((uint32_t)abs((int)(axisDiff[axis] - value)) > (logical >> 2))
									{
										axisPass2 = false;
										swprintf_s(buf, TEXT("Captured axis %d"), axis);
										SendDlgItemMessage(dgHwnd2, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);

										KeyMapping keybindMapping = {
											uniqueKeybindIdx++,
											targetBind,
											KeybindType_Axis,
											axis,
											isTargetKeybindOneshot};
										mapping->mappings.push_back(keybindMapping);
										captureButton = false;

										goto Error;
									}
									break;
								}
								axisDiff[axis] = value;
								break;
							}

							case 0x39: // Hat Switch
								if (value < 0x8)
								{
									axisPass2 = false;
									swprintf_s(buf, TEXT("Captured hat switch %d"), value);
									SendDlgItemMessage(dgHwnd2, IDC_STATIC_CAP, WM_SETTEXT, 0, (LPARAM)buf);

									KeyMapping keybindMapping = {
										uniqueKeybindIdx++,
										targetBind,
										KeybindType_Hat,
										value,
										isTargetKeybindOneshot};
									mapping->mappings.push_back(keybindMapping);
									captureButton = false;

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

		static void ResetState(HWND hW)
		{
			captureButton = false;
			populateMappings(hW);
		}

		INT_PTR CALLBACK ConfigurePython2DlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
				case WM_CREATE:
					SetWindowLongPtr(hW, GWLP_USERDATA, lParam);
					break;
				case WM_INITDIALOG:
				{
					if (!InitHid())
						return FALSE;

					dgHwnd2 = hW;
					SetWindowLongPtr(hW, GWLP_USERDATA, lParam);

					const Python2DlgConfig* cfg = (Python2DlgConfig*)lParam;

					LoadMappings(Python2Device::TypeName(), mapVector);

					SendDlgItemMessage(hW, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
					std::wstring selectedDevice;
					LoadSetting(Python2Device::TypeName(), cfg->port, APINAME, N_DEVICE, selectedDevice);
					for (auto i = 0; i != cfg->devList.size(); i++)
					{
						SendDlgItemMessageW(hW, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)cfg->devList[i].c_str());
						if (selectedDevice == cfg->devListGroups.at(i))
						{
							SendDlgItemMessage(hW, IDC_COMBO1, CB_SETCURSEL, i, i);
						}
					}

					Register(hW);
					populateListHeaders(hW);
					populateMappingButtons(hW);
					ResetState(hW);

					return TRUE;
				}
				case WM_COMMAND:
					if (HIWORD(wParam) == BN_CLICKED)
					{
						switch (LOWORD(wParam))
						{
							case IDOK:
							{
								INT_PTR res = RESULT_OK;

								const Python2DlgConfig* cfg = (Python2DlgConfig*)GetWindowLongPtr(hW, GWLP_USERDATA);

								// Save machine configuration selection
								auto deviceIdx = ComboBox_GetCurSel(GetDlgItem(hW, IDC_COMBO1));
								if (!SaveSetting<std::wstring>(Python2Device::TypeName(), cfg->port, APINAME, N_DEVICE, cfg->devListGroups[deviceIdx]))
									res = RESULT_FAILED;

								SaveMappings(Python2Device::TypeName(), mapVector);

								EndDialog(hW, res);
								return TRUE;
							}

							case IDCANCEL:
								EndDialog(hW, RESULT_CANCELED);
								return TRUE;

							case IDC_BIND:
								captureButton = true;
								MSG_PRESS_ESC(hW);
								SetTimer(hW, 1, 5000, nullptr);
								return TRUE;

							case IDC_ONESHOT:
							{
								int sel;
								HWND lhW;
								LVITEM lv;

								lhW = GetDlgItem(hW, IDC_LIST_BOUND_KEYS);
								while (1)
								{
									ZeroMemory(&lv, sizeof(LVITEM));
									auto nextSel = ListView_GetNextItem(lhW, -1, LVNI_SELECTED);

									if (nextSel < 0)
										break;

									sel = nextSel;

									lv.iItem = sel;
									lv.mask = LVIF_PARAM;
									ListView_GetItem(lhW, &lv);
									ListView_DeleteItem(lhW, sel);
									ListView_EnsureVisible(lhW, sel, 0);

									const size_t mapIdx = (lv.lParam >> 16) & 0xffff;
									const auto uniqueId = lv.lParam & 0xffff;
									if (mapIdx < mapVector.size())
									{
										for (auto& it : mapVector[mapIdx].mappings)
										{
											if (it.uniqueId == uniqueId)
											{
												it.isOneshot = !it.isOneshot;
											}
										}
									}
								}

								ResetState(hW);

								SetFocus(lhW);
								ListView_SetItemState(lhW, sel, LVIS_SELECTED, LVIS_SELECTED);
								ListView_EnsureVisible(lhW, sel, 1);

								break;
							}

							case IDC_UNBIND:
							{
								int sel;
								HWND lhW;
								lhW = GetDlgItem(hW, IDC_LIST_BOUND_KEYS);
								while (1)
								{
									LVITEM lv;
									ZeroMemory(&lv, sizeof(LVITEM));
									auto nextSel = ListView_GetNextItem(lhW, -1, LVNI_SELECTED);

									if (nextSel < 0)
										break;

									sel = nextSel;

									lv.iItem = sel;
									lv.mask = LVIF_PARAM;
									ListView_GetItem(lhW, &lv);
									ListView_DeleteItem(lhW, sel);
									ListView_EnsureVisible(lhW, sel, 0);

									const size_t mapIdx = (lv.lParam >> 16) & 0xffff;
									const auto uniqueId = lv.lParam & 0xffff;
									if (mapIdx < mapVector.size())
									{
										mapVector[mapIdx].mappings.erase(
											std::remove_if(
												mapVector[mapIdx].mappings.begin(),
												mapVector[mapIdx].mappings.end(),
												[uniqueId](KeyMapping& v) { return v.uniqueId == uniqueId; }),
											mapVector[mapIdx].mappings.end());
									}
								}

								ResetState(hW);

								if (sel - 1 >= 0)
									sel -= 1;
								else
									sel = 0;

								SetFocus(lhW);
								ListView_SetItemState(lhW, sel, LVIS_SELECTED, LVIS_SELECTED);
								ListView_EnsureVisible(lhW, sel, 1);

								break;
							}
						}
					}
					break;

				case WM_INPUT:
					auto ret = S_FALSE;
					if (captureButton)
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

						if (!captureButton)
							ResetState(hW);
					}
					DefWindowProc(hW, uMsg, wParam, lParam); //TODO should call for cleanup?
					return ret;
			}
			return FALSE;
		}
	} // namespace noop
} // namespace usb_python2
