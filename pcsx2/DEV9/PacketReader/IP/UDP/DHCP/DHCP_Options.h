// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <string>
#include <vector>

#include "DEV9/PacketReader/IP/IP_Packet.h"
#include "DEV9/PacketReader/IP/IP_Options.h"

namespace PacketReader::IP::UDP::DHCP
{
	//Unlike IPOptions, DCHP length field does not count the option header
	//GetLength(), howver, includes the option header
	class DHCPopNOP : public BaseOption
	{
		virtual u8 GetLength() const { return 1; }
		virtual u8 GetCode() const { return 0; }

		virtual void WriteBytes(u8* buffer, int* offset) const
		{
			buffer[*offset] = GetCode();
			(*offset)++;
		}

		virtual DHCPopNOP* Clone() const
		{
			return new DHCPopNOP(*this);
		}
	};

	class DHCPopSubnet : public BaseOption
	{
	public:
		IP_Address subnetMask{};

		DHCPopSubnet(IP_Address mask);
		DHCPopSubnet(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopSubnet* Clone() const
		{
			return new DHCPopSubnet(*this);
		}
	};

	class DHCPopRouter : public BaseOption //can be longer then 1 address
	{
	public:
		std::vector<IP_Address> routers;
		DHCPopRouter(const std::vector<IP_Address>& routerIPs);
		DHCPopRouter(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + 4 * routers.size(); }
		virtual u8 GetCode() const { return 3; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopRouter* Clone() const
		{
			return new DHCPopRouter(*this);
		}
	};

	class DHCPopDNS : public BaseOption //can be longer then 1 address
	{
	public:
		std::vector<IP_Address> dnsServers;
		DHCPopDNS(const std::vector<IP_Address>& dnsIPs);
		DHCPopDNS(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + 4 * dnsServers.size(); }
		virtual u8 GetCode() const { return 6; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopDNS* Clone() const
		{
			return new DHCPopDNS(*this);
		}
	};

	class DHCPopHostName : public BaseOption
	{
	public:
		//ASCII encoding
		std::string hostName;

		DHCPopHostName(const std::string& name);
		DHCPopHostName(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + hostName.size(); }
		virtual u8 GetCode() const { return 12; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopHostName* Clone() const
		{
			return new DHCPopHostName(*this);
		}
	};

	class DHCPopDnsName : public BaseOption
	{
	public:
		//ASCII encoding
		std::string domainName;

		DHCPopDnsName(const std::string& name);
		DHCPopDnsName(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + domainName.size(); }
		virtual u8 GetCode() const { return 15; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopDnsName* Clone() const
		{
			return new DHCPopDnsName(*this);
		}
	};

	class DHCPopBCIP : public BaseOption //The IP to send broadcasts to
	{
	public:
		IP_Address broadcastIP{};

		DHCPopBCIP(IP_Address data);
		DHCPopBCIP(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 28; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopBCIP* Clone() const
		{
			return new DHCPopBCIP(*this);
		}
	};

	//What even sent this?
	class DHCPopNBIOSType : public BaseOption
	{
	private:
		u8 type = 0;

	public:
		//Getters/Setters
		bool GetHNode() const;
		void SetHNode(bool value);

		bool GetMNode() const;
		void SetMNode(bool value);

		bool GetPNode() const;
		void SetPNode(bool value);

		bool GetBNode() const;
		void SetBNode(bool value);

		DHCPopNBIOSType(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 3; }
		virtual u8 GetCode() const { return 46; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopNBIOSType* Clone() const
		{
			return new DHCPopNBIOSType(*this);
		}
	};

	class DHCPopREQIP : public BaseOption //The IP to send broadcasts to
	{
	public:
		IP_Address requestedIP{};

		DHCPopREQIP(IP_Address data);
		DHCPopREQIP(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 50; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopREQIP* Clone() const
		{
			return new DHCPopREQIP(*this);
		}
	};

