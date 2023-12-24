// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DHCP_Packet.h"
#include "DEV9/PacketReader/NetLib.h"

#include "common/Console.h"

namespace PacketReader::IP::UDP::DHCP
{
	DHCP_Packet::DHCP_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;
		//Bits 0-31 //Bytes 0-3
		NetLib::ReadByte08(buffer, &offset, &op);
		NetLib::ReadByte08(buffer, &offset, &hardwareType);
		NetLib::ReadByte08(buffer, &offset, &hardwareAddressLength);
		NetLib::ReadByte08(buffer, &offset, &hops);

		//Bits 32-63 //Bytes 4-7
		NetLib::ReadUInt32(buffer, &offset, &transactionID);

		//Bits 64-95 //Bytes 8-11
		NetLib::ReadUInt16(buffer, &offset, &seconds);
		NetLib::ReadUInt16(buffer, &offset, &flags);

		//Bits 96-127 //Bytes 12-15
		NetLib::ReadIPAddress(buffer, &offset, &clientIP);

		//Bits 128-159 //Bytes 16-19
		NetLib::ReadIPAddress(buffer, &offset, &yourIP);

		//Bits 160-191 //Bytes 20-23
		NetLib::ReadIPAddress(buffer, &offset, &serverIP);

		//Bits 192-223 //Bytes 24-27
		NetLib::ReadIPAddress(buffer, &offset, &gatewayIP);

		//Bits 192+ //Bytes 28-43
		NetLib::ReadByteArray(buffer, &offset, 16, clientHardwareAddress);

		//Bytes 44-235
		//Assume BOOTP unused
		offset += 192;

		//Bytes 236-239
		NetLib::ReadUInt32(buffer, &offset, &magicCookie);
		bool opReadFin = false;

		do
		{
			u8 opKind = buffer[offset];
			if (opKind == 255)
			{
				options.push_back(new DHCPopEND());
				opReadFin = true;
				offset += 1;
				continue;
			}
			if ((offset + 1) >= bufferSize)
			{
				Console.Error("DEV9: DHCP_Packet: Unexpected end of packet");
				options.push_back(new DHCPopEND());
				opReadFin = true;
				continue;
			}
			u8 opLen = buffer[offset + 1];
			switch (opKind)
			{
				case 0:
					options.push_back(new DHCPopNOP());
					offset += 1;
					continue;
				case 1:
					options.push_back(new DHCPopSubnet(buffer, offset));
					break;
				case 3:
					options.push_back(new DHCPopRouter(buffer, offset));
					break;
				case 6:
					options.push_back(new DHCPopDNS(buffer, offset));
					break;
				case 12:
					options.push_back(new DHCPopHostName(buffer, offset));
					break;
				case 15:
					options.push_back(new DHCPopDnsName(buffer, offset));
					break;
				case 28:
					options.push_back(new DHCPopBCIP(buffer, offset));
					break;
				case 46:
					//Do we actually care about this?
					options.push_back(new DHCPopNBIOSType(buffer, offset));
					break;
				case 50:
					options.push_back(new DHCPopREQIP(buffer, offset));
					break;
				case 51:
					options.push_back(new DHCPopIPLT(buffer, offset));
					break;
				case 53:
					options.push_back(new DHCPopMSG(buffer, offset));
					break;
				case 54:
					options.push_back(new DHCPopSERVIP(buffer, offset));
					break;
				case 55:
					options.push_back(new DHCPopREQLIST(buffer, offset));
					break;
				case 56:
					options.push_back(new DHCPopMSGStr(buffer, offset));
					break;
				case 57:
					options.push_back(new DHCPopMMSGS(buffer, offset));
					break;
				case 58:
					options.push_back(new DHCPopT1(buffer, offset));
					break;
				case 59:
					options.push_back(new DHCPopT2(buffer, offset));
					break;
				case 60:
					options.push_back(new DHCPopClassID(buffer, offset));
					break;
				case 61:
					//Do we actully care about this?
					options.push_back(new DHCPopClientID(buffer, offset));
					break;
				default:
					Console.Error("DEV9: DHCP_Packet: Got Unknown Option %d with len %d", opKind, opLen);
					break;
			}
			offset += opLen + 2;
			if (offset >= bufferSize)
			{
				Console.Error("DEV9: DHCP_Packet: Unexpected end of packet");
				options.push_back(new DHCPopNOP());
				opReadFin = true;
			}
		} while (opReadFin == false);
	}
	DHCP_Packet::DHCP_Packet(const DHCP_Packet& original)
		: op{original.op}
		, hardwareType(original.hardwareType)
		, hardwareAddressLength{original.hardwareAddressLength}
		, hops{original.hops}
		, transactionID{original.transactionID}
		, seconds{original.seconds}
		, flags{original.flags}
		, clientIP{original.clientIP}
		, yourIP{original.yourIP}
		, serverIP{original.serverIP}
		, gatewayIP{original.gatewayIP}
		, magicCookie(original.magicCookie)
		, maxLength{original.maxLength}
	{
		memcpy(clientHardwareAddress, original.clientHardwareAddress, 16);
		//Assume BOOTP unused

		//Clone options
		options.reserve(original.options.size());
		for (size_t i = 0; i < options.size(); i++)
			options.push_back(original.options[i]->Clone());
	}


	int DHCP_Packet::GetLength()
	{
		return maxLength - (8 + 20);
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

		NetLib::WriteIPAddress(buffer, offset, clientIP);
		NetLib::WriteIPAddress(buffer, offset, yourIP);
		NetLib::WriteIPAddress(buffer, offset, serverIP);
		NetLib::WriteIPAddress(buffer, offset, gatewayIP);

		NetLib::WriteByteArray(buffer, offset, 16, clientHardwareAddress);
		//empty bytes
		memset(buffer + *offset, 0, 64 + 128);
		*offset += 64 + 128;

		NetLib::WriteUInt32(buffer, offset, magicCookie);

		int len = 240;
		for (size_t i = 0; i < options.size(); i++)
		{
			if (len + options[i]->GetLength() < maxLength)
			{
				len += options[i]->GetLength();
				options[i]->WriteBytes(buffer, offset);
			}
			else
			{
				Console.Error("DEV9: DHCP_Packet: Oversized DHCP packet not handled");
				//We need space for DHCP End
				if (len == maxLength)
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

	DHCP_Packet* DHCP_Packet::Clone() const
	{
		return new DHCP_Packet(*this);
	}

	DHCP_Packet::~DHCP_Packet()
	{
		for (size_t i = 0; i < options.size(); i++)
			delete options[i];
	}
} // namespace PacketReader::IP::UDP::DHCP
