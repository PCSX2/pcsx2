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
#include "rawinput.h"
#include "USB/Win32/Config_usb.h"
#include "USB/qemu-usb/input-keymap.h"
#include "USB/qemu-usb/input-keymap-win32-to-qcode.h"

namespace usb_hid
{
	namespace raw
	{

#define CHECK(exp)      \
	do                  \
	{                   \
		if (!(exp))     \
			goto Error; \
	} while (0)
#define SAFE_FREE(p)    \
	do                  \
	{                   \
		if (p)          \
		{               \
			free(p);    \
			(p) = NULL; \
		}               \
	} while (0)

		// VKEY from https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
		// and convert to HID usage id from "10 Keyboard/Keypad Page (0x07)"
		// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

		int RawInput::TokenOut(const uint8_t* data, int len)
		{
			//	std::array<uint8_t, 8> report{ 0 };
			//	memcpy(report.data() + 1, data, report.size() - 1);

			return len;
		}

		static void ParseRawInput(PRAWINPUT pRawInput, HIDState* hs)
		{
			PHIDP_PREPARSED_DATA pPreparsedData = NULL;
			HIDP_CAPS Caps;
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
			int numberOfButtons;

			GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICENAME, name, &nameSize);

			devName = name;
			std::transform(devName.begin(), devName.end(), devName.begin(), ::toupper);

			CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0);
			CHECK(pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize));
			CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);
			CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS);

			//Get pressed buttons
			CHECK(pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps));
			//If fails, maybe wheel only has axes
			capsLength = Caps.NumberInputButtonCaps;
			HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData);

			numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
			usageLength = countof(usage); //numberOfButtons;

			NTSTATUS stat;
			if ((stat = HidP_GetUsages(
					 HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
					 (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid)) == HIDP_STATUS_SUCCESS)
			{
				for (uint32_t i = 0; i < usageLength; i++)
				{
					uint16_t btn = usage[i] - pButtonCaps->Range.UsageMin;
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

					switch (pValueCaps[i].Range.UsageMin)
					{
						case HID_USAGE_GENERIC_X: //0x30
						case HID_USAGE_GENERIC_Y:
						case HID_USAGE_GENERIC_Z:
						case HID_USAGE_GENERIC_RX:
						case HID_USAGE_GENERIC_RY:
						case HID_USAGE_GENERIC_RZ: //0x35
							//int axis = (value * 0x3FFF) / pValueCaps[i].LogicalMax;
							break;
						case HID_USAGE_GENERIC_HATSWITCH:
							//Console.Warning("Hat: %02X\n", value);
							break;
					}
				}
			}

		Error:
			SAFE_FREE(pPreparsedData);
			SAFE_FREE(pButtonCaps);
			SAFE_FREE(pValueCaps);
		}

		static void ParseRawInputKB(RAWKEYBOARD& k, HIDState* hs)
		{
			if (hs->kind != HID_KEYBOARD || !hs->kbd.eh_entry)
				return;

			if (KEYBOARD_OVERRUN_MAKE_CODE == k.MakeCode)
				return;

			QKeyCode qcode = Q_KEY_CODE_UNMAPPED;
			if (k.VKey < (int)qemu_input_map_win32_to_qcode.size())
				qcode = qemu_input_map_win32_to_qcode[k.VKey];

			//TODO
			if (k.Flags & RI_KEY_E0)
			{
				if (Q_KEY_CODE_SHIFT == qcode)
					qcode = Q_KEY_CODE_SHIFT_R;
				else if (Q_KEY_CODE_CTRL == qcode)
					qcode = Q_KEY_CODE_CTRL_R;
				else if (Q_KEY_CODE_ALT == qcode)
					qcode = Q_KEY_CODE_ALT_R;
			}

			InputEvent ev{};
			ev.type = INPUT_EVENT_KIND_KEY;
			ev.u.key.down = !(k.Flags & RI_KEY_BREAK);
			ev.u.key.key.type = KEY_VALUE_KIND_QCODE;
			ev.u.key.key.u.qcode = qcode;

			hs->kbd.eh_entry(hs, &ev);
		}

		static void SendPointerEvent(InputEvent& ev, HIDState* hs)
		{
			hs->ptr.eh_entry(hs, &ev);
		}

		static void ParseRawInputMS(RAWMOUSE& m, HIDState* hs)
		{
			if (!hs->ptr.eh_entry || (hs->kind != HID_MOUSE && hs->kind != HID_TABLET))
				return;

			int z = 0;
			InputEvent ev{};

			if (m.usButtonFlags & RI_MOUSE_WHEEL)
				z = (short)m.usButtonData / WHEEL_DELTA;

			ev.type = INPUT_EVENT_KIND_BTN;

			if (m.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN)
			{
				ev.u.btn.button = INPUT_BUTTON_LEFT;
				ev.u.btn.down = true;
				SendPointerEvent(ev, hs);
			}
			if (m.ulButtons & RI_MOUSE_LEFT_BUTTON_UP)
			{
				ev.u.btn.button = INPUT_BUTTON_LEFT;
				ev.u.btn.down = false;
				SendPointerEvent(ev, hs);
			}

			if (m.ulButtons & RI_MOUSE_RIGHT_BUTTON_DOWN)
			{
				ev.u.btn.button = INPUT_BUTTON_RIGHT;
				ev.u.btn.down = true;
				SendPointerEvent(ev, hs);
			}
			if (m.ulButtons & RI_MOUSE_RIGHT_BUTTON_UP)
			{
				ev.u.btn.button = INPUT_BUTTON_RIGHT;
				ev.u.btn.down = false;
				SendPointerEvent(ev, hs);
			}

			if (m.ulButtons & RI_MOUSE_MIDDLE_BUTTON_DOWN)
			{
				ev.u.btn.button = INPUT_BUTTON_MIDDLE;
				ev.u.btn.down = true;
				SendPointerEvent(ev, hs);
			}
			if (m.ulButtons & RI_MOUSE_MIDDLE_BUTTON_UP)
			{
				ev.u.btn.button = INPUT_BUTTON_MIDDLE;
				ev.u.btn.down = false;
				SendPointerEvent(ev, hs);
			}

			if (z != 0)
			{
				ev.u.btn.button = (z < 0) ? INPUT_BUTTON_WHEEL_DOWN : INPUT_BUTTON_WHEEL_UP;
				for (int i = 0; i < z; i++)
				{
					ev.u.btn.down = true;
					SendPointerEvent(ev, hs);
					ev.u.btn.down = false; // TODO needs an UP event?
					SendPointerEvent(ev, hs);
				}
			}

			if (m.usFlags & MOUSE_MOVE_ABSOLUTE)
			{
				/*ev.type = INPUT_EVENT_KIND_ABS;
				ev.u.abs.axis = INPUT_AXIS_X;
				ev.u.abs.value = m.lLastX;
				SendPointerEvent(ev, hs);

				ev.u.abs.axis = INPUT_AXIS_Y;
				ev.u.abs.value = m.lLastY;
				SendPointerEvent(ev, hs);*/
			}
			else
			{
				ev.type = INPUT_EVENT_KIND_REL;
				ev.u.rel.axis = INPUT_AXIS_X;
				ev.u.rel.value = m.lLastX;
				SendPointerEvent(ev, hs);

				ev.u.rel.axis = INPUT_AXIS_Y;
				ev.u.rel.value = m.lLastY;
				SendPointerEvent(ev, hs);
			}


			if (hs->ptr.eh_sync)
				hs->ptr.eh_sync(hs);
		}

		void RawInput::ParseRawInput(PRAWINPUT pRawInput)
		{
			if (pRawInput->header.dwType == RIM_TYPEKEYBOARD)
				ParseRawInputKB(pRawInput->data.keyboard, mHIDState);
			else if (pRawInput->header.dwType == RIM_TYPEMOUSE)
				ParseRawInputMS(pRawInput->data.mouse, mHIDState);
			//	else
			//		ParseRawInput(pRawInput, mHIDState);
		}

		int RawInput::Open()
		{
			Close();
			shared::rawinput::RegisterCallback(this);
			return 0;
		}

		int RawInput::Close()
		{
			shared::rawinput::UnregisterCallback(this);
			Reset();
			return 0;
		}

		int RawInput::Configure(int port, const char* dev_type, HIDType type, void* data)
		{
			Win32Handles* h = (Win32Handles*)data;
			INT_PTR res = RESULT_CANCELED;
			return res;
		}

	} // namespace raw
} // namespace usb_hid
