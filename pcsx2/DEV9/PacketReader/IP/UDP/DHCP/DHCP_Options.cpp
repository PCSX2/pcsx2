/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "DHCP_Options.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::IP::UDP::DHCP
{
	DHCPopSubnet::DHCPopSubnet(IP_Address mask)
		: subnetMask{mask}
	{
	}
	DHCPopSubnet::DHCPopSubnet(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByteArray(data, &offset, 4, (u8*)&subnetMask);
	}
	void DHCPopSubnet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);
		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&subnetMask);
	}

	DHCPopRouter::DHCPopRouter(const std::vector<IP_Address>& routerIPs)
		: routers{routerIPs}
	{
	}
	DHCPopRouter::DHCPopRouter(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		routers = {(IP_Address*)&data[offset], (IP_Address*)&data[offset + len]};
		//offset += len;
	}
	void DHCPopRouter::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, routers.size() * 4, (u8*)&routers[0]);
	}

	DHCPopDNS::DHCPopDNS(const std::vector<IP_Address>& dnsIPs)
		: dnsServers{dnsIPs}
	{
	}
	DHCPopDNS::DHCPopDNS(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		dnsServers = {(IP_Address*)&data[offset], (IP_Address*)&data[offset + len]};
		//offset += len;
	}
	void DHCPopDNS::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, dnsServers.size() * 4, (u8*)&dnsServers[0]);
	}

	DHCPopHostName::DHCPopHostName(std::string name)
	{
		if (name.size() > 255)
		{
			Console.Error("DEV9: DHCPopHostName: Name too long");
			hostName = name.substr(0, 255);
		}
		else
			hostName = name;
	}
	DHCPopHostName::DHCPopHostName(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		hostName = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopHostName::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, hostName.size(), (u8*)hostName.c_str());
	}

	DHCPopDnsName::DHCPopDnsName(std::string name)
	{
		if (name.size() > 255)
		{
			Console.Error("DEV9: DHCPopDnsName: Name too long");
			domainName = name.substr(0, 255);
		}
		else
			domainName = name;
	}
	DHCPopDnsName::DHCPopDnsName(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		domainName = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopDnsName::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, domainName.size(), (u8*)domainName.c_str());
	}

	DHCPopBCIP::DHCPopBCIP(IP_Address data)
		: broadcastIP{data}
	{
	}
	DHCPopBCIP::DHCPopBCIP(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByteArray(data, &offset, 4, (u8*)&broadcastIP);
	}
	void DHCPopBCIP::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&broadcastIP);
	}

	bool DHCPopNBIOSType::GetHNode()
	{
		return ((type & (1 << 3)) != 0);
	}
	void DHCPopNBIOSType::SetHNode(bool value)
	{
		if (value)
		{
			type |= (1 << 3);
		}
		else
		{
			type &= ~(1 << 3);
		}
	}
	bool DHCPopNBIOSType::GetMNode()
	{
		return ((type & (1 << 2)) != 0);
	}
	void DHCPopNBIOSType::SetMNode(bool value)
	{
		if (value)
		{
			type |= (1 << 2);
		}
		else
		{
			type &= ~(1 << 2);
		}
	}
	bool DHCPopNBIOSType::GetPNode()
	{
		return ((type & (1 << 1)) != 0);
	}
	void DHCPopNBIOSType::SetPNode(bool value)
	{
		if (value)
		{
			type |= (1 << 1);
		}
		else
		{
			type &= ~(1 << 1);
		}
	}
	bool DHCPopNBIOSType::GetBNode()
	{
		return ((type & 1) != 0);
	}
	void DHCPopNBIOSType::SetBNode(bool value)
	{
		if (value)
		{
			type |= 1;
		}
		else
		{
			type &= ~1;
		}
	}
	//
	DHCPopNBIOSType::DHCPopNBIOSType(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByte08(data, &offset, &type);
	}
	void DHCPopNBIOSType::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByte08(buffer, offset, type);
	}

	DHCPopREQIP::DHCPopREQIP(IP_Address data)
		: requestedIP{data}
	{
	}
	DHCPopREQIP::DHCPopREQIP(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByteArray(data, &offset, 4, (u8*)&requestedIP);
	}
	void DHCPopREQIP::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&requestedIP);
	}

	DHCPopIPLT::DHCPopIPLT(u32 LeaseTime)
		: ipLeaseTime{LeaseTime}
	{
	}
	DHCPopIPLT::DHCPopIPLT(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipLeaseTime);
	}
	void DHCPopIPLT::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipLeaseTime);
	}

	DHCPopMSG::DHCPopMSG(u8 msg)
		: message{msg}
	{
	}
	DHCPopMSG::DHCPopMSG(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByte08(data, &offset, &message);
	}
	void DHCPopMSG::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByte08(buffer, offset, message);
	}

	DHCPopSERVIP::DHCPopSERVIP(IP_Address data)
		: serverIP{data}
	{
	}
	DHCPopSERVIP::DHCPopSERVIP(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByteArray(data, &offset, 4, (u8*)&serverIP);
	}
	void DHCPopSERVIP::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, 4, (u8*)&serverIP);
	}

	DHCPopREQLIST::DHCPopREQLIST(const std::vector<u8>& requestList)
		: requests{requestList}
	{
	}
	DHCPopREQLIST::DHCPopREQLIST(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		requests = {&data[offset], &data[offset + len]};
		//offset += len;
	}
	void DHCPopREQLIST::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, requests.size(), &requests[0]);
	}

	DHCPopMSGStr::DHCPopMSGStr(std::string msg)
	{
		if (msg.size() > 255)
		{
			Console.Error("DEV9: DHCPopMSGStr: String too long");
			message = msg.substr(0, 255);
		}
		else
			message = msg;
	}
	DHCPopMSGStr::DHCPopMSGStr(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		message = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopMSGStr::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, message.size(), (u8*)message.c_str());
	}

	DHCPopMMSGS::DHCPopMMSGS(u16 mms)
		: maxMessageSize{mms}
	{
	}
	DHCPopMMSGS::DHCPopMMSGS(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt16(data, &offset, &maxMessageSize);
	}
	void DHCPopMMSGS::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt16(buffer, offset, maxMessageSize);
	}

	DHCPopT1::DHCPopT1(u32 t1)
		: ipRenewalTimeT1{t1}
	{
	}
	DHCPopT1::DHCPopT1(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipRenewalTimeT1);
	}
	void DHCPopT1::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipRenewalTimeT1);
	}

	DHCPopT2::DHCPopT2(u32 t2)
		: ipRebindingTimeT2{t2}
	{
	}
	DHCPopT2::DHCPopT2(u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipRebindingTimeT2);
	}
	void DHCPopT2::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipRebindingTimeT2);
	}

	DHCPopClassID::DHCPopClassID(std::string id)
	{
		if (id.size() > 255)
		{
			Console.Error("DEV9: DHCPopClassID: Class ID too long");
			classID = id.substr(0, 255);
		}
		else
			classID = id;
	}
	DHCPopClassID::DHCPopClassID(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		classID = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopClassID::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, classID.size(), (u8*)classID.c_str());
	}

	DHCPopClientID::DHCPopClientID(const std::vector<u8>& value)
		: clientID{value}
	{
	}
	DHCPopClientID::DHCPopClientID(u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		clientID = {&data[offset], &data[offset + len]};
		//offset += len;
	}
	void DHCPopClientID::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, clientID.size(), &clientID[0]);
	}

} // namespace PacketReader::IP::UDP::DHCP
