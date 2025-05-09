// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DHCP_Options.h"
#include "DEV9/PacketReader/NetLib.h"

#include "common/Console.h"

namespace PacketReader::IP::UDP::DHCP
{
	DHCPopSubnet::DHCPopSubnet(IP_Address mask)
		: subnetMask{mask}
	{
	}
	DHCPopSubnet::DHCPopSubnet(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadIPAddress(data, &offset, &subnetMask);
	}
	void DHCPopSubnet::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);
		NetLib::WriteIPAddress(buffer, offset, subnetMask);
	}

	DHCPopRouter::DHCPopRouter(const std::vector<IP_Address>& routerIPs)
		: routers{routerIPs}
	{
	}
	DHCPopRouter::DHCPopRouter(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		routers = {(IP_Address*)&data[offset], (IP_Address*)&data[offset + len]};
		//offset += len;
	}
	void DHCPopRouter::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, routers.size() * 4, (u8*)&routers[0]);
	}

	DHCPopDNS::DHCPopDNS(const std::vector<IP_Address>& dnsIPs)
		: dnsServers{dnsIPs}
	{
	}
	DHCPopDNS::DHCPopDNS(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		dnsServers = {(IP_Address*)&data[offset], (IP_Address*)&data[offset + len]};
		//offset += len;
	}
	void DHCPopDNS::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, dnsServers.size() * 4, (u8*)&dnsServers[0]);
	}

	DHCPopHostName::DHCPopHostName(const std::string& name)
	{
		if (name.size() > 255)
		{
			Console.Error("DEV9: DHCPopHostName: Name too long");
			hostName = name.substr(0, 255);
		}
		else
			hostName = name;
	}
	DHCPopHostName::DHCPopHostName(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		hostName = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopHostName::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, hostName.size(), (u8*)hostName.c_str());
	}

	DHCPopDnsName::DHCPopDnsName(const std::string& name)
	{
		if (name.size() > 255)
		{
			Console.Error("DEV9: DHCPopDnsName: Name too long");
			domainName = name.substr(0, 255);
		}
		else
			domainName = name;
	}
	DHCPopDnsName::DHCPopDnsName(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		domainName = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopDnsName::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, domainName.size(), (u8*)domainName.c_str());
	}

	DHCPopBCIP::DHCPopBCIP(IP_Address data)
		: broadcastIP{data}
	{
	}
	DHCPopBCIP::DHCPopBCIP(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadIPAddress(data, &offset, &broadcastIP);
	}
	void DHCPopBCIP::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteIPAddress(buffer, offset, broadcastIP);
	}

	bool DHCPopNBIOSType::GetHNode() const
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
	bool DHCPopNBIOSType::GetMNode() const
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
	bool DHCPopNBIOSType::GetPNode() const
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
	bool DHCPopNBIOSType::GetBNode() const
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
	DHCPopNBIOSType::DHCPopNBIOSType(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByte08(data, &offset, &type);
	}
	void DHCPopNBIOSType::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByte08(buffer, offset, type);
	}

	DHCPopREQIP::DHCPopREQIP(IP_Address data)
		: requestedIP{data}
	{
	}
	DHCPopREQIP::DHCPopREQIP(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadIPAddress(data, &offset, &requestedIP);
	}
	void DHCPopREQIP::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteIPAddress(buffer, offset, requestedIP);
	}

	DHCPopIPLT::DHCPopIPLT(u32 LeaseTime)
		: ipLeaseTime{LeaseTime}
	{
	}
	DHCPopIPLT::DHCPopIPLT(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipLeaseTime);
	}
	void DHCPopIPLT::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipLeaseTime);
	}

	DHCPopMSG::DHCPopMSG(u8 msg)
		: message{msg}
	{
	}
	DHCPopMSG::DHCPopMSG(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadByte08(data, &offset, &message);
	}
	void DHCPopMSG::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByte08(buffer, offset, message);
	}

	DHCPopSERVIP::DHCPopSERVIP(IP_Address data)
		: serverIP{data}
	{
	}
	DHCPopSERVIP::DHCPopSERVIP(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadIPAddress(data, &offset, &serverIP);
	}
	void DHCPopSERVIP::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteIPAddress(buffer, offset, serverIP);
	}

	DHCPopREQLIST::DHCPopREQLIST(const std::vector<u8>& requestList)
		: requests{requestList}
	{
	}
	DHCPopREQLIST::DHCPopREQLIST(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		requests = {&data[offset], &data[offset + len]};
		//offset += len;
	}
	void DHCPopREQLIST::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, requests.size(), &requests[0]);
	}

	DHCPopMSGStr::DHCPopMSGStr(const std::string& msg)
	{
		if (msg.size() > 255)
		{
			Console.Error("DEV9: DHCPopMSGStr: String too long");
			message = msg.substr(0, 255);
		}
		else
			message = msg;
	}
	DHCPopMSGStr::DHCPopMSGStr(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		message = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopMSGStr::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, message.size(), (u8*)message.c_str());
	}

	DHCPopMMSGS::DHCPopMMSGS(u16 mms)
		: maxMessageSize{mms}
	{
	}
	DHCPopMMSGS::DHCPopMMSGS(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt16(data, &offset, &maxMessageSize);
	}
	void DHCPopMMSGS::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt16(buffer, offset, maxMessageSize);
	}

	DHCPopT1::DHCPopT1(u32 t1)
		: ipRenewalTimeT1{t1}
	{
	}
	DHCPopT1::DHCPopT1(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipRenewalTimeT1);
	}
	void DHCPopT1::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipRenewalTimeT1);
	}

	DHCPopT2::DHCPopT2(u32 t2)
		: ipRebindingTimeT2{t2}
	{
	}
	DHCPopT2::DHCPopT2(const u8* data, int offset)
	{
		offset += 2;
		NetLib::ReadUInt32(data, &offset, &ipRebindingTimeT2);
	}
	void DHCPopT2::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteUInt32(buffer, offset, ipRebindingTimeT2);
	}

	DHCPopClassID::DHCPopClassID(const std::string& id)
	{
		if (id.size() > 255)
		{
			Console.Error("DEV9: DHCPopClassID: Class ID too long");
			classID = id.substr(0, 255);
		}
		else
			classID = id;
	}
	DHCPopClassID::DHCPopClassID(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);
		classID = std::string((char*)&data[offset], len);
		//offset += len;
	}
	void DHCPopClassID::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, classID.size(), (u8*)classID.c_str());
	}

	DHCPopClientID::DHCPopClientID(const std::vector<u8>& value)
		: clientID{value}
	{
	}
	DHCPopClientID::DHCPopClientID(const u8* data, int offset)
	{
		offset += 1;
		u8 len;
		NetLib::ReadByte08(data, &offset, &len);

		clientID = {&data[offset], &data[offset + len]};
		//offset += len;
	}
	void DHCPopClientID::WriteBytes(u8* buffer, int* offset) const
	{
		NetLib::WriteByte08(buffer, offset, GetCode());
		NetLib::WriteByte08(buffer, offset, GetLength() - 2);

		NetLib::WriteByteArray(buffer, offset, clientID.size(), &clientID[0]);
	}

} // namespace PacketReader::IP::UDP::DHCP
