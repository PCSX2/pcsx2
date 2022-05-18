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

#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"

#include "fmt/core.h"

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include <Netcfgx.h>
#include <devguid.h>

#include <tchar.h>
#include "tap.h"
#include "DEV9/DEV9.h"
#include <string>

#include <wil/com.h>
#include <wil/resource.h>

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
	wil::unique_hkey netcard_key;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, netcard_key.put()) != ERROR_SUCCESS)
		return false;

	int i = 0;
	for (;;)
	{
		TCHAR enum_name[256];
		TCHAR unit_string[256];
		TCHAR component_id_string[] = _T("ComponentId");
		TCHAR component_id[256];
		TCHAR net_cfg_instance_id_string[] = _T("NetCfgInstanceId");
		TCHAR net_cfg_instance_id[256];
		DWORD data_type;

		DWORD len = std::size(enum_name);
		LSTATUS status = RegEnumKeyEx(netcard_key.get(), i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		_stprintf_s(unit_string, _T("%s\\%s"), ADAPTER_KEY, enum_name);

		wil::unique_hkey unit_key;
		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, unit_key.put());

		if (status != ERROR_SUCCESS)
		{
			return false;
		}
		else
		{
			len = sizeof(component_id);
			status = RegQueryValueEx(unit_key.get(), component_id_string, nullptr, &data_type,
				(LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key.get(), net_cfg_instance_id_string, nullptr, &data_type,
					(LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					// tap_ovpnconnect, tap0901 or root\tap, no clue why
					if ((!wcsncmp(component_id, L"tap", 3) || !wcsncmp(component_id, L"root\\tap", 8)) && !_tcscmp(net_cfg_instance_id, guid))
					{
						return true;
					}
				}
			}
		}
		++i;
	}

	return false;
}

std::vector<AdapterEntry> TAPAdapter::GetAdapters()
{
	std::vector<AdapterEntry> tap_nic;
	DWORD len;
	DWORD cSubKeys = 0;

	wil::unique_hkey control_net_key;
	LSTATUS status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ | KEY_QUERY_VALUE,
		control_net_key.put());

	if (status != ERROR_SUCCESS)
		return tap_nic;

	status = RegQueryInfoKey(control_net_key.get(), nullptr, nullptr, nullptr, &cSubKeys, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr, nullptr);

	if (status != ERROR_SUCCESS)
		return tap_nic;

	for (DWORD i = 0; i < cSubKeys; i++)
	{
		TCHAR enum_name[256];
		TCHAR connection_string[256];
		TCHAR name_data[256];
		DWORD name_type;
		const TCHAR name_string[] = _T("Name");

		len = std::size(enum_name);
		status = RegEnumKeyEx(control_net_key.get(), i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status != ERROR_SUCCESS)
			continue;

		_stprintf_s(connection_string, _T("%s\\%s\\Connection"),
			NETWORK_CONNECTIONS_KEY, enum_name);

		wil::unique_hkey connection_key;
		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string, 0, KEY_READ, connection_key.put());

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key.get(), name_string, nullptr, &name_type, (LPBYTE)name_data,
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
					t.type = Pcsx2Config::DEV9Options::NetApi::TAP;
					t.name = StringUtil::WideStringToUTF8String(std::wstring(name_data));
					t.guid = StringUtil::WideStringToUTF8String(std::wstring(enum_name));
					tap_nic.push_back(t);
				}
			}
		}
	}

	return tap_nic;
}

