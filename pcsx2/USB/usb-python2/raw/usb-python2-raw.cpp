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
#include "USB/USB.h"
#include "USB/Win32/Config_usb.h"
#include "usb-python2-raw.h"

#include <wx/fileconf.h>
#include "gui/AppConfig.h"
#include "gui/StringHelpers.h"
#include "USB/shared/inifile_usb.h"

#include "DEV9/DEV9.h"

namespace usb_python2
{
	namespace raw
	{
		std::wstring getKeyLabel(const KeyMapping key);

		uint32_t axisDiff2[32]; //previous axes values
		bool axisPass22 = false;

		static bool resetKeybinds = false;

		static void ParseRawInputHID(PRAWINPUT pRawInput)
		{
			PHIDP_PREPARSED_DATA pPreparsedData = NULL;
			HIDP_CAPS Caps = {0};
			PHIDP_BUTTON_CAPS pButtonCaps = NULL;
			PHIDP_VALUE_CAPS pValueCaps = NULL;
			UINT bufferSize = 0;
			ULONG usageLength, value;
			TCHAR name[1024] = {0};
			UINT nameSize = 1024;
			RID_DEVICE_INFO devInfo = {0};
			std::wstring devName;
			USHORT capsLength = 0;
			USAGE usage[32] = {0};
			Mappings* mapping = NULL;

			std::vector<uint32_t> usageCountButtons(countof(usage));
			std::vector<uint32_t> usageCountHats(8);
			std::map<wchar_t*, int> updatedInputState;

			auto iter = mappings.find(pRawInput->header.hDevice);
			if (iter != mappings.end())
			{
				mapping = iter->second;
			}
			else
			{
				GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);

				devName = name;
				std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);

				for (auto& it : mapVector)
				{
					if (it.hidPath == devName)
					{
						mapping = &it;
						mappings[pRawInput->header.hDevice] = mapping;
						break;
					}
				}
			}

			if (mapping == NULL)
				return;

			devName = mapping->hidPath;

			CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0);
			CHECK(pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize));
			CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);
			CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS);

			//Get pressed buttons
			CHECK(pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps));
			capsLength = Caps.NumberInputButtonCaps;
			HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData);

			uint32_t numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
			usageLength = countof(usage);

			if (HidP_GetUsages(
					 HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
					 (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS)
			{
				for (uint32_t i = 0; i < usageLength; i++)
				{
					usageCountButtons[usage[i] - pButtonCaps->Range.UsageMin + 1]++;
				}
			}

			/// Get axes' values
			CHECK(pValueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS) * Caps.NumberInputValueCaps));
			capsLength = Caps.NumberInputValueCaps;
			if (HidP_GetValueCaps(HidP_Input, pValueCaps, &capsLength, pPreparsedData) == HIDP_STATUS_SUCCESS)
			{
				for (USHORT i = 0; i < capsLength; i++)
				{
					if (HidP_GetUsageValue(
							HidP_Input, pValueCaps[i].UsagePage, 0,
							pValueCaps[i].Range.UsageMin, &value, pPreparsedData,
							(PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid) != HIDP_STATUS_SUCCESS)
					{
						continue; // if here then maybe something is up with HIDP_CAPS.NumberInputValueCaps
					}

					//Get mapped axis for physical axis
					uint16_t v = 0;
					switch (pValueCaps[i].Range.UsageMin)
					{
						case HID_USAGE_GENERIC_X: //0x30
						case HID_USAGE_GENERIC_Y:
						case HID_USAGE_GENERIC_Z:
						case HID_USAGE_GENERIC_RX:
						case HID_USAGE_GENERIC_RY:
						case HID_USAGE_GENERIC_RZ: //0x35
							v = pValueCaps[i].Range.UsageMin - HID_USAGE_GENERIC_X;
							for (auto& mappedKey : mapping->mappings)
							{
								if (mappedKey.bindType == KeybindType_Axis && mappedKey.value == v)
									currentInputStateAnalog[buttonLabelList[mappedKey.keybindId]] = (value - pValueCaps[i].LogicalMin) / double(pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin);
							}
							break;
						case HID_USAGE_GENERIC_HATSWITCH:
							for (size_t i = 0; i < usageCountHats.size(); i++)
								usageCountHats[i] = value == i;
							break;
					}

				}
			}

			for (auto& mappedKey : mapping->mappings)
			{
				// Update buttons
				for (size_t i = 0; i < usageCountButtons.size(); i++)
				{
					if (mappedKey.bindType == KeybindType_Button && mappedKey.value == i)
					{
						if (usageCountButtons[mappedKey.value] > 0)
						{
							if (!gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)])
								updatedInputState[buttonLabelList[mappedKey.keybindId]] = 1 | (mappedKey.isOneshot ? 0x80 : 0);

							gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] = true;
						}
						else
						{
							if (updatedInputState.find(buttonLabelList[mappedKey.keybindId]) == updatedInputState.end() || updatedInputState[buttonLabelList[mappedKey.keybindId]] == 0) // Only reset value if it wasn't set by a button already
								updatedInputState[buttonLabelList[mappedKey.keybindId]] = gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] ? 2 : 0;

							gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] = false;
						}
					}
				}

				// Update hats
				for (size_t i = 0; i < usageCountHats.size(); i++)
				{
					if (mappedKey.bindType == KeybindType_Hat && mappedKey.value == i)
					{
						if (usageCountHats[mappedKey.value] > 0)
						{
							if (!gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)])
								updatedInputState[buttonLabelList[mappedKey.keybindId]] = 1 | (mappedKey.isOneshot ? 0x80 : 0);

							gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] = true;
						}
						else
						{
							if (updatedInputState.find(buttonLabelList[mappedKey.keybindId]) == updatedInputState.end() || updatedInputState[buttonLabelList[mappedKey.keybindId]] == 0) // Only reset value if it wasn't set by a button already
								updatedInputState[buttonLabelList[mappedKey.keybindId]] = gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] ? 2 : 0;

							gamepadButtonIsPressed[devName][mappedKey.value | (mappedKey.bindType << 28)] = false;
						}
					}
				}
			}

			for (auto& k : updatedInputState)
			{
				currentInputStatePad[k.first] = k.second;

				if ((k.second & 3) == 1)
				{
					keyStateUpdates[k.first].push_back({wxDateTime::UNow(), true});

					if (k.second & 0x80) // Oneshot
						keyStateUpdates[k.first].push_back({wxDateTime::UNow(), false});
				}
				else if ((k.second & 3) == 2)
				{
					keyStateUpdates[k.first].push_back({wxDateTime::UNow(), false});
				}
			}

		Error:
			SAFE_FREE(pPreparsedData);
			SAFE_FREE(pButtonCaps);
			SAFE_FREE(pValueCaps);
		}

		static void ParseRawInputKB(PRAWINPUT pRawInput)
		{
			Mappings* mapping = nullptr;

			for (auto& it : mapVector)
			{
				if (!it.hidPath.compare(TEXT("Keyboard")))
				{
					mapping = &it;
					break;
				}
			}

			if (mapping == NULL)
				return;

			for (auto& mappedKey : mapping->mappings)
			{
				if (mappedKey.bindType == KeybindType_Keyboard && mappedKey.value == pRawInput->data.keyboard.VKey)
				{
					// Alternatively, keep a counter for how many keys are pressing the keybind at once
					if (pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
					{
						if (!mappedKey.isOneshot)
							keyStateUpdates[buttonLabelList[mappedKey.keybindId]].push_back({wxDateTime::UNow(), false});
					}
					else
					{
						if (!keyboardButtonIsPressed[pRawInput->data.keyboard.VKey])
						{
							keyStateUpdates[buttonLabelList[mappedKey.keybindId]].push_back({wxDateTime::UNow(), true});

							if (mappedKey.isOneshot)
								keyStateUpdates[buttonLabelList[mappedKey.keybindId]].push_back({wxDateTime::UNow(), false});
						}
					}
				}
			}

			keyboardButtonIsPressed[pRawInput->data.keyboard.VKey] = (pRawInput->data.keyboard.Flags & RI_KEY_BREAK) == 0;
		}

		void RawInputPad::ParseRawInput(PRAWINPUT pRawInput)
		{
			if (pRawInput->header.dwType == RIM_TYPEKEYBOARD)
				ParseRawInputKB(pRawInput);
			else if (pRawInput->header.dwType == RIM_TYPEHID)
				ParseRawInputHID(pRawInput);
		}

		int RawInputPad::Open()
		{
			PHIDP_PREPARSED_DATA pPreparsedData = nullptr;

			Close();

			mappings.clear();
			currentInputStateKeyboard.clear();
			currentInputStatePad.clear();
			currentInputStateAnalog.clear();
			currentKeyStates.clear();
			keyStateUpdates.clear();

			keyboardButtonIsPressed.clear();
			gamepadButtonIsPressed.clear();

			LoadMappings(mDevType, mapVector);

			std::wstring selectedDevice;
			LoadSetting(Python2Device::TypeName(), mPort, APINAME, N_DEVICE, selectedDevice);

			shared::rawinput::RegisterCallback(this);
			return 0;
		}

		int RawInputPad::Close()
		{
			shared::rawinput::UnregisterCallback(this);
			return 0;
		}

		void RawInputPad::UpdateKeyStates(std::wstring keybind)
		{
			const auto currentTimestamp = wxDateTime::UNow();
			while (keyStateUpdates[keybind].size() > 0)
			{
				auto curState = keyStateUpdates[keybind].front();
				keyStateUpdates[keybind].pop_front();

				// Remove stale inputs that occur during times where the game can't query for inputs.
				// The timeout value is based on what felt ok to me so just adjust as needed.
				const auto timestampDiff = currentTimestamp - curState.timestamp;
				if (timestampDiff.GetMilliseconds() > 150)
				{
					//Console.WriteLn(L"Dropping delayed input... %s %ld ms late", keybind, timestampDiff);
					continue;
				}

				//Console.WriteLn(L"Keystate update %s %d", keybind, curState.state);

				currentKeyStates[keybind] = curState.state;

				break;
			}
		}

		bool RawInputPad::GetKeyState(std::wstring keybind)
		{
			UpdateKeyStates(keybind);

			auto it = currentKeyStates.find(keybind);
			if (it != currentKeyStates.end())
				return it->second;

			return false;
		}

		bool RawInputPad::GetKeyStateOneShot(std::wstring keybind)
		{
			UpdateKeyStates(keybind);

			auto isPressed = false;
			auto it = currentKeyStates.find(keybind);
			if (it != currentKeyStates.end())
			{
				isPressed = it->second;
				it->second = false;
			}

			return isPressed;
		}

		double RawInputPad::GetKeyStateAnalog(std::wstring keybind)
		{
			const auto it = currentInputStateAnalog.find(keybind);
			if (it == currentInputStateAnalog.end())
				return 0;
			return it->second;
		}

		bool RawInputPad::IsKeybindAvailable(std::wstring keybind)
		{
			return currentInputStateKeyboard.find(keybind) != currentInputStateKeyboard.end() || currentInputStatePad.find(keybind) != currentInputStatePad.end();
		}

		bool RawInputPad::IsAnalogKeybindAvailable(std::wstring keybind)
		{
			return currentInputStateAnalog.find(keybind) != currentInputStateAnalog.end();
		}

// ---------
#include "python2-config-raw-res.h"

		INT_PTR CALLBACK ConfigurePython2DlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
		int RawInputPad::Configure(int port, const char* dev_type, void* data)
		{
			Win32Handles* h = (Win32Handles*)data;
			INT_PTR res = RESULT_FAILED;
			if (shared::rawinput::Initialize(h->hWnd))
			{
				std::vector<std::wstring> devList;
				std::vector<std::wstring> devListGroups;

				const wxString iniPath = StringUtil::UTF8StringToWxString(Path::Combine(EmuFolders::Settings, "Python2.ini"));
				CIniFile ciniFile;

				if (!ciniFile.Load(iniPath.ToStdWstring()))
					return (int)res;

				auto sections = ciniFile.GetSections();
				for (auto itr = sections.begin(); itr != sections.end(); itr++)
				{
					auto groupName = (*itr)->GetSectionName();
					if (groupName.find(L"GameEntry ") == 0)
					{
						devListGroups.push_back(groupName);

						auto gameName = (*itr)->GetKeyValue(L"Name");
						if (!gameName.empty())
							devList.push_back(gameName);
					}
				}

				Python2DlgConfig config(port, dev_type, devList, devListGroups);
				res = DialogBoxParam(h->hInst, MAKEINTRESOURCE(IDD_PYTHON2CONFIG), h->hWnd, ConfigurePython2DlgProc, (LPARAM)&config);
				shared::rawinput::Uninitialize();
			}
			return (int)res;
		}

	} // namespace raw
} // namespace usb_pad
