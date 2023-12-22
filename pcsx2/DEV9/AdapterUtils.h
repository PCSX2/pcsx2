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
	typedef std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterBuffer;
#elif defined(__POSIX__)
	typedef ifaddrs Adapter;
	struct IfAdaptersDeleter
	{
		void operator()(ifaddrs* buffer) const { freeifaddrs(buffer); }
	};
	typedef std::unique_ptr<ifaddrs, IfAdaptersDeleter> AdapterBuffer;
#endif
	bool GetAdapter(const std::string& name, Adapter* adapter, AdapterBuffer* buffer);
	bool GetAdapterAuto(Adapter* adapter, AdapterBuffer* buffer);

	std::optional<PacketReader::MAC_Address> GetAdapterMAC(Adapter* adapter);
	std::optional<PacketReader::IP::IP_Address> GetAdapterIP(Adapter* adapter);
	// Mask.
	std::vector<PacketReader::IP::IP_Address> GetGateways(Adapter* adapter);
	std::vector<PacketReader::IP::IP_Address> GetDNS(Adapter* adapter);
}; // namespace AdapterUtils
