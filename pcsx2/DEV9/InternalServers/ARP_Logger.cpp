// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ARP_Logger.h"
#include "DEV9/PacketReader/EthernetFrame.h"

#include "common/Console.h"

using namespace PacketReader;
using namespace PacketReader::ARP;

namespace InternalServers
{
	void ARP_Logger::InspectRecv(Payload* payload)
	{
		ARP_Packet* arp = static_cast<ARP_Packet*>(payload);
		LogPacket(arp);
	}

	void ARP_Logger::InspectSend(Payload* payload)
	{
		ARP_Packet* arp = static_cast<ARP_Packet*>(payload);
		LogPacket(arp);
	}

	std::string ARP_Logger::ArrayToString(const std::unique_ptr<u8[]>& data, int length)
	{
		std::string str;
		if (length != 0)
		{
			str.reserve(length * 4);
			for (size_t i = 0; i < length; i++)
				str += std::to_string(data[i]) + ":";

			str.pop_back();
		} //else leave string empty

		return str;
	}

	const char* ARP_Logger::HardwareTypeToString(u8 op)
	{
		switch (op)
		{
			case 1:
				return "Ethernet";
			case 6:
				return "IEEE 802";
			default:
				return "Unknown";
		}
	}

	const char* ARP_Logger::ProtocolToString(u16 protocol)
	{
		switch (protocol)
		{
			case static_cast<u16>(EtherType::IPv4):
				return "IPv4";
			case static_cast<u16>(EtherType::ARP):
				return "ARP";
			default:
				return "Unknown";
		}
	}

	const char* ARP_Logger::OperationToString(u16 op)
	{
		switch (op)
		{
			case 1:
				return "Request";
			case 2:
				return "Reply";
			default:
				return "Unknown";
		}
	}

	void ARP_Logger::LogPacket(ARP_Packet* arp)
	{
		Console.WriteLn("DEV9: ARP: Hardware Type %s (%i)", HardwareTypeToString(arp->hardwareType), arp->hardwareType);
		Console.WriteLn("DEV9: ARP: Protocol %s (%i)", ProtocolToString(arp->protocol), arp->protocol);
		Console.WriteLn("DEV9: ARP: Operation %s (%i)", OperationToString(arp->op), arp->op);
		Console.WriteLn("DEV9: ARP: Hardware Length %i", arp->hardwareAddressLength);
		Console.WriteLn("DEV9: ARP: Protocol Length %i", arp->protocolAddressLength);
		Console.WriteLn("DEV9: ARP: Sender Hardware Address %s", ArrayToString(arp->senderHardwareAddress, arp->hardwareAddressLength).c_str());
		Console.WriteLn("DEV9: ARP: Sender Protocol Address %s", ArrayToString(arp->senderProtocolAddress, arp->protocolAddressLength).c_str());
		Console.WriteLn("DEV9: ARP: Target Hardware Address %s", ArrayToString(arp->targetHardwareAddress, arp->hardwareAddressLength).c_str());
		Console.WriteLn("DEV9: ARP: Target Protocol Address %s", ArrayToString(arp->targetProtocolAddress, arp->protocolAddressLength).c_str());
	}
} // namespace InternalServers
