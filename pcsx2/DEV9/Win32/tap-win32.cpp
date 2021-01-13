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
#include <windows.h>
#include <tchar.h>
#include "tap.h"
#include "..\dev9.h"
#include <string>

//=============
// TAP IOCTLs
//=============

#define TAP_CONTROL_CODE(request, method) \
	CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

// clang-format off
#define TAP_IOCTL_GET_MAC               TAP_CONTROL_CODE(1, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION           TAP_CONTROL_CODE(2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU               TAP_CONTROL_CODE(3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO              TAP_CONTROL_CODE(4, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE(5, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS      TAP_CONTROL_CODE(6, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ      TAP_CONTROL_CODE(7, METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE          TAP_CONTROL_CODE(8, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT   TAP_CONTROL_CODE(9, METHOD_BUFFERED)
// clang-format on

//=================
// Registry keys
//=================

#define ADAPTER_KEY L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

#define NETWORK_CONNECTIONS_KEY L"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

//======================
// Filesystem prefixes
//======================

#define USERMODEDEVICEDIR "\\\\.\\Global\\"
#define TAPSUFFIX ".tap"

#define TAP_COMPONENT_ID "tap0901"

bool IsTAPDevice(const TCHAR* guid)
{
	HKEY netcard_key;
	LONG status;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &netcard_key);

	if (status != ERROR_SUCCESS)
		return false;

	for (;;)
	{
		TCHAR enum_name[256];
		TCHAR unit_string[256];
		HKEY unit_key;
		TCHAR component_id_string[] = _T("ComponentId");
		TCHAR component_id[256];
		TCHAR net_cfg_instance_id_string[] = _T("NetCfgInstanceId");
		TCHAR net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof(enum_name);
		status = RegEnumKeyEx(netcard_key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		_sntprintf(unit_string, sizeof(unit_string), _T("%s\\%s"), ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key);

		if (status != ERROR_SUCCESS)
		{
			return false;
		}
		else
		{
			len = sizeof(component_id);
			status = RegQueryValueEx(unit_key, component_id_string, nullptr, &data_type,
									 (LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, nullptr, &data_type,
										 (LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					// tap_ovpnconnect, tap0901 or root\tap, no clue why
					if ((!wcsncmp(component_id, L"tap", 3) || !wcsncmp(component_id, L"root\\tap", 8)) && !_tcscmp(net_cfg_instance_id, guid))
					{
						RegCloseKey(unit_key);
						RegCloseKey(netcard_key);
						return true;
					}
				}
			}
			RegCloseKey(unit_key);
		}
		++i;
	}

	RegCloseKey(netcard_key);
	return false;
}

std::vector<AdapterEntry> TAPAdapter::GetAdapters()
{
	std::vector<AdapterEntry> tap_nic;
	LONG status;
	HKEY control_net_key;
	DWORD len;
	DWORD cSubKeys = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ | KEY_QUERY_VALUE,
						  &control_net_key);

	if (status != ERROR_SUCCESS)
		return tap_nic;

	status = RegQueryInfoKey(control_net_key, nullptr, nullptr, nullptr, &cSubKeys, nullptr, nullptr,
							 nullptr, nullptr, nullptr, nullptr, nullptr);

	if (status != ERROR_SUCCESS)
		return tap_nic;

	for (DWORD i = 0; i < cSubKeys; i++)
	{
		TCHAR enum_name[256];
		TCHAR connection_string[256];
		HKEY connection_key;
		TCHAR name_data[256];
		DWORD name_type;
		const TCHAR name_string[] = _T("Name");

		len = sizeof(enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status != ERROR_SUCCESS)
			continue;

		_sntprintf(connection_string, sizeof(connection_string), _T("%s\\%s\\Connection"),
				   NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string, 0, KEY_READ, &connection_key);

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key, name_string, nullptr, &name_type, (LPBYTE)name_data,
									 &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				continue;
			}
			else
			{
				if (IsTAPDevice(enum_name))
				{
					AdapterEntry t;
					t.type = NetApi::TAP;
					t.name = std::wstring(name_data);
					t.guid = std::wstring(enum_name);
					tap_nic.push_back(t);
				}
			}

			RegCloseKey(connection_key);
		}
	}

	RegCloseKey(control_net_key);

	return tap_nic;
}

