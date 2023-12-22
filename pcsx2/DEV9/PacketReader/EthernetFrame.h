// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "DEV9/net.h"
#include "MAC_Address.h"
#include "Payload.h"

namespace PacketReader
{
	enum struct EtherType : u16
	{
		null = 0x0000,
		IPv4 = 0x0800,
		ARP = 0x0806,
		//Tags extend the ether header length
		VlanQTag = 0x8100,
		VlanServiceQTag = 0x88A8,
		VlanDoubleQTag = 0x9100
	};

	class EthernetFrame
	{
	public:
		MAC_Address destinationMAC{};
		MAC_Address sourceMAC{};

		u16 protocol = 0;
		int headerLength = 14;
		//Length
	private:
		std::unique_ptr<Payload> payload;

	public:
		//Takes ownership of payload
		EthernetFrame(Payload* data);
		EthernetFrame(NetPacket* pkt);

		Payload* GetPayload();

		void WritePacket(NetPacket* pkt);
	};
} // namespace PacketReader
