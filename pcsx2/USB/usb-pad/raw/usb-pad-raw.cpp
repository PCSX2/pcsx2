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
#include "usb-pad-raw.h"

namespace usb_pad
{
	namespace raw
	{

		void RawInputPad::WriterThread(void* ptr)
		{
			DWORD res = 0, res2 = 0, written = 0;
			std::array<uint8_t, 8> buf;

			RawInputPad* pad = static_cast<RawInputPad*>(ptr);
			pad->mWriterThreadIsRunning = true;

			while (pad->mUsbHandle != INVALID_HANDLE_VALUE)
			{
				if (pad->mFFData.wait_dequeue_timed(buf, std::chrono::milliseconds(1000)))
				{
					res = WriteFile(pad->mUsbHandle, buf.data(), buf.size(), &written, &pad->mOLWrite);
					uint8_t* d = buf.data();

					WaitForSingleObject(pad->mOLWrite.hEvent, 1000);
				}
			}

			pad->mWriterThreadIsRunning = false;
		}

		void RawInputPad::ReaderThread(void* ptr)
		{
			RawInputPad* pad = static_cast<RawInputPad*>(ptr);
			DWORD res = 0, res2 = 0, read = 0;
			std::array<uint8_t, 32> report; //32 is random

			pad->mReaderThreadIsRunning = true;
			int errCount = 0;

			while (pad->mUsbHandle != INVALID_HANDLE_VALUE)
			{
				if (GetOverlappedResult(pad->mUsbHandle, &pad->mOLRead, &read, FALSE))                                                                   // TODO check if previous read finally completed after WaitForSingleObject timed out
					ReadFile(pad->mUsbHandle, report.data(), std::min(pad->mCaps.InputReportByteLength, (USHORT)report.size()), nullptr, &pad->mOLRead); // Seems to only read data when input changes and not current state overall

				if (WaitForSingleObject(pad->mOLRead.hEvent, 1000) == WAIT_OBJECT_0)
				{
					if (!pad->mReportData.try_enqueue(report)) // TODO May leave queue with too stale data. Use multi-producer/consumer queue?
					{
						if (!errCount)
							Console.Warning("%s: Could not enqueue report data: %zd\n", APINAME, pad->mReportData.size_approx());
						errCount = (++errCount) % 16;
					}
				}
			}

			pad->mReaderThreadIsRunning = false;
		}

		int RawInputPad::TokenIn(uint8_t* buf, int len)
		{
			ULONG value = 0;
			int player = 1 - mPort;

			//Console.Warning("usb-pad: poll len=%li\n", len);
			if (mDoPassthrough)
			{
				std::array<uint8_t, 32> report; //32 is random
				if (mReportData.try_dequeue(report))
				{
					//ZeroMemory(buf, len);
					int size = std::min((int)mCaps.InputReportByteLength, len);
					memcpy(buf, report.data(), size);
					return size;
				}
				return 0;
			}

			//in case compiler magic with bitfields interferes
			wheel_data_t data_summed;
			memset(&data_summed, 0xFF, sizeof(data_summed));
			data_summed.hatswitch = 0x8;
			data_summed.buttons = 0;

			int copied = 0;
			//TODO fix the logics, also Config.cpp
			for (auto& it : mapVector)
			{

				if (data_summed.steering < it.data[player].steering)
				{
					data_summed.steering = it.data[player].steering;
					copied |= 1;
				}

				//if(data_summed.clutch < it.data[player].clutch)
				//	data_summed.clutch = it.data[player].clutch;

				if (data_summed.throttle < it.data[player].throttle)
				{
					data_summed.throttle = it.data[player].throttle;
					copied |= 2;
				}

				if (data_summed.brake < it.data[player].brake)
				{
					data_summed.brake = it.data[player].brake;
					copied |= 4;
				}

				data_summed.buttons |= it.data[player].buttons;
				if (data_summed.hatswitch > it.data[player].hatswitch)
					data_summed.hatswitch = it.data[player].hatswitch;
			}

			if (!copied)
				memcpy(&data_summed, &mDataCopy, sizeof(wheel_data_t));
			else
			{
				if (!(copied & 1))
					data_summed.steering = mDataCopy.steering;
				if (!(copied & 2))
					data_summed.throttle = mDataCopy.throttle;
				if (!(copied & 4))
					data_summed.brake = mDataCopy.brake;
			}

			pad_copy_data(mType, buf, data_summed);

			if (copied)
				memcpy(&mDataCopy, &data_summed, sizeof(wheel_data_t));
			return len;
		}

