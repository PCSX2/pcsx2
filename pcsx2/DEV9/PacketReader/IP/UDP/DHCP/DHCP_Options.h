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
		virtual u8 GetLength() { return 1; }
		virtual u8 GetCode() { return 0; }

		virtual void WriteBytes(u8* buffer, int* offset)
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
		IP_Address subnetMask{0};

		DHCPopSubnet(IP_Address mask);
		DHCPopSubnet(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 1; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopRouter(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + 4 * routers.size(); }
		virtual u8 GetCode() { return 3; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopDNS(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + 4 * dnsServers.size(); }
		virtual u8 GetCode() { return 6; }

		virtual void WriteBytes(u8* buffer, int* offset);

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

		DHCPopHostName(std::string name);
		DHCPopHostName(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + hostName.size(); }
		virtual u8 GetCode() { return 12; }

		virtual void WriteBytes(u8* buffer, int* offset);

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

		DHCPopDnsName(std::string name);
		DHCPopDnsName(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + domainName.size(); }
		virtual u8 GetCode() { return 15; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual DHCPopDnsName* Clone() const
		{
			return new DHCPopDnsName(*this);
		}
	};

	class DHCPopBCIP : public BaseOption //The IP to send broadcasts to
	{
	public:
		IP_Address broadcastIP{0};

		DHCPopBCIP(IP_Address data);
		DHCPopBCIP(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 28; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		bool GetHNode();
		void SetHNode(bool value);

		bool GetMNode();
		void SetMNode(bool value);

		bool GetPNode();
		void SetPNode(bool value);

		bool GetBNode();
		void SetBNode(bool value);

		DHCPopNBIOSType(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 3; }
		virtual u8 GetCode() { return 46; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual DHCPopNBIOSType* Clone() const
		{
			return new DHCPopNBIOSType(*this);
		}
	};

	class DHCPopREQIP : public BaseOption //The IP to send broadcasts to
	{
	public:
		IP_Address requestedIP{0};

		DHCPopREQIP(IP_Address data);
		DHCPopREQIP(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 50; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopIPLT(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 51; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopMSG(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 3; }
		virtual u8 GetCode() { return 53; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual DHCPopMSG* Clone() const
		{
			return new DHCPopMSG(*this);
		}
	};

	class DHCPopSERVIP : public BaseOption //DHCP server ip
	{
	public:
		IP_Address serverIP{0};

		DHCPopSERVIP(IP_Address data);
		DHCPopSERVIP(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 54; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopREQLIST(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + requests.size(); }
		virtual u8 GetCode() { return 55; }

		virtual void WriteBytes(u8* buffer, int* offset);

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

		DHCPopMSGStr(std::string msg);
		DHCPopMSGStr(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + message.size(); }
		virtual u8 GetCode() { return 56; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopMMSGS(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 4; }
		virtual u8 GetCode() { return 57; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopT1(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 58; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopT2(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 6; }
		virtual u8 GetCode() { return 59; }

		virtual void WriteBytes(u8* buffer, int* offset);

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

		DHCPopClassID(std::string id);
		DHCPopClassID(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + classID.size(); }
		virtual u8 GetCode() { return 60; }

		virtual void WriteBytes(u8* buffer, int* offset);

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
		DHCPopClientID(u8* data, int offset); //Offset will include Kind and Len

		virtual u8 GetLength() { return 2 + clientID.size(); }
		virtual u8 GetCode() { return 61; }

		virtual void WriteBytes(u8* buffer, int* offset);

		virtual DHCPopClientID* Clone() const
		{
			return new DHCPopClientID(*this);
		}
	};

	class DHCPopEND : public BaseOption
	{
	public:
		DHCPopEND() {}

		virtual u8 GetLength() { return 1; }
		virtual u8 GetCode() { return 255; }

		virtual void WriteBytes(u8* buffer, int* offset)
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
