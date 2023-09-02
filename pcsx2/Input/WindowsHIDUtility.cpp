/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "Input/WindowsHIDUtility.h"

namespace WindowsHIDUtility
{
	std::vector<HidDeviceInfo> FindHids(u16 vid, u16 pid)
	{
		std::vector<HidDeviceInfo> foundDevs;

		GUID GUID_DEVINTERFACE_HID;
		HidD_GetHidGuid(&GUID_DEVINTERFACE_HID);

		HDEVINFO hdev = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_HID, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (hdev != INVALID_HANDLE_VALUE)
		{
			SP_DEVICE_INTERFACE_DATA devInterfaceData;
			devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
			for (int i = 0; SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_DEVINTERFACE_HID, i, &devInterfaceData); ++i)
			{
				DWORD size = 0;
				SetupDiGetDeviceInterfaceDetailW(hdev, &devInterfaceData, 0, 0, &size, 0);
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size)
					continue;

				SP_DEVICE_INTERFACE_DETAIL_DATA_W* devInterfaceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(size);

				if (!devInterfaceDetails)
					continue;

				devInterfaceDetails->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

				SP_DEVINFO_DATA devInfoData;
				devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

				if (!SetupDiGetDeviceInterfaceDetailW(hdev, &devInterfaceData, devInterfaceDetails, size, &size, &devInfoData))
					continue;

				HANDLE hfile = CreateFileW(devInterfaceDetails->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
				if (hfile != INVALID_HANDLE_VALUE)
				{
					HIDD_ATTRIBUTES attributes;
					attributes.Size = sizeof(attributes);
					if (HidD_GetAttributes(hfile, &attributes))
					{
						if (attributes.VendorID == vid && attributes.ProductID == pid)
						{
							PHIDP_PREPARSED_DATA pData;
							HIDP_CAPS caps;
							if (HidD_GetPreparsedData(hfile, &pData))
							{
								if (HidP_GetCaps(pData, &caps) == HIDP_STATUS_SUCCESS)
								{
									foundDevs.push_back({caps, attributes.VendorID, attributes.ProductID, devInterfaceDetails->DevicePath});
								}
								HidD_FreePreparsedData(pData);
							}
						}
					}
					CloseHandle(hfile);
				}
				free(devInterfaceDetails);
			}
			SetupDiDestroyDeviceInfoList(hdev);
		}
		return foundDevs;
	}

} // namespace WindowsHIDUtility