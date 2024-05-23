// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include <string>
#include <memory>
#include <optional>
#include <vector>

#include "DEV9/PacketReader/MAC_Address.h"
#include "DEV9/PacketReader/IP/IP_Address.h"

namespace AdapterUtils
{
#ifdef _WIN32
	typedef IP_ADAPTER_ADDRESSES Adapter;
	typedef std::unique_ptr<std::byte[]> AdapterBuffer;
#elif defined(__POSIX__)
	typedef ifaddrs Adapter;
	struct IfAdaptersDeleter
	{
		void operator()(ifaddrs* buffer) const { freeifaddrs(buffer); }
	};
	typedef std::unique_ptr<ifaddrs, IfAdaptersDeleter> AdapterBuffer;
#endif

	u16 ReadAddressFamily(const sockaddr* unknownAddr);

	// Adapter is a structure that contains ptrs to data stored within AdapterBuffer.
	// We need to return this buffer the caller can free it after it's finished with Adapter.
	// AdapterBuffer is a unique_ptr, so will be freed when it leaves scope.
#ifdef _WIN32
	// includeHidden sets GAA_FLAG_INCLUDE_ALL_INTERFACES, used by TAPAdapter
	Adapter* GetAllAdapters(AdapterBuffer* buffer, bool includeHidden = false);
#elif defined(__POSIX__)
	Adapter* GetAllAdapters(AdapterBuffer* buffer);
#endif
	bool GetAdapter(const std::string& name, Adapter* adapter, AdapterBuffer* buffer);
	bool GetAdapterAuto(Adapter* adapter, AdapterBuffer* buffer);

	std::optional<PacketReader::MAC_Address> GetAdapterMAC(const Adapter* adapter);
	std::optional<PacketReader::IP::IP_Address> GetAdapterIP(const Adapter* adapter);
	// Mask.
	std::vector<PacketReader::IP::IP_Address> GetGateways(const Adapter* adapter);
	std::vector<PacketReader::IP::IP_Address> GetDNS(const Adapter* adapter);
}; // namespace AdapterUtils
