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
#include "Global.h"
#include "HidDevice.h"

int FindHids(HidDeviceInfo** foundDevs, int vid, int pid)
{
	GUID GUID_DEVINTERFACE_HID;
	int numFoundDevs = 0;
	*foundDevs = 0;
	HidD_GetHidGuid(&GUID_DEVINTERFACE_HID);
	HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hdev != INVALID_HANDLE_VALUE)
	{
		SP_DEVICE_INTERFACE_DATA devInterfaceData;
		devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		for (int i = 0; SetupDiEnumDeviceInterfaces(hdev, 0, &GUID_DEVINTERFACE_HID, i, &devInterfaceData); i++)
		{

			DWORD size = 0;
			SetupDiGetDeviceInterfaceDetail(hdev, &devInterfaceData, 0, 0, &size, 0);
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size)
				continue;
			SP_DEVICE_INTERFACE_DETAIL_DATA* devInterfaceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(size);
			if (!devInterfaceDetails)
				continue;

			devInterfaceDetails->cbSize = sizeof(*devInterfaceDetails);
			SP_DEVINFO_DATA devInfoData;
			devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			if (!SetupDiGetDeviceInterfaceDetail(hdev, &devInterfaceData, devInterfaceDetails, size, &size, &devInfoData))
				continue;

			HANDLE hfile = CreateFile(devInterfaceDetails->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
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
								if (numFoundDevs % 32 == 0)
								{
									*foundDevs = (HidDeviceInfo*)realloc(*foundDevs, sizeof(HidDeviceInfo) * (32 + numFoundDevs));
								}
								HidDeviceInfo* dev = &foundDevs[0][numFoundDevs++];
								dev->caps = caps;
								dev->vid = attributes.VendorID;
								dev->pid = attributes.ProductID;
								dev->path = wcsdup(devInterfaceDetails->DevicePath);
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
	return numFoundDevs;
}