		int RawInputPad::TokenOut(const uint8_t* data, int len)
		{
			if (mUsbHandle == INVALID_HANDLE_VALUE)
				return 0;

			if (data[0] == 0x8 || data[0] == 0xB)
				return len;
			//if(data[0] == 0xF8 && data[1] == 0x5) sendCrap = true;
			if (data[0] == 0xF8 &&
				/* Allow range changes */
				!(data[1] == 0x81 || data[1] == 0x02 || data[1] == 0x03))
				return len; //don't send extended commands

			std::array<uint8_t, 8> report{0};

			//If i'm reading it correctly MOMO report size for output has Report Size(8) and Report Count(7), so that's 7 bytes
			//Now move that 7 bytes over by one and add report id of 0 (right?). Supposedly mandatory for HIDs.
			memcpy(report.data() + 1, data, report.size() - 1);

			if (!mFFData.enqueue(report))
			{
				return 0;
			}

			return len;
		}

		static void ParseRawInputHID(PRAWINPUT pRawInput, int subtype)
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
			USAGE usage[MAX_BUTTONS] = {0};
			Mappings* mapping = NULL;
			int numberOfButtons;

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

			CHECK(GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, NULL, &bufferSize) == 0);
			CHECK(pPreparsedData = (PHIDP_PREPARSED_DATA)malloc(bufferSize));
			CHECK((int)GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_PREPARSEDDATA, pPreparsedData, &bufferSize) >= 0);
			CHECK(HidP_GetCaps(pPreparsedData, &Caps) == HIDP_STATUS_SUCCESS);
			//pSize = sizeof(devInfo);
			//GetRawInputDeviceInfo(pRawInput->header.hDevice, RIDI_DEVICEINFO, &devInfo, &pSize);

			//Unset buttons/axes mapped to this device
			//pad_reset_data(&mapping->data[0]);
			//pad_reset_data(&mapping->data[1]);
			memset(&mapping->data[0], 0xFF, sizeof(wheel_data_t));
			memset(&mapping->data[1], 0xFF, sizeof(wheel_data_t));
			mapping->data[0].buttons = 0;
			mapping->data[1].buttons = 0;
			mapping->data[0].hatswitch = 0x8;
			mapping->data[1].hatswitch = 0x8;

			//Get pressed buttons
			CHECK(pButtonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS) * Caps.NumberInputButtonCaps));
			//If fails, maybe wheel only has axes
			capsLength = Caps.NumberInputButtonCaps;
			HidP_GetButtonCaps(HidP_Input, pButtonCaps, &capsLength, pPreparsedData);

			numberOfButtons = pButtonCaps->Range.UsageMax - pButtonCaps->Range.UsageMin + 1;
			usageLength = countof(usage);
			NTSTATUS stat;
			if ((stat = HidP_GetUsages(
					 HidP_Input, pButtonCaps->UsagePage, 0, usage, &usageLength, pPreparsedData,
					 (PCHAR)pRawInput->data.hid.bRawData, pRawInput->data.hid.dwSizeHid)) == HIDP_STATUS_SUCCESS)
			{
				for (uint32_t i = 0; i < usageLength; i++)
				{
					uint16_t btn = mapping->btnMap[usage[i] - pButtonCaps->Range.UsageMin];
					for (int j = 0; j < 2; j++)
					{
						PS2WheelTypes wt = (PS2WheelTypes)subtype;
						if (PLY_IS_MAPPED(j, btn))
						{
							uint32_t wtbtn = 1 << convert_wt_btn(wt, PLY_GET_VALUE(j, btn));
							mapping->data[j].buttons |= wtbtn;
						}
					}
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

					//Console.Warning("Min/max %d/%d\t", pValueCaps[i].LogicalMin, pValueCaps[i].LogicalMax);

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
							v = mapping->axisMap[pValueCaps[i].Range.UsageMin - HID_USAGE_GENERIC_X];
							break;
						case HID_USAGE_GENERIC_HATSWITCH:
							//Console.Warning("Hat: %02X\n", value);
							v = mapping->axisMap[6];
							break;
					}

					for (int j = 0; j < 2; j++)
					{
						if (!PLY_IS_MAPPED(j, v))
							continue;

						switch (PLY_GET_VALUE(j, v))
						{
							case PAD_AXIS_X: // X-axis
								//Console.Warning("X: %d\n", value);
								// Need for logical min too?
								//generic_data.axis_x = ((value - pValueCaps[i].LogicalMin) * 0x3FF) / (pValueCaps[i].LogicalMax - pValueCaps[i].LogicalMin);
								if (subtype == WT_DRIVING_FORCE_PRO || subtype == WT_DRIVING_FORCE_PRO_1102)
									mapping->data[j].steering = (value * 0x3FFF) / pValueCaps[i].LogicalMax;
								else
									//XXX Limit value range to 0..1023 if using 'generic' wheel descriptor
									mapping->data[j].steering = (value * 0x3FF) / pValueCaps[i].LogicalMax;
								break;

							case PAD_AXIS_Y: // Y-axis
								//XXX Limit value range to 0..255
								mapping->data[j].clutch = (value * 0xFF) / pValueCaps[i].LogicalMax;
								break;

							case PAD_AXIS_Z: // Z-axis
								//Console.Warning("Z: %d\n", value);
								//XXX Limit value range to 0..255
								mapping->data[j].throttle = (value * 0xFF) / pValueCaps[i].LogicalMax;
								break;

							case PAD_AXIS_RZ: // Rotate-Z
								//Console.Warning("Rz: %d\n", value);
								//XXX Limit value range to 0..255
								mapping->data[j].brake = (value * 0xFF) / pValueCaps[i].LogicalMax;
								break;

							case PAD_AXIS_HAT: // TODO Hat Switch
								//Console.Warning("Hat: %02X\n", value);
								//TODO 4 vs 8 direction hat switch
								if (pValueCaps[i].LogicalMax == 4 && value < 4)
									mapping->data[j].hatswitch = HATS_8TO4[value];
								else
									mapping->data[j].hatswitch = value;
								break;
						}
					}
				}
			}

		Error:
			SAFE_FREE(pPreparsedData);
			SAFE_FREE(pButtonCaps);
			SAFE_FREE(pValueCaps);
		}

		static void ParseRawInputKB(PRAWINPUT pRawInput, int subtype)
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

			for (uint32_t i = 0; i < PAD_BUTTON_COUNT; i++)
			{
				uint16_t btn = mapping->btnMap[i];
				for (int j = 0; j < 2; j++)
				{
					if (PLY_IS_MAPPED(j, btn))
					{
						PS2WheelTypes wt = (PS2WheelTypes) subtype;
						if (PLY_GET_VALUE(j, mapping->btnMap[i]) == pRawInput->data.keyboard.VKey)
						{
							uint32_t wtbtn = convert_wt_btn(wt, i);
							if (pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
								mapping->data[j].buttons &= ~(1 << wtbtn); //unset
							else                                           /* if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE) */
								mapping->data[j].buttons |= (1 << wtbtn);  //set
						}
					}
				}
			}

			for (uint32_t i = 0; i < 4 /*PAD_HAT_COUNT*/; i++)
			{
				uint16_t btn = mapping->hatMap[i];
				for (int j = 0; j < 2; j++)
				{
					if (PLY_IS_MAPPED(j, btn))
					{
						if (PLY_GET_VALUE(j, mapping->hatMap[i]) == pRawInput->data.keyboard.VKey)
							if (pRawInput->data.keyboard.Flags & RI_KEY_BREAK)
								mapping->data[j].hatswitch = 0x8;
							else //if(pRawInput->data.keyboard.Flags == RI_KEY_MAKE)
								mapping->data[j].hatswitch = HATS_8TO4[i];
					}
				}
			}
		}

		void RawInputPad::ParseRawInput(PRAWINPUT pRawInput)
		{
			if (pRawInput->header.dwType == RIM_TYPEKEYBOARD)
				ParseRawInputKB(pRawInput, Type());
			else if (pRawInput->header.dwType == RIM_TYPEHID)
				ParseRawInputHID(pRawInput, Type());
		}

		int RawInputPad::Open()
		{
			PHIDP_PREPARSED_DATA pPreparsedData = nullptr;
			HIDD_ATTRIBUTES attr;

			Close();

			mappings.clear();
			LoadMappings(mDevType, mapVector);

			memset(&mOLRead, 0, sizeof(OVERLAPPED));
			memset(&mOLWrite, 0, sizeof(OVERLAPPED));
			memset(&mDataCopy, 0xFF, sizeof(mDataCopy));

			mUsbHandle = INVALID_HANDLE_VALUE;
			std::wstring path;
			if (!LoadSetting(mDevType, mPort, APINAME, N_JOYSTICK, path))
				return 1;

			LoadSetting(mDevType, mPort, APINAME, N_WHEEL_PT, mDoPassthrough);

			mUsbHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
									 FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

			if (mUsbHandle != INVALID_HANDLE_VALUE)
			{
				mOLRead.hEvent = CreateEvent(0, 0, 0, 0);
				mOLWrite.hEvent = CreateEvent(0, 0, 0, 0);

				HidD_GetAttributes(mUsbHandle, &(attr));

				bool isClassicLogitech = (attr.VendorID == PAD_VID) && (attr.ProductID != 0xC262);
				bool isKeyboardmania = (attr.VendorID == 0x0507) && (attr.ProductID == 0x0010);
				if (!isClassicLogitech && !isKeyboardmania)
				{
					Console.Warning("USB: Vendor is not Logitech or wheel is G920. Not sending force feedback commands for safety reasons.\n");
					mDoPassthrough = 0;
					Close();
				}
				else if (!mWriterThreadIsRunning)
				{
					if (mWriterThread.joinable())
						mWriterThread.join();
					mWriterThread = std::thread(RawInputPad::WriterThread, this);
				}

				if (mDoPassthrough)
				{
					// for passthrough only
					HidD_GetPreparsedData(mUsbHandle, &pPreparsedData);
					HidP_GetCaps(pPreparsedData, &(mCaps));
					HidD_FreePreparsedData(pPreparsedData);

					if (!mReaderThreadIsRunning)
					{
						if (mReaderThread.joinable())
							mReaderThread.join();
						mReaderThread = std::thread(RawInputPad::ReaderThread, this);
					}
				}

				shared::rawinput::RegisterCallback(this);
				return 0;
			}
			else
				Console.Warning("USB: Could not open device '%s'.\nPassthrough and FFB will not work.\n", path.c_str());

			return 0;
		}

		int RawInputPad::Close()
		{
			if (mUsbHandle != INVALID_HANDLE_VALUE)
			{
				Reset();
				Sleep(100); // give WriterThread some time to write out Reset() commands
				CloseHandle(mUsbHandle);
				CloseHandle(mOLRead.hEvent);
				CloseHandle(mOLWrite.hEvent);
			}

			shared::rawinput::UnregisterCallback(this);
			mUsbHandle = INVALID_HANDLE_VALUE;
			return 0;
		}

// ---------
#include "raw-config-res.h"

		INT_PTR CALLBACK ConfigureRawDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
		int RawInputPad::Configure(int port, const char* dev_type, void* data)
		{
			Win32Handles* h = (Win32Handles*)data;
			INT_PTR res = RESULT_FAILED;
			if (shared::rawinput::Initialize(h->hWnd))
			{
				RawDlgConfig config(port, dev_type);
				res = DialogBoxParam(h->hInst, MAKEINTRESOURCE(IDD_RAWCONFIG), h->hWnd, ConfigureRawDlgProc, (LPARAM)&config);
				shared::rawinput::Uninitialize();
			}
			return (int)res;
		}

	} // namespace raw
} // namespace usb_pad
