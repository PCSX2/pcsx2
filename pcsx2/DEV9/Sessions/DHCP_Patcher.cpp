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

#include "DHCP_Patcher.h"
#include "../PacketReader/IP/UDP/UDP_Packet.h"

using PacketReader::PayloadPtr;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DHCP;

namespace Sessions
{
	size_t FindOptionIndex(std::vector<BaseOption*> options, u8 code)
	{
		for (size_t i = 0; i < options.size(); i++)
			if (options[i] != nullptr && options[i]->GetCode() == code)
				return i;
		return -1;
	}

	IP_Payload* DHCP_Patcher::InspectSent(IP_Payload* payload)
	{
		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		UDP_Packet* udppkt = new UDP_Packet(ipPayload->data, ipPayload->GetLength());

		if (udppkt->destinationPort == 67 && udppkt->sourcePort == 68)
		{
			Console.Error("DHCP Sent;");
			PayloadPtr payload = *static_cast<PayloadPtr*>(udppkt->GetPayload());
			DHCP_Packet* dhcppkt = new DHCP_Packet(payload.data, payload.GetLength());

			//No modifications
			delete dhcppkt;
			delete udppkt;
			return nullptr;
		}



		//u8 msg;

		////Loop though options
		//for (size_t i = 0; i < dhcppkt->options.size(); i++)
		//{
		//	switch (dhcppkt->options[i]->GetCode())
		//	{
		//		//Only care about three options
		//		case 53:
		//		{
		//			const DHCPopMSG* opMsg = static_cast<DHCPopMSG*>(dhcppkt->options[i]);
		//			//msg = opMsg->message;
		//			break;
		//		}
		//		case 55:
		//		{
		//			const DHCPopREQLIST* opReq = static_cast<DHCPopREQLIST*>(dhcppkt->options[i]);
		//			reqList = opReq->requests;
		//			break;
		//		}
		//		case 57:
		//		{
		//			const DHCPopMMSGS* opMMS = static_cast<DHCPopMMSGS*>(dhcppkt->options[i]);
		//			maxMessage = opMMS->maxMessageSize;
		//			break;
		//		}
		//		default:
		//			break;
		//	}
		//}

		delete udppkt;
		return nullptr;
	}

	IP_Payload* DHCP_Patcher::InspectRecv(IP_Payload* payload)
	{
		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		UDP_Packet* udppkt = new UDP_Packet(ipPayload->data, ipPayload->GetLength());

		if (udppkt->destinationPort == 68 && udppkt->sourcePort == 67)
		{
			Console.Error("DHCP recv;");
			PayloadPtr payload = *static_cast<PayloadPtr*>(udppkt->GetPayload());
			DHCP_Packet* dhcppkt = new DHCP_Packet(payload.data, payload.GetLength());

			//udppkt->SetPayload(dhcppkt, true);

			//No modifications
			delete dhcppkt;
			delete udppkt;
		}



		//Response
		//PayloadPtr* payload = static_cast<PayloadPtr*>(udpPayload);
		//DHCP_Packet* dhcppkt = new DHCP_Packet(payload->data, payload->GetLength());

		//u8 msg;

		//std::vector<u8> list = reqList;

		////Needed for broadcast option
		//IP_Address yourIP = dhcppkt->yourIP;
		//IP_Address subnetMask{0};
		//IP_Address serverIP{0};
		////IP_Address gatewayIP{0};

		////Loop though options
		//for (size_t i = 0; i < dhcppkt->options.size(); i++)
		//{
		//	const u8 code = dhcppkt->options[i]->GetCode();

		//	//Pull any info we might need
		//	switch (code)
		//	{
		//		//Case 1, maybe case 50?
		//		case 1:
		//		{
		//			const DHCPopSubnet* opMsg = static_cast<DHCPopSubnet*>(dhcppkt->options[i]);
		//			subnetMask = opMsg->subnetMask;
		//			break;
		//		}
		//		//case 3:
		//		//{
		//		//	const DHCPopRouter* opRouter = static_cast<DHCPopRouter*>(dhcppkt->options[i]);
		//		//	gatewayIP = opRouter->routers[0]; //Take 1st Router
		//		//}
		//		case 53:
		//		{
		//			const DHCPopMSG* opMsg = static_cast<DHCPopMSG*>(dhcppkt->options[i]);
		//			msg = opMsg->message;
		//			break;
		//		}
		//		case 54:
		//		{
		//			const DHCPopSERVIP* opServIP = static_cast<DHCPopSERVIP*>(dhcppkt->options[i]);
		//			serverIP = opServIP->serverIP;
		//			break;
		//		}

		//		default:
		//			break;
		//	}
		//}

		////Fill in anything missing from header
		//if (*(u32*)&dhcppkt->serverIP == 0)
		//{
		//	Console.Error("DHCP Server didn't fill in Server IP field");
		//	dhcppkt->serverIP = serverIP;
		//}

		//dhcppkt->flags = 0;
		//dhcppkt->seconds = 0;

		////Clone options
		//std::vector<BaseOption*> dhcpOptions = dhcppkt->options;

		////Clear options
		////We want the options to be placed in the order
		////they are requested, so clear and re-add
		//dhcppkt->options.clear();

		//size_t index;
		////Pull End & ServerIP off list
		////We want these two to be the
		////last options on the list
		//BaseOption* opLT = nullptr;
		//BaseOption* opServIP = nullptr;
		//BaseOption* opEnd = nullptr;

		//index = FindOptionIndex(dhcpOptions, 51);
		//if (index != -1)
		//{
		//	opLT = dhcpOptions[index];
		//	dhcpOptions[index] = nullptr;
		//}
		//index = FindOptionIndex(dhcpOptions, 54);
		//if (index != -1)
		//{
		//	opServIP = dhcpOptions[index];
		//	dhcpOptions[index] = nullptr;
		//}
		//index = FindOptionIndex(dhcpOptions, 255);
		//if (index != -1)
		//{
		//	opEnd = dhcpOptions[index];
		//	dhcpOptions[index] = nullptr;
		//}

		////Make sure MSG is 1st
		//index = FindOptionIndex(dhcpOptions, 53);
		//if (index != -1)
		//{
		//	dhcppkt->options.push_back(dhcpOptions[index]);
		//	dhcpOptions[index] = nullptr;
		//}

		//if (msg == 1 || msg == 3) //Fill out Requests
		//{
		//	//loop though req list, adding in order they are requested
		//	for (size_t i = 0; i < list.size(); i++)
		//	{
		//		index = FindOptionIndex(dhcpOptions, list[i]);
		//		if (index != -1)
		//		{
		//			dhcppkt->options.push_back(dhcpOptions[index]);
		//			dhcpOptions[index] = nullptr;
		//		}
		//		else
		//		{
		//			//Fill in any requests missed by DHCP server
		//			switch (list[i])
		//			{
		//				case 0:
		//					break;
		//				case 15:
		//					dhcppkt->options.push_back(new DHCPopDnsName("DummyName"));
		//					Console.Error("DHCP Patcher answered domain name");
		//					break;
		//				case 28:
		//				{
		//					if (*(u32*)&subnetMask == 0)
		//					{
		//						Console.Error("DHCP Patcher found unanswered request %d, and wasn't able to fulfill", list[i]);
		//						break;
		//					}

		//					u32 broadcastInt = (*(u32*)&yourIP | ~*(u32*)&subnetMask);
		//					IP_Address broadcast = *(IP_Address*)&broadcastInt;
		//					dhcppkt->options.push_back(new DHCPopBCIP(broadcast));
		//					Console.Error("DHCP Patcher answered broadcast IP");
		//					break;
		//				}
		//				default:
		//					Console.Error("DHCP Patcher found unanswered request %d, and wasn't able to fulfill", list[i]);
		//					break;
		//			}
		//		}
		//	}
		//}

		////Drop anything not requested
		//for (size_t i = 0; i < dhcpOptions.size(); i++)
		//{
		//	if (dhcpOptions[i] != nullptr)
		//	{
		//		delete dhcpOptions[i];
		//		dhcpOptions[i] = nullptr;
		//	}
		//}

		////Put final options on list
		//if (opLT != nullptr)
		//	dhcppkt->options.push_back(opLT);
		//if (opServIP != nullptr)
		//	dhcppkt->options.push_back(opServIP);
		//if (opEnd != nullptr)
		//	dhcppkt->options.push_back(opEnd);

		//And done
		delete udppkt;
		return nullptr;
	}
} // namespace Sessions
