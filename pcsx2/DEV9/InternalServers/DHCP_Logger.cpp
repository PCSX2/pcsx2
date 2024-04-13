// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DHCP_Logger.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

#include <algorithm>

#include "common/Console.h"
#include "common/StringUtil.h"

#include "DEV9/AdapterUtils.h"

using PacketReader::PayloadPtr;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DHCP;

namespace InternalServers
{
#ifdef _WIN32
	void DHCP_Logger::Init(PIP_ADAPTER_ADDRESSES adapter)
#elif defined(__POSIX__)
	void DHCP_Logger::Init(ifaddrs* adapter)
#endif
	{
		std::optional<IP_Address> adIP = AdapterUtils::GetAdapterIP(adapter);
		if (adIP.has_value())
			pcIP = adIP.value();
		else
			pcIP = {};
	}

	void DHCP_Logger::InspectRecv(IP_Payload* payload)
	{
		UDP_Packet* udpPacket = static_cast<UDP_Packet*>(payload);
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udpPacket->GetPayload());
		DHCP_Packet dhcp(udpPayload->data, udpPayload->GetLength());
		Console.WriteLn("DEV9: DHCP: Host PC IP is %s", IpToString(pcIP).c_str());
		LogPacket(&dhcp);
	}

	void DHCP_Logger::InspectSend(IP_Payload* payload)
	{
		UDP_Packet* udpPacket = static_cast<UDP_Packet*>(payload);
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udpPacket->GetPayload());
		DHCP_Packet dhcp(udpPayload->data, udpPayload->GetLength());
		Console.WriteLn("DEV9: DHCP: Host PC IP is %s", IpToString(pcIP).c_str());
		LogPacket(&dhcp);
	}

	std::string DHCP_Logger::IpToString(IP_Address ip)
	{
		return StringUtil::StdStringFromFormat("%u.%u.%u.%u", ip.bytes[0], ip.bytes[1], ip.bytes[2], ip.bytes[3]);
	}

	std::string DHCP_Logger::HardwareAddressToString(u8* data, size_t len)
	{
		std::string str;
		if (len != 0)
		{
			str.reserve(len * 4);
			for (size_t i = 0; i < len; i++)
				str += StringUtil::StdStringFromFormat("%.2X:", data[i]);

			str.pop_back();
		} // else leave string empty
		return str;
	}

	std::string DHCP_Logger::ClientIdToString(const std::vector<u8>& data)
	{
		std::string str;
		if (data.size() != 0)
		{
			str.reserve(data.size() * 4);
			for (size_t i = 0; i < data.size(); i++)
				str += StringUtil::StdStringFromFormat("%.2X:", data[i]);

			str.pop_back();
		} // else leave string empty
		return str;
	}

	const char* DHCP_Logger::OpToString(u8 op)
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

	const char* DHCP_Logger::HardwareTypeToString(u8 op)
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

	const char* DHCP_Logger::OptionToString(u8 option)
	{
		switch (option)
		{
			case 0:
				return "Nop";
			case 1:
				return "Subnet";
			case 3:
				return "Routers";
			case 6:
				return "DNS";
			case 12:
				return "Host Name";
			case 15:
				return "DNS Name";
			case 28:
				return "Broadcast IP";
			case 46:
				return "NetBIOS Type";
			case 50:
				return "Requested IP";
			case 51:
				return "IP Lease Time";
			case 53:
				return "Message Type";
			case 54:
				return "Server IP";
			case 55:
				return "Request List";
			case 56:
				return "Message String";
			case 57:
				return "Max Message Size";
			case 58:
				return "Renewal Time T1";
			case 59:
				return "Rebinding Time T2";
			case 60:
				return "Class ID";
			case 61:
				return "Client ID";
			case 255:
				return "End";
			default:
				return "Unknown";
		}
	}

	const char* DHCP_Logger::MessageCodeToString(u8 op)
	{
		switch (op)
		{
			case 1:
				return "DHCP Discover";
			case 2:
				return "DHCP Offer";
			case 3:
				return "DHCP Request";
			case 4:
				return "DHCP Decline";
			case 5:
				return "DHCP ACK";
			case 6:
				return "DHCP NACK";
			case 7:
				return "DHCP Release";
			case 8:
				return "DHCP Inform";
			default:
				return "Unknown";
		}
	}

	void DHCP_Logger::LogPacket(DHCP_Packet* dhcp)
	{
		Console.WriteLn("DEV9: DHCP: Op %s (%i)", OpToString(dhcp->op), dhcp->op);
		Console.WriteLn("DEV9: DHCP: Hardware Type %s (%i)", HardwareTypeToString(dhcp->hardwareType), dhcp->hardwareType);
		Console.WriteLn("DEV9: DHCP: Hardware Address Length %i", dhcp->hardwareAddressLength);
		Console.WriteLn("DEV9: DHCP: Hops %i", dhcp->hops);
		Console.WriteLn("DEV9: DHCP: Transaction ID %i", dhcp->transactionID);
		Console.WriteLn("DEV9: DHCP: Seconds %i", dhcp->seconds);
		Console.WriteLn("DEV9: DHCP: Flags 0x%.4X", dhcp->flags);
		Console.WriteLn("DEV9: DHCP: Client IP %s", IpToString(dhcp->clientIP).c_str());
		Console.WriteLn("DEV9: DHCP: Your IP %s", IpToString(dhcp->yourIP).c_str());
		Console.WriteLn("DEV9: DHCP: Server IP %s", IpToString(dhcp->serverIP).c_str());
		Console.WriteLn("DEV9: DHCP: Gateway IP %s", IpToString(dhcp->gatewayIP).c_str());
		Console.WriteLn("DEV9: DHCP: Gateway IP %s", IpToString(dhcp->gatewayIP).c_str());
		Console.WriteLn("DEV9: DHCP: Client Hardware Address %s", HardwareAddressToString(dhcp->clientHardwareAddress, std::min<u8>(dhcp->hardwareAddressLength, 16)).c_str());
		Console.WriteLn("DEV9: DHCP: Magic Cookie 0x%.8X", dhcp->magicCookie);

		Console.WriteLn("DEV9: DHCP: Options Count %i", dhcp->options.size());

		for (size_t i = 0; i < dhcp->options.size(); i++)
		{
			BaseOption* entry = dhcp->options[i];
			Console.WriteLn("DEV9: DHCP: Option %s (%i)", OptionToString(entry->GetCode()), entry->GetCode());
			Console.WriteLn("DEV9: DHCP: Option Size %i", entry->GetLength());
			switch (entry->GetCode())
			{
				case 0:
					break;
				case 1:
				{
					const DHCPopSubnet* subnet = static_cast<DHCPopSubnet*>(entry);
					Console.WriteLn("DEV9: DHCP: Subnet %s", IpToString(subnet->subnetMask).c_str());
					break;
				}
				case 3:
				{
					const DHCPopRouter* routers = static_cast<DHCPopRouter*>(entry);
					Console.WriteLn("DEV9: DHCP: Routers Count %i", routers->routers.size());
					for (size_t j = 0; j < routers->routers.size(); j++)
						Console.WriteLn("DEV9: DHCP: Router %s", IpToString(routers->routers[j]).c_str());
					break;
				}
				case 6:
				{
					const DHCPopDNS* dns = static_cast<DHCPopDNS*>(entry);
					Console.WriteLn("DEV9: DHCP: DNS Count %i", dns->dnsServers.size());
					for (size_t j = 0; j < dns->dnsServers.size(); j++)
						Console.WriteLn("DEV9: DHCP: DNS %s", IpToString(dns->dnsServers[j]).c_str());
					break;
				}
				case 12:
				{
					const DHCPopHostName* name = static_cast<DHCPopHostName*>(entry);
					Console.WriteLn("DEV9: DHCP: Host Name %s", name->hostName.c_str());
					break;
				}
				case 15:
				{
					const DHCPopDnsName* name = static_cast<DHCPopDnsName*>(entry);
					Console.WriteLn("DEV9: DHCP: Domain Name %s", name->domainName.c_str());
					break;
				}
				case 28:
				{
					const DHCPopBCIP* broadcast = static_cast<DHCPopBCIP*>(entry);
					Console.WriteLn("DEV9: DHCP: Broadcast IP %s", IpToString(broadcast->broadcastIP).c_str());
					break;
				}
				case 46:
				{
					DHCPopNBIOSType* biosType = static_cast<DHCPopNBIOSType*>(entry);
					Console.WriteLn("DEV9: DHCP: NetBIOS B-Node %s", biosType->GetBNode() ? "True" : "False");
					Console.WriteLn("DEV9: DHCP: NetBIOS P-Node %s", biosType->GetPNode() ? "True" : "False");
					Console.WriteLn("DEV9: DHCP: NetBIOS M-Node %s", biosType->GetMNode() ? "True" : "False");
					Console.WriteLn("DEV9: DHCP: NetBIOS H-Node %s", biosType->GetHNode() ? "True" : "False");
					break;
				}
				case 50:
				{
					const DHCPopREQIP* req = static_cast<DHCPopREQIP*>(entry);
					Console.WriteLn("DEV9: DHCP: Requested IP %s", IpToString(req->requestedIP).c_str());
					break;
				}
				case 51:
				{
					const DHCPopIPLT* iplt = static_cast<DHCPopIPLT*>(entry);
					Console.WriteLn("DEV9: DHCP: IP Least Time %i", iplt->ipLeaseTime);
					break;
				}
				case 53:
				{
					const DHCPopMSG* msg = static_cast<DHCPopMSG*>(entry);
					Console.WriteLn("DEV9: DHCP: Message %s (%i)", MessageCodeToString(msg->message), msg->message);
					break;
				}
				case 54:
				{
					const DHCPopSERVIP* req = static_cast<DHCPopSERVIP*>(entry);
					Console.WriteLn("DEV9: DHCP: Server IP %s", IpToString(req->serverIP).c_str());
					break;
				}
				case 55:
				{
					const DHCPopREQLIST* reqList = static_cast<DHCPopREQLIST*>(entry);
					Console.WriteLn("DEV9: DHCP: Request Count %i", reqList->requests.size());
					for (size_t j = 0; j < reqList->requests.size(); j++)
						Console.WriteLn("DEV9: DHCP: Requested %s (%i)", OptionToString(reqList->requests[j]), reqList->requests[j]);
					break;
				}
				case 56:
				{
					const DHCPopMSGStr* msg = static_cast<DHCPopMSGStr*>(entry);
					Console.WriteLn("DEV9: DHCP: Message %s", msg->message.c_str());
					break;
				}
				case 57:
				{
					const DHCPopMMSGS* maxMs = static_cast<DHCPopMMSGS*>(entry);
					Console.WriteLn("DEV9: DHCP: Max Message Size %i", maxMs->maxMessageSize);
					break;
				}
				case 58:
				{
					const DHCPopT1* t1 = static_cast<DHCPopT1*>(entry);
					Console.WriteLn("DEV9: DHCP: Renewal Time (T1) %i", t1->ipRenewalTimeT1);
					break;
				}
				case 59:
				{
					const DHCPopT2* t2 = static_cast<DHCPopT2*>(entry);
					Console.WriteLn("DEV9: DHCP: Rebinding Time (T2) %i", t2->ipRebindingTimeT2);
					break;
				}
				case 60:
				{
					const DHCPopClassID* id = static_cast<DHCPopClassID*>(entry);
					Console.WriteLn("DEV9: DHCP: Class ID %s", id->classID.c_str());
					break;
				}
				case 61:
				{
					const DHCPopClientID* id = static_cast<DHCPopClientID*>(entry);
					Console.WriteLn("DEV9: DHCP: Client ID %s", ClientIdToString(id->clientID).c_str());
					break;
				}
				case 255:
					break;
				default:
					break;
			}
		}
	}
} // namespace InternalServers