	class DHCPopIPLT : public BaseOption
	{
	public:
		u32 ipLeaseTime;

		DHCPopIPLT(u32 LeaseTime);
		DHCPopIPLT(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 51; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopIPLT* Clone() const
		{
			return new DHCPopIPLT(*this);
		}
	};

	class DHCPopMSG : public BaseOption
	{
	public:
		u8 message;
		DHCPopMSG(u8 msg);
		DHCPopMSG(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 3; }
		virtual u8 GetCode() const { return 53; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopMSG* Clone() const
		{
			return new DHCPopMSG(*this);
		}
	};

	class DHCPopSERVIP : public BaseOption //DHCP server ip
	{
	public:
		IP_Address serverIP{};

		DHCPopSERVIP(IP_Address data);
		DHCPopSERVIP(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 54; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopSERVIP* Clone() const
		{
			return new DHCPopSERVIP(*this);
		}
	};

	class DHCPopREQLIST : public BaseOption
	{
	public:
		std::vector<u8> requests;

		DHCPopREQLIST(const std::vector<u8>& requestList);
		DHCPopREQLIST(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + requests.size(); }
		virtual u8 GetCode() const { return 55; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopREQLIST* Clone() const
		{
			return new DHCPopREQLIST(*this);
		}
	};

	class DHCPopMSGStr : public BaseOption
	{
	public:
		//ASCII encoding
		std::string message;

		DHCPopMSGStr(const std::string& msg);
		DHCPopMSGStr(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + message.size(); }
		virtual u8 GetCode() const { return 56; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopMSGStr* Clone() const
		{
			return new DHCPopMSGStr(*this);
		}
	};

	class DHCPopMMSGS : public BaseOption
	{
	public:
		u16 maxMessageSize;

		DHCPopMMSGS(u16 mms);
		DHCPopMMSGS(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 4; }
		virtual u8 GetCode() const { return 57; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopMMSGS* Clone() const
		{
			return new DHCPopMMSGS(*this);
		}
	};

	class DHCPopT1 : public BaseOption
	{
	public:
		u32 ipRenewalTimeT1;

		DHCPopT1(u32 t1);
		DHCPopT1(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 58; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopT1* Clone() const
		{
			return new DHCPopT1(*this);
		}
	};

	class DHCPopT2 : public BaseOption
	{
	public:
		u32 ipRebindingTimeT2;

		DHCPopT2(u32 t2);
		DHCPopT2(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 6; }
		virtual u8 GetCode() const { return 59; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopT2* Clone() const
		{
			return new DHCPopT2(*this);
		}
	};

	class DHCPopClassID : public BaseOption
	{
	public:
		//ASCII encoding
		std::string classID;

		DHCPopClassID(const std::string& id);
		DHCPopClassID(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + classID.size(); }
		virtual u8 GetCode() const { return 60; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopClassID* Clone() const
		{
			return new DHCPopClassID(*this);
		}
	};

	class DHCPopClientID final : public BaseOption
	{
	public:
		std::vector<u8> clientID;

		DHCPopClientID(const std::vector<u8>& value);
		DHCPopClientID(const u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() const { return 2 + clientID.size(); }
		virtual u8 GetCode() const { return 61; }

		virtual void WriteBytes(u8* buffer, int* offset) const;

		virtual DHCPopClientID* Clone() const
		{
			return new DHCPopClientID(*this);
		}
	};

	class DHCPopEND : public BaseOption
	{
	public:
		DHCPopEND() {}

		virtual u8 GetLength() const { return 1; }
		virtual u8 GetCode() const { return 255; }

		virtual void WriteBytes(u8* buffer, int* offset) const
		{
			buffer[*offset] = GetCode();
			(*offset)++;
		}

		virtual DHCPopEND* Clone() const
		{
			return new DHCPopEND(*this);
		}
	};
} // namespace PacketReader::IP::UDP::DHCP