AdapterOptions TAPAdapter::GetAdapterOptions()
{
	return AdapterOptions::None;
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
HANDLE TAPOpen(const std::string& device_guid)
{
	struct
	{
		unsigned long major;
		unsigned long minor;
		unsigned long debug;
	} version;
	LONG version_len;

	std::string device_path = USERMODEDEVICEDIR + device_guid + TAPSUFFIX;

	wil::unique_hfile handle(CreateFileA(
		device_path.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		0));

	if (!handle)
	{
		return INVALID_HANDLE_VALUE;
	}

	BOOL bret = DeviceIoControl(handle.get(), TAP_IOCTL_GET_VERSION,
		&version, sizeof(version),
		&version, sizeof(version), (LPDWORD)&version_len, NULL);

	if (bret == FALSE)
	{
		return INVALID_HANDLE_VALUE;
	}

	if (!TAPSetStatus(handle.get(), TRUE))
	{
		return INVALID_HANDLE_VALUE;
	}

	return handle.release();
}

PIP_ADAPTER_ADDRESSES FindAdapterViaIndex(PIP_ADAPTER_ADDRESSES adapterList, int ifIndex)
{
	PIP_ADAPTER_ADDRESSES currentAdapter = adapterList;
	do
	{
		if (currentAdapter->IfIndex == ifIndex)
			break;

		currentAdapter = currentAdapter->Next;
	} while (currentAdapter);
	return currentAdapter;
}

//IP_ADAPTER_ADDRESSES is a structure that contains ptrs to data in other regions
//of the buffer, se we need to return both so the caller can free the buffer
//after it's finished reading the needed data from IP_ADAPTER_ADDRESSES
bool TAPGetWin32Adapter(const std::string& name, PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer)
{
	int neededSize = 256;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	ULONG dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	PIP_ADAPTER_ADDRESSES pAdapterInfo;

	//GAA_FLAG_INCLUDE_ALL_INTERFACES needed to get Tap when bridged
	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES,
		NULL,
		AdapterInfo.get(),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: GetWin32Adapter() buffer too small, resizing");
		//
		neededSize = dwBufLen / sizeof(IP_ADAPTER_ADDRESSES) + 1;
		AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
		dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES,
			NULL,
			AdapterInfo.get(),
			&dwBufLen);
	}

	if (dwStatus != ERROR_SUCCESS)
		return 0;

	pAdapterInfo = AdapterInfo.get();

	do
	{
		if (0 == strcmp(pAdapterInfo->AdapterName, name.c_str()))
			break;

		pAdapterInfo = pAdapterInfo->Next;
	} while (pAdapterInfo);

	if (pAdapterInfo == nullptr)
		return false;

	//If we are bridged, then we won't show up without GAA_FLAG_INCLUDE_ALL_INTERFACES
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfoReduced = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
		NULL,
		AdapterInfoReduced.get(),
		&dwBufLen);

	if (dwStatus != ERROR_SUCCESS)
		return 0;

	//If we find our adapter in the reduced list, we are not bridged
	if (FindAdapterViaIndex(AdapterInfoReduced.get(), pAdapterInfo->IfIndex) != nullptr)
	{
		*adapter = *pAdapterInfo;
		buffer->swap(AdapterInfo);
		return true;
	}

	//We must be bridged
	Console.WriteLn("DEV9: Current adapter is probably bridged");
	Console.WriteLn(fmt::format("DEV9: Adapter Display name: {}", StringUtil::WideStringToUTF8String(pAdapterInfo->FriendlyName)));

	//We will need to find the bridge adapter that out adapter is
	//as the IP information of the tap adapter is null
	//connected to, the method used to do this is undocumented and windows 8+

	//Only solution found is detailed in this MSDN fourm post by Jeffrey Tippet[MSFT], with a sectin copyied below
	//Some adjustments to the method where required before this would work on my system.
	//https://social.msdn.microsoft.com/Forums/vstudio/en-US/6dc9097e-0c33-427c-8e1b-9e2c81fad367/how-to-detect-if-network-interface-is-part-of-ethernet-bridge-?forum=wdk
	/* To detect the newer LWF driver, it's trickier, since the binding over the NIC would be to the generic IM platform.
	 * Knowing that a NIC is bound to the generic IM platform tells you that it's being used for some fancy thing,
	 * but it doesn't tell you whether it's a bridge or an LBFO team or something more exotic.
	 * The way to distinguish exactly which flavor of ms_implat you have is to look at which LWF driver is bound to the *virtual miniport* above the IM driver.
	 * This is two steps then.
	 * 
	 * 1. Given a physical NIC, you first want to determine which virtual NIC is layered over it.
	 * 2. Given a virtual NIC, you want to determine whether ms_bridge is bound to it.
	 * 
	 * To get the first part, look through the interface stack table (GetIfStackTable). Search the stack table for any entry where the lower is the IfIndex of the physical NIC.
	 * For any such entry (there will probably be a few), check if that entry's upper IfIndex is the IfIndex for a virtual miniport with component ID "COMPOSITEBUS\MS_IMPLAT_MP".
	 * If you find such a thing, that means the physical NIC is a member of a bridge/LBFO/something-else-fancy.
	 * If you don't find it, then you know the NIC isn't part of the bridge that comes with Windows 8 / Windows 10.
	 * 
	 * To get the second part, just use the same INetCfg code above on the *virtual* NIC's component. If the ms_bridge component is bound to the virtual NIC,
	 * then that virtual NIC is doing bridging. Otherwise, it's doing something else (like LBFO).
	 */

	//Step 1
	//Find any rows that how our adapter as the lower index
	//check if the upper adapter has a non-null address
	//If not, we repeat the search with the upper adapter
	//If multiple rows have our adapter, we check all of them
	std::vector<NET_IFINDEX> potentialBridges;
	std::vector<NET_IFINDEX> searchList;
	searchList.push_back(pAdapterInfo->IfIndex);
	int checkCount = 1;

	PMIB_IFSTACK_TABLE table;
	GetIfStackTable(&table);
	//Note that we append to the collection during iteration
	for (size_t vi = 0; vi < searchList.size(); vi++)
	{
		int targetIndex = searchList[vi];

		for (ULONG i = 0; i < table->NumEntries; i++)
		{
			MIB_IFSTACK_ROW row = table->Table[i];
			if (row.LowerLayerInterfaceIndex == targetIndex)
			{
				PIP_ADAPTER_ADDRESSES potentialAdapter = FindAdapterViaIndex(AdapterInfoReduced.get(), row.HigherLayerInterfaceIndex);
				if (potentialAdapter != nullptr)
				{
					Console.WriteLn(fmt::format("DEV9: {} is possible bridge (Check 1 passed)", StringUtil::WideStringToUTF8String(potentialAdapter->Description)));
					potentialBridges.push_back(row.HigherLayerInterfaceIndex);
				}
				else
					searchList.push_back(row.HigherLayerInterfaceIndex);
				break;
			}
		}
	}
	//Cleanup
	FreeMibTable(table);
	AdapterInfoReduced = nullptr;

	//Step 2
	//Init COM
	HRESULT cohr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (cohr == RPC_E_CHANGED_MODE)
		cohr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (!SUCCEEDED(cohr))
		return false;

	PIP_ADAPTER_ADDRESSES bridgeAdapter = nullptr;

	//Create Instance of INetCfg
	if (auto netcfg = wil::CoCreateInstanceNoThrow<INetCfg>(CLSID_CNetCfg))
	{
		HRESULT hr = netcfg->Initialize(nullptr);
		if (SUCCEEDED(hr))
		{
			//Get the bridge component
			//The bridged adapter should have this bound
			wil::com_ptr_nothrow<INetCfgComponent> bridge;
			hr = netcfg->FindComponent(L"ms_bridge", bridge.put());

			if (SUCCEEDED(hr))
			{
				//Get a List of network adapters via INetCfg
				wil::com_ptr_nothrow<IEnumNetCfgComponent> components;
				hr = netcfg->EnumComponents(&GUID_DEVCLASS_NET, components.put());
				if (SUCCEEDED(hr))
				{
					//Search possible bridge adapters
					for (const auto& index : potentialBridges)
					{
						//We need to match the adapter index to an INetCfgComponent
						//We do this by matching IP_ADAPTER_ADDRESSES.AdapterName
						//with the INetCfgComponent Instance GUID
						PIP_ADAPTER_ADDRESSES cAdapterInfo = FindAdapterViaIndex(AdapterInfo.get(), index);

						if (cAdapterInfo == nullptr || cAdapterInfo->AdapterName == nullptr)
							continue;

						//Convert Name to GUID
						wchar_t wName[40] = {0};
						mbstowcs(wName, cAdapterInfo->AdapterName, 39);
						GUID nameGuid;
						hr = IIDFromString(wName, &nameGuid);
						if (!SUCCEEDED(hr))
							continue;

						//Loop through components
						wil::com_ptr_nothrow<INetCfgComponent> component;
						while (true)
						{
							if (components->Next(1, component.put(), nullptr) != S_OK)
								break;

							GUID comInstGuid;
							hr = component->GetInstanceGuid(&comInstGuid);

							if (SUCCEEDED(hr) && IsEqualGUID(nameGuid, comInstGuid))
							{
								wil::unique_cotaskmem_string comId;
								hr = component->GetId(comId.put());
								if (!SUCCEEDED(hr))
									continue;

								//The bridge adapter for Win8+ has this ComponentID
								//However not every adapter with this componentID is a bridge
								if (wcscmp(L"compositebus\\ms_implat_mp", comId.get()) == 0)
								{
									wil::unique_cotaskmem_string dispName;
									hr = component->GetDisplayName(dispName.put());
									if (SUCCEEDED(hr))
										Console.WriteLn(fmt::format("DEV9: {} is possible bridge (Check 2 passed)", StringUtil::WideStringToUTF8String(dispName.get())));

									//Check if adapter has the ms_bridge component bound to it.
									auto bindings = bridge.try_query<INetCfgComponentBindings>();
									if (!bindings)
										continue;

									hr = bindings->IsBoundTo(component.get());
									if (hr != S_OK)
										continue;

									hr = component->GetDisplayName(dispName.put());
									if (SUCCEEDED(hr))
										Console.WriteLn(fmt::format("DEV9: {} is bridge (Check 3 passed)", StringUtil::WideStringToUTF8String(dispName.get())));

									bridgeAdapter = cAdapterInfo;
									break;
								}
							}
						}
						components->Reset();
						if (bridgeAdapter != nullptr)
							break;
					}
				}
			}
			netcfg->Uninitialize();
		}
	}

	CoUninitialize();

	if (bridgeAdapter != nullptr)
	{
		*adapter = *bridgeAdapter;
		buffer->swap(AdapterInfo);
		return true;
	}

	return false;
}

TAPAdapter::TAPAdapter()
	: NetAdapter()
{
	if (!EmuConfig.DEV9.EthEnable)
		return;
	htap = TAPOpen(EmuConfig.DEV9.EthDevice);

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

	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;
	if (TAPGetWin32Adapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer))
		InitInternalServer(&adapter);
	else
	{
		Console.Error("DEV9: Failed to get adapter information");
		InitInternalServer(nullptr);
	}

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

	if (result && VerifyPkt(pkt, read_size))
	{
		InspectRecv(pkt);
		return true;
	}
	else
		return false;
}
//sends the packet .rv :true success
bool TAPAdapter::send(NetPacket* pkt)
{
	InspectSend(pkt);
	if (NetAdapter::send(pkt))
		return true;

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

void TAPAdapter::reloadSettings()
{
	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;
	if (TAPGetWin32Adapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer))
		ReloadInternalServer(&adapter);
	else
		ReloadInternalServer(nullptr);
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