static int TAPGetMACAddress(HANDLE handle, u8* addr)
{
	DWORD len = 0;

	return DeviceIoControl(handle, TAP_IOCTL_GET_MAC,
						   addr, 6,
						   addr, 6, &len, NULL);
}

//Set the connection status
static int TAPSetStatus(HANDLE handle, int status)
{
	DWORD len = 0;

	return DeviceIoControl(handle, TAP_IOCTL_SET_MEDIA_STATUS,
						   &status, sizeof(status),
						   &status, sizeof(status), &len, NULL);
}
//Open the TAP adapter and set the connection to enabled :)
HANDLE TAPOpen(const char* device_guid)
{
	char device_path[256];

	struct
	{
		unsigned long major;
		unsigned long minor;
		unsigned long debug;
	} version;
	LONG version_len;

	_snprintf(device_path, sizeof(device_path), "%s%s%s",
			  USERMODEDEVICEDIR,
			  device_guid,
			  TAPSUFFIX);

	HANDLE handle = CreateFileA(
		device_path,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		0);

	if (handle == INVALID_HANDLE_VALUE)
	{
		return INVALID_HANDLE_VALUE;
	}

	BOOL bret = DeviceIoControl(handle, TAP_IOCTL_GET_VERSION,
								&version, sizeof(version),
								&version, sizeof(version), (LPDWORD)&version_len, NULL);

	if (bret == FALSE)
	{
		CloseHandle(handle);
		return INVALID_HANDLE_VALUE;
	}

	if (!TAPSetStatus(handle, TRUE))
	{
		return INVALID_HANDLE_VALUE;
	}

	return handle;
}



TAPAdapter::TAPAdapter()
	: NetAdapter()
{
	if (config.ethEnable == 0)
		return;
	htap = TAPOpen(config.Eth);

	read.Offset = 0;
	read.OffsetHigh = 0;
	read.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	write.Offset = 0;
	write.OffsetHigh = 0;
	write.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	cancel = CreateEvent(NULL, TRUE, FALSE, NULL);

	u8 hostMAC[6];
	u8 newMAC[6];

	TAPGetMACAddress(htap, hostMAC);
	memcpy(newMAC, ps2MAC, 6);

	//Lets take the hosts last 2 bytes to make it unique on Xlink
	newMAC[5] = hostMAC[4];
	newMAC[4] = hostMAC[5];

	SetMACAddress(newMAC);

	isActive = true;
}

bool TAPAdapter::blocks()
{
	return true; //we use blocking io
}
bool TAPAdapter::isInitialised()
{
	return (htap != NULL);
}
//gets a packet.rv :true success
bool TAPAdapter::recv(NetPacket* pkt)
{
	DWORD read_size;
	BOOL result = ReadFile(htap,
						   pkt->buffer,
						   sizeof(pkt->buffer),
						   &read_size,
						   &read);

	if (!result)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			HANDLE readHandles[]{read.hEvent, cancel};
			const DWORD waitResult = WaitForMultipleObjects(2, readHandles, FALSE, INFINITE);

			if (waitResult == WAIT_OBJECT_0 + 1)
			{
				CancelIo(htap);
				//Wait for the I/O subsystem to acknowledge our cancellation
				result = GetOverlappedResult(htap, &read, &read_size, TRUE);
			}
			else
				result = GetOverlappedResult(htap, &read, &read_size, FALSE);
		}
	}

	if (result)
		return VerifyPkt(pkt, read_size);
	else
		return false;
}
//sends the packet .rv :true success
bool TAPAdapter::send(NetPacket* pkt)
{
	DWORD writen;
	BOOL result = WriteFile(htap,
							pkt->buffer,
							pkt->size,
							&writen,
							&write);

	if (!result)
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_IO_PENDING)
		{
			WaitForSingleObject(write.hEvent, INFINITE);
			result = GetOverlappedResult(htap, &write, &writen, FALSE);
		}
	}

	if (result)
	{
		if (writen != pkt->size)
			return false;

		return true;
	}
	else
		return false;
}
void TAPAdapter::close()
{
	SetEvent(cancel);
}
TAPAdapter::~TAPAdapter()
{
	if (!isActive)
		return;
	CloseHandle(read.hEvent);
	CloseHandle(write.hEvent);
	CloseHandle(cancel);
	TAPSetStatus(htap, FALSE);
	CloseHandle(htap);
	isActive = false;
}