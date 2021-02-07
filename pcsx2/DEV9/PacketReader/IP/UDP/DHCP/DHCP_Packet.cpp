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
#include "DHCP_Packet.h"
#include "../../../NetLib.h"

namespace PacketReader::IP::UDP::DHCP
{
	DHCP_Packet::DHCP_Packet(u8* buffer, int bufferSize)
	{
		Console.Error("Reading DHCP packet");
		int offset = 0;
		//Bits 0-31 //Bytes 0-3
		NetLib::ReadByte08(buffer, &offset, &op);
		Console.Error("OP           %d", op);
		NetLib::ReadByte08(buffer, &offset, &hardwareType);
		Console.Error("HwType       %d", hardwareType);
		NetLib::ReadByte08(buffer, &offset, &hardwareAddressLength);
		Console.Error("HwAddrLen    %d", hardwareAddressLength);
		NetLib::ReadByte08(buffer, &offset, &hops);
		Console.Error("Hops         %d", hops);

		//Bits 32-63 //Bytes 4-7
		NetLib::ReadUInt32(buffer, &offset, &transactionID);
		Console.Error("xid          0x%08X", transactionID);

		//Bits 64-95 //Bytes 8-11
		NetLib::ReadUInt16(buffer, &offset, &seconds);
		Console.Error("Seconds      %d", seconds);
		NetLib::ReadUInt16(buffer, &offset, &flags);
		Console.Error("Flags        0x%04X", flags);

		//Bits 96-127 //Bytes 12-15
		NetLib::ReadByteArray(buffer, &offset, 4, (u8*)&clientIP);
		Console.Error("CIP          %d.%d.%d.%d", clientIP.bytes[0], clientIP.bytes[1], clientIP.bytes[2], clientIP.bytes[3]);

		//Bits 128-159 //Bytes 16-19
		NetLib::ReadByteArray(buffer, &offset, 4, (u8*)&yourIP);
		Console.Error("YIP          %d.%d.%d.%d", yourIP.bytes[0], yourIP.bytes[1], yourIP.bytes[2], yourIP.bytes[3]);

		//Bits 160-191 //Bytes 20-23
		NetLib::ReadByteArray(buffer, &offset, 4, (u8*)&serverIP);
		Console.Error("SIP          %d.%d.%d.%d", serverIP.bytes[0], serverIP.bytes[1], serverIP.bytes[2], serverIP.bytes[3]);

		//Bits 192-223 //Bytes 24-27
		NetLib::ReadByteArray(buffer, &offset, 4, (u8*)&gatewayIP);
		Console.Error("GIP          %d.%d.%d.%d", gatewayIP.bytes[0], gatewayIP.bytes[1], gatewayIP.bytes[2], gatewayIP.bytes[3]);

		//Bits 192+ //Bytes 28-43
		NetLib::ReadByteArray(buffer, &offset, 16, clientHardwareAddress);
		Console.Error("CHwAddr      %02X:%02X:%02X:%02X:%02X:%02X", //Only care about mac bytes
					  clientHardwareAddress[0], clientHardwareAddress[1], clientHardwareAddress[2], clientHardwareAddress[3], clientHardwareAddress[4], clientHardwareAddress[5]);

		//Bytes 44-235
		//Assume BOOTP unused
		offset += 192;

		//Bytes 236-239
		NetLib::ReadUInt32(buffer, &offset, &magicCookie);
		Console.Error("Cookie       0x%08X", magicCookie);
		bool opReadFin = false;

		Console.Error("Reading Options...");
		do
		{
			u8 opKind = buffer[offset];
			if (opKind == 255)
			{
				Console.Error("Got End");
				options.push_back(new DHCPopEND());
				opReadFin = true;
				offset += 1;
				continue;
			}
			if ((offset + 1) >= bufferSize)
			{
				Console.Error("Unexpected end of packet");
				options.push_back(new DHCPopEND());
				opReadFin = true;
				continue;
			}
			u8 opLen = buffer[offset + 1];
			switch (opKind)
			{
				case 0:
					Console.Error("Got NOP");
					options.push_back(new DHCPopNOP());
					offset += 1;
					continue;
				case 1:
				{
					Console.Error("Got Subnet");
					DHCPopSubnet* subnet = new DHCPopSubnet(buffer, offset);
					Console.Error("Entry        %d.%d.%d.%d",
								  subnet->subnetMask.bytes[0], subnet->subnetMask.bytes[1], subnet->subnetMask.bytes[2], subnet->subnetMask.bytes[3]);
					options.push_back(subnet);
					break;
				}
				case 3:
				{
					Console.Error("Got Router");
					DHCPopRouter* router = new DHCPopRouter(buffer, offset);
					for (size_t i = 0; i < router->routers.size(); i++)
					{
						Console.Error("Entry%d	 %d.%d.%d.%d", i + 1,
									  router->routers[i].bytes[0], router->routers[i].bytes[1], router->routers[i].bytes[2], router->routers[i].bytes[3]);
					}
					options.push_back(router);
					break;
				}
				case 6:
				{
					Console.Error("Got DNS Servers");
					DHCPopDNS* dns = new DHCPopDNS(buffer, offset);
					for (size_t i = 0; i < dns->dnsServers.size(); i++)
					{
						Console.Error("Entry%d	 %d.%d.%d.%d", i + 1,
									  dns->dnsServers[i].bytes[0], dns->dnsServers[i].bytes[1], dns->dnsServers[i].bytes[2], dns->dnsServers[i].bytes[3]);
					}
					options.push_back(dns);
					break;
				}
				case 12:
				{
					Console.Error("Got HostName");
					DHCPopHostName* hostName = new DHCPopHostName(buffer, offset);
					Console.Error("Entry        %s", hostName->hostName.c_str());
					options.push_back(hostName);
					break;
				}
				case 15:
				{
					Console.Error("Got Domain Name");
					DHCPopDnsName* domainName = new DHCPopDnsName(buffer, offset);
					Console.Error("Entry        %s", domainName->domainName.c_str());
					options.push_back(domainName);
					break;
				}
				case 28:
				{
					Console.Error("Got broadcast");
					DHCPopBCIP* broadcast = new DHCPopBCIP(buffer, offset);
					Console.Error("Entry        %d.%d.%d.%d",
								  broadcast->broadcastIP.bytes[0], broadcast->broadcastIP.bytes[1], broadcast->broadcastIP.bytes[2], broadcast->broadcastIP.bytes[3]);
					options.push_back(broadcast);
					break;
				}
				case 46:
				{
					Console.Error("Got NETBIOS Node Type");
					//Do we actully care about this?
					DHCPopNBIOSType* nodeType = new DHCPopNBIOSType(buffer, offset);
					options.push_back(nodeType);
					break;
				}
				case 50:
				{
					Console.Error("Got Request IP");
					DHCPopREQIP* reqIP = new DHCPopREQIP(buffer, offset);
					Console.Error("Entry        %d.%d.%d.%d",
								  reqIP->requestedIP.bytes[0], reqIP->requestedIP.bytes[1], reqIP->requestedIP.bytes[2], reqIP->requestedIP.bytes[3]);
					options.push_back(reqIP);
					break;
				}
				case 51:
				{
					Console.Error("Got IP Address Lease Time");
					DHCPopIPLT* iplt = new DHCPopIPLT(buffer, offset);
					Console.Error("Entry        %d", iplt->ipLeaseTime);
					options.push_back(iplt);
					break;
				}
				case 53:
				{
					Console.Error("Got MSG (value)");
					DHCPopMSG* msg = new DHCPopMSG(buffer, offset);
					Console.Error("Entry        %d", msg->message);
					options.push_back(msg);
					break;
				}
				case 54:
				{
					Console.Error("Got Server IP");
					DHCPopSERVIP* servIP = new DHCPopSERVIP(buffer, offset);
					Console.Error("Entry        %d.%d.%d.%d",
								  servIP->serverIP.bytes[0], servIP->serverIP.bytes[1], servIP->serverIP.bytes[2], servIP->serverIP.bytes[3]);
					options.push_back(servIP);
					break;
				}
				case 55:
				{
					Console.Error("Got Request List");
					DHCPopREQLIST* req = new DHCPopREQLIST(buffer, offset);
					for (size_t i = 0; i < req->requests.size(); i++)
						Console.Error("Entry%d	 %d", i + 1, req->requests[i]);
					options.push_back(req);
					break;
				}
				case 56:
				{
					Console.Error("Got MSG (string)");
					DHCPopMSGStr* msg = new DHCPopMSGStr(buffer, offset);
					Console.Error("Entry        %s", msg->message.c_str());
					options.push_back(msg);
					break;
				}
				case 57:
				{
					Console.Error("Got Max Message Size");
					DHCPopMMSGS* maxmsg = new DHCPopMMSGS(buffer, offset);
					Console.Error("Entry        %d", maxmsg->maxMessageSize);
					options.push_back(maxmsg);
					break;
				}
				case 58:
				{
					Console.Error("Got Renewal (T1) Time");
					DHCPopT1* t1 = new DHCPopT1(buffer, offset);
					Console.Error("Entry        %d", t1->ipRenewalTimeT1);
					options.push_back(t1);
					break;
				}
				case 59:
				{
					Console.Error("Got Rebinding (T2) Time");
					DHCPopT2* t2 = new DHCPopT2(buffer, offset);
					Console.Error("Entry        %d", t2->ipRebindingTimeT2);
					options.push_back(t2);
					break;
				}
				case 60:
				{
					Console.Error("Got ClassID");
					DHCPopClassID* classID = new DHCPopClassID(buffer, offset);
					Console.Error("Entry        %s", classID->classID.c_str());
					options.push_back(classID);
					break;
				}
				case 61:
				{
					Console.Error("Got Client ID");
					DHCPopClientID* clientID = new DHCPopClientID(buffer, offset);
					//Do we actully care about this?
					options.push_back(clientID);
					break;
				}
				default:
					Console.Error("Got Unknown Option %d with len %d", opKind, opLen);
					break;
			}
			offset += opLen + 2;
			if (offset >= bufferSize)
			{
				Console.Error("Unexpected end of packet");
				options.push_back(new DHCPopNOP());
				opReadFin = true;
			}
		} while (opReadFin == false);
	}

	int DHCP_Packet::GetLength()
	{
		//int len = 240;
		//for (size_t i = 0; i < options.size(); i++)
		//{
		//	//TODO, handle
		//	if (len + options[i]->GetLength() < maxLenth - (8+20))
		//		len += options[i]->GetLength();
		//	else
		//	{
		//		len = maxLenth - (8 + 20);
		//		break;
		//	}
		//}

		//return len;
		return maxLenth - (8 + 20);
	}

	void DHCP_Packet::WriteBytes(u8* buffer, int* offset)
	{
		int start = *offset;
		NetLib::WriteByte08(buffer, offset, op);
		NetLib::WriteByte08(buffer, offset, hardwareType);
		NetLib::WriteByte08(buffer, offset, hardwareAddressLength);
		NetLib::WriteByte08(buffer, offset, hops);

		NetLib::WriteUInt32(buffer, offset, transactionID);

		NetLib::WriteUInt16(buffer, offset, seconds);
		NetLib::WriteUInt16(buffer, offset, flags);

		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&clientIP);
		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&yourIP);
		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&serverIP);
		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&gatewayIP);

		NetLib::WriteByteArray(buffer, offset, 16, clientHardwareAddress);
		//empty bytes
		memset(buffer + *offset, 0, 64 + 128);
		*offset += 64 + 128;

		NetLib::WriteUInt32(buffer, offset, magicCookie);

		int len = 240;
		for (size_t i = 0; i < options.size(); i++)
		{
			if (len + options[i]->GetLength() < maxLenth)
			{
				len += options[i]->GetLength();
				options[i]->WriteBytes(buffer, offset);
			}
			else
			{
				Console.Error("Oversized DHCP packet not handled");
				//We need space for DHCP End
				if (len == maxLenth)
				{
					i -= 1;
					int pastLength = options[i]->GetLength();
					len -= pastLength;
					*offset -= pastLength;
				}

				DHCPopEND end;
				end.WriteBytes(buffer, offset);
				break;
			}
		}

		int end = start + GetLength();
		int delta = end - *offset;

		memset(&buffer[*offset], 0, delta);
		*offset = start + GetLength();
	}

	DHCP_Packet::~DHCP_Packet()
	{
		for (size_t i = 0; i < options.size(); i++)
			delete options[i];
	}
} // namespace PacketReader::IP::UDP::DHCP